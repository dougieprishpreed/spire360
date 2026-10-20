#include "collection_screen.h"

#include "../scene/controls.h"
#include "../scene/scene.h"
#include "../scene/value.h"
#include "inspect_screens.h"
#include "top_bar.h"

namespace spire {

static Control* control_of(Node* n) { return n && n->is_control() ? (Control*)n : NULL; }

static const char* const LOCKED_ICON = "images/packed/common_ui/locked_model.png";

CollectionScreen::CollectionScreen(SceneLibrary* lib, Kind kind)
    : lib_(lib), kind_(kind), root_(NULL), screen_(NULL), contents_(NULL), row_(-1), col_(0), loaded_(false), inspect_(NULL),
      inspect_node_(NULL) {}

/* The inspect node is under root_, freed with it by the owner. */
CollectionScreen::~CollectionScreen() { delete inspect_; }

std::string CollectionScreen::query() const {
    return kind_ == RELICS ? "{\"cmd\":\"relic_collection\"}" : "{\"cmd\":\"potion_lab\"}";
}

bool CollectionScreen::is_answer(const std::string& type) { return type == "relic_collection" || type == "potion_lab"; }

Node* CollectionScreen::build(std::string* error) {
    const char* path = kind_ == RELICS ? "scenes/screens/relic_collection/relic_collection.tscn"
                                       : "scenes/screens/potion_lab/potion_lab.tscn";
    Node* screen = instance(lib_, path);
    if (!screen) {
        if (error) *error = std::string("no ") + path;
        return NULL;
    }
    screen_ = screen;
    Control* root = full_rect_control(kind_ == RELICS ? "RelicCollection" : "PotionLab");
    root_ = root;
    ColorRect* back = (ColorRect*)SceneLibrary::create_node("ColorRect");
    back->name = "Backstop";
    back->anchor[SIDE_RIGHT] = back->anchor[SIDE_BOTTOM] = 1;
    back->color = Color(0, 0, 0, 0.85f);
    root->add_child(back);
    root->add_child(screen);
    show_glyph(lib_, find_named(screen->find_unique("BackButton"), "HotkeyIcon"), "b");
    contents_ = control_of(screen->find_unique("ScreenContents"));
    scroll_.attach(contents_);
    /* OnSubmenuShown: the contents fade in once loaded */
    if (contents_) contents_->modulate.a = 0;
    /* NRelicCollectionCategory._Ready: the category icon hidden */
    static const char* const relic_nodes[7] = { "Starter", "Common", "Uncommon", "Rare", "Shop", "Ancient", "Event" };
    if (kind_ == RELICS)
        for (int i = 0; i < 7; ++i) hide(find_named(screen->find_unique(relic_nodes[i]), "Icon"));
    root->add_child(frame_driver(this));
    return root;
}

/* NRelicCollectionEntry._Ready / NLabPotionHolder._Ready */
Control* CollectionScreen::make_entry(const Json& item, Control** shown) {
    *shown = NULL;
    const std::string& vis = item["visibility"].str();
    const Json& o = item["outline"]; /* the pool's LabOutlineColor, alpha 0.66 */
    bool has_outline = o.size() >= 4;
    Color outline = has_outline ? Color(jv_float(o[(size_t)0]), jv_float(o[1]), jv_float(o[2]), jv_float(o[3])) : Color(0, 0, 0, 0);
    if (kind_ == RELICS) {
        Control* e = control_of(instance(lib_, "scenes/screens/relic_collection/relic_collection_entry.tscn"));
        if (!e) return NULL;
        Node* holder = e->get_node("RelicHolder");
        if (vis == "locked") {
            TextureRect* t = (TextureRect*)SceneLibrary::create_node("TextureRect");
            t->name = "Locked";
            t->expand_mode = TextureRect::EXPAND_IGNORE_SIZE;
            t->stretch_mode = TextureRect::STRETCH_KEEP_ASPECT;
            set_texture(lib_, t, LOCKED_ICON);
            place(t, 0, 0, 68, 68);
            t->pivot_offset = Vec2(34, 34);
            t->modulate = Color(0.5f, 0.5f, 0.5f, 1);
            t->mouse_filter = 2;
            if (holder) holder->add_child(t);
            *shown = t;
        } else {
            Control* r = control_of(instance(lib_, "scenes/relics/relic.tscn"));
            if (r) {
                for (int k = 0; k < 4; ++k) r->anchor[k] = 0;
                place(r, 0, 0, 68, 68);
                r->pivot_offset = Vec2(34, 34);
                r->mouse_filter = 2;
                Node* icon = r->find_unique("Icon");
                set_relic_icon(lib_, icon, relic_icon_base(item));
                Node* line = icon ? icon->find_unique("Outline") : NULL;
                if (!line) line = r->find_unique("Outline");
                set_visible(line, true);
                if (vis == "not_seen") {
                    if (icon && icon->is_canvas_item()) ((CanvasItem*)icon)->self_modulate = Color(0, 0, 0, 0.9f);
                    if (line && line->is_canvas_item()) ((CanvasItem*)line)->self_modulate = Color(1, 1, 1, 0.5f);
                } else if (has_outline && line && line->is_canvas_item()) {
                    ((CanvasItem*)line)->self_modulate = outline;
                }
                if (holder) holder->add_child(r);
                *shown = r;
            }
        }
        return e;
    }
    Control* e = control_of(instance(lib_, "scenes/screens/potion_lab/lab_potion_holder.tscn"));
    if (!e) return NULL;
    Node* holder = e->get_node("PotionHolder");
    Control* p = control_of(make_potion(lib_, item["id"].str()));
    if (p) {
        Node* image = p->find_unique("Image");
        Node* line = image ? image->find_unique("Outline") : NULL;
        if (vis == "locked") {
            set_texture(lib_, image, LOCKED_ICON);
            hide(line);
            p->modulate = Color(0.5f, 0.5f, 0.5f, 1);
        } else if (vis == "not_seen") {
            if (image && image->is_canvas_item()) ((CanvasItem*)image)->self_modulate = Color(0, 0, 0, 0.9f);
            if (line && line->is_canvas_item()) ((CanvasItem*)line)->modulate = Color(1, 1, 1, 0.5f);
        } else if (has_outline && line && line->is_canvas_item()) {
            ((CanvasItem*)line)->modulate = outline;
        }
        p->mouse_filter = 2;
        if (holder) holder->add_child(p);
        *shown = p;
    }
    return e;
}

/* A grid's entries, and its rows (GetGridItems: columns at a time). */
void CollectionScreen::fill_grid(Node* grid, const Json& items) {
    if (!grid) return;
    int columns = grid->type == "GridContainer" ? ((GridContainer*)grid)->columns : 10;
    if (columns < 1) columns = 1;
    for (size_t i = 0; i < items.size(); ++i) {
        Control* shown = NULL;
        Control* e = make_entry(items[i], &shown);
        if (!e) continue;
        grid->add_child(e);
        if (i % (size_t)columns == 0) rows_.push_back(std::vector<Entry>());
        Entry en = { e, shown ? shown : e, items_.size() };
        items_.push_back(&items[i]);
        rows_.back().push_back(en);
    }
}

void CollectionScreen::set_data(const Json& msg) {
    if (!screen_ || loaded_) return;
    data_ = msg;
    loaded_ = true;
    const Json& cats = data_["categories"];
    for (size_t c = 0; c < cats.size(); ++c) {
        const Json& cat = cats[c];
        Node* node = screen_->find_unique(cat["node"].str());
        if (!node) continue;
        set_label(kind_ == RELICS ? node->find_unique("Header") : node->get_node("Header"), cat["header"].str());
        const Json& groups = cat["groups"];
        if (!groups.size()) {
            fill_grid(node->find_unique(kind_ == RELICS ? "RelicsContainer" : "PotionsContainer"), cat["items"]);
            continue;
        }
        /* LoadRelics: the subcategories after the header row, in order */
        int at = 2;
        for (size_t g = 0; g < groups.size(); ++g) {
            const Json& grp = groups[g];
            Node* sub = instance(lib_, "scenes/screens/relic_collection/relic_collection_subcategory.tscn");
            if (!sub) continue;
            node->add_child(sub);
            std::vector<Node*>& ch = node->children;
            if (at < (int)ch.size() - 1) {
                ch.pop_back();
                ch.insert(ch.begin() + at, sub);
            }
            ++at;
            Node* header = sub->find_unique("Header");
            set_label(header, grp["header"].str());
            set_visible(header, grp["show_header"].b(true));
            set_visible(sub->get_node("Spacer"), grp["spacer"].b(true));
            Node* icon = sub->find_unique("Icon");
            if (grp["icon"].is_string()) {
                set_texture(lib_, icon, grp["icon"].str());
                set_visible(icon, true);
            } else {
                hide(icon);
            }
            fill_grid(sub->find_unique("RelicsContainer"), grp["items"]);
        }
    }
    scroll_.to_top();
    if (contents_) tweens_.tween_from(&contents_->modulate.a, 0.0f, 1.0f, kind_ == RELICS ? 0.4f : 0.25f);
    if (!rows_.empty()) focus(0, 0);
}

void CollectionScreen::focus(int row, int col) {
    if (rows_.empty()) return;
    if (row < 0) row = 0;
    if (row >= (int)rows_.size()) row = (int)rows_.size() - 1;
    if (col >= (int)rows_[(size_t)row].size()) col = (int)rows_[(size_t)row].size() - 1;
    if (col < 0) col = 0;
    if (row == row_ && col == col_) return;
    if (row_ >= 0 && row_ < (int)rows_.size() && col_ < (int)rows_[(size_t)row_].size()) {
        /* OnUnfocus */
        Control* old = rows_[(size_t)row_][(size_t)col_].shown;
        tweens_.tween(&old->scale, Vec2(1, 1), kind_ == RELICS ? 1.0f : 0.5f, TRANS_EXPO, EASE_OUT);
    }
    row_ = row;
    col_ = col;
    const Entry& e = rows_[(size_t)row][(size_t)col];
    /* OnFocus */
    if (kind_ == RELICS) {
        tweens_.kill(&e.shown->scale);
        e.shown->scale = Vec2(1.25f, 1.25f);
    } else {
        tweens_.tween(&e.shown->scale, Vec2(1.2f, 1.2f), 0.05f);
    }
    tips_.show_for_control(lib_, root_, e.node, (*items_[e.item])["tips"], kind_ == POTIONS ? 32.0f : 0.0f);
    scroll_.focus(e.node);
}

CollectionScreen::Action CollectionScreen::input(const PadState& pad, std::string* cmd) {
    cmd->clear();
    if (inspect_) {
        if (!inspect_->input(pad)) {
            if (inspect_node_->parent) inspect_node_->parent->remove_child(inspect_node_);
            delete inspect_node_;
            delete inspect_;
            inspect_node_ = NULL;
            inspect_ = NULL;
        }
        return NONE;
    }
    if (pad.pressed & PadState::B) return BACK;
    if ((pad.pressed & PadState::A) && kind_ == RELICS && row_ >= 0) {
        /* OnRelicEntryPressed: NInspectRelicScreen over the collection's relics */
        Json list = Json::make_array();
        for (size_t i = 0; i < items_.size(); ++i) list.append(*items_[i]);
        inspect_ = new InspectRelicScreen(lib_);
        std::string err;
        inspect_node_ = inspect_->build(list, (int)rows_[(size_t)row_][(size_t)col_].item, &err);
        if (inspect_node_) {
            tips_.hide();
            root_->add_child(inspect_node_);
        } else {
            delete inspect_;
            inspect_ = NULL;
        }
        return NONE;
    }
    if (rows_.empty() || row_ < 0) {
        if (pad.pressed & PadState::UP) scroll_.scroll_by(400);
        if (pad.pressed & PadState::DOWN) scroll_.scroll_by(-400);
        return NONE;
    }
    int n = (int)rows_[(size_t)row_].size();
    if (pad.pressed & PadState::LEFT) focus(row_, col_ > 0 ? col_ - 1 : n - 1);
    else if (pad.pressed & PadState::RIGHT) focus(row_, col_ + 1 < n ? col_ + 1 : 0);
    else if ((pad.pressed & PadState::UP) && row_ > 0) focus(row_ - 1, col_);
    else if ((pad.pressed & PadState::DOWN) && row_ + 1 < (int)rows_.size()) focus(row_ + 1, col_);
    return NONE;
}

void CollectionScreen::frame(double dt) {
    tweens_.update(dt);
    scroll_.frame(dt);
    tips_.frame();
}

} /* namespace spire */

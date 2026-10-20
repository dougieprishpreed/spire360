#include "card_library_screen.h"

#include "../scene/controls.h"
#include "../scene/scene.h"
#include "card_view.h"
#include "inspect_screens.h"
#include "loc.h"

#include <algorithm>
#include <math.h>
#include <stdlib.h>
#include <string.h>

namespace spire {

/* NCardGrid as the pile screens: the holders' small scale, padding; the
 * library's grid right of the sidebar (CardGrid offset_left 288). */
static const float CARD_W = 300.0f * 0.8f, CARD_H = 422.0f * 0.8f;
static const float PADDING = 40.0f, GRID_LEFT = 288.0f, GRID_TOP = 80.0f, BOTTOM_MARGIN = 320.0f;
static const float SMALL_SCALE = 0.8f, HOVER_SCALE = 1.0f;

static Control* control_of(Node* n) { return n && n->is_control() ? (Control*)n : NULL; }

CardLibraryScreen::CardLibraryScreen(SceneLibrary* lib)
    : lib_(lib), root_(NULL), upgrades_(false), rows_(NULL), focus_(-1), item_focus_(0), columns_(1), scroll_(0),
      scroll_to_(0), lim_lo_(0), lim_hi_(0), view_h_(1000), count_label_(NULL), no_results_(NULL), inspect_(NULL),
      inspect_node_(NULL) {}

/* The inspect node is under root_, freed with it by the owner. */
CardLibraryScreen::~CardLibraryScreen() { delete inspect_; }

std::string CardLibraryScreen::first_query() const {
    return format("{\"cmd\":\"library\",\"pool\":\"%s\"}", pool_.c_str());
}

void CardLibraryScreen::add(Node* screen, const char* unique, Kind kind, const std::string& key) {
    Item it;
    it.kind = kind;
    it.node = control_of(screen->find_unique(unique));
    it.key = key;
    it.on = false;
    it.reticle = it.node ? find_named(it.node, "SelectionReticle") : NULL;
    hide(it.reticle);
    if (it.node) items_.push_back(it);
}

Node* CardLibraryScreen::build(const std::string& character, std::string* error) {
    Node* screen = instance(lib_, "scenes/screens/card_library/card_library.tscn");
    if (!screen) {
        if (error) *error = "no card_library.tscn";
        return NULL;
    }
    character_ = character;
    pool_ = character.empty() ? "ironclad" : character; /* OnSubmenuOpened */
    Control* root = full_rect_control("CardLibrary");
    root_ = root;
    ColorRect* back = (ColorRect*)SceneLibrary::create_node("ColorRect");
    back->name = "Backstop";
    back->anchor[SIDE_RIGHT] = back->anchor[SIDE_BOTTOM] = 1;
    back->color = Color(0, 0, 0, 0.85f);
    root->add_child(back);
    root->add_child(screen);
    hide(screen->find_unique("SearchBar"));
    hide(screen->find_unique("MultiplayerCards"));
    hide(screen->find_unique("Stats"));
    show_glyph(lib_, find_named(screen->find_unique("BackButton"), "HotkeyIcon"), "b");
    Node* grid = screen->find_unique("CardGrid");
    hide(grid ? grid->get_node("Scrollbar") : NULL);
    Node* scroll = grid ? find_named(grid, "ScrollContainer") : NULL;
    rows_ = full_rect_control("Rows");
    if (scroll) scroll->add_child(rows_);
    else root->add_child(rows_);
    count_label_ = screen->find_unique("CardCountLabel");
    no_results_ = screen->find_unique("NoResultsLabel");
    set_label(no_results_, Loc::get("card_library", "NO_RESULTS"));
    hide(no_results_);

    /* the sidebar, top to bottom */
    static const char* const pools[][2] = { { "IroncladPool", "ironclad" }, { "SilentPool", "silent" },
                                            { "RegentPool", "regent" },     { "NecrobinderPool", "necrobinder" },
                                            { "DefectPool", "defect" },     { "ColorlessPool", "colorless" },
                                            { "AncientsPool", "ancients" }, { "MiscPool", "misc" } };
    for (size_t i = 0; i < 8; ++i) add(screen, pools[i][0], POOL, pools[i][1]);
    add(screen, "CardTypeSorter", SORTER, "1");
    add(screen, "AttackType", TICK, "type:Attack");
    add(screen, "SkillType", TICK, "type:Skill");
    add(screen, "PowerType", TICK, "type:Power");
    add(screen, "OtherType", TICK, "type:Other");
    add(screen, "RaritySorter", SORTER, "0");
    add(screen, "CommonRarity", TICK, "rarity:Common");
    add(screen, "UncommonRarity", TICK, "rarity:Uncommon");
    add(screen, "RareRarity", TICK, "rarity:Rare");
    add(screen, "OtherRarity", TICK, "rarity:Other");
    add(screen, "CostSorter", SORTER, "2");
    add(screen, "Cost0", TICK, "cost:0");
    add(screen, "Cost1", TICK, "cost:1");
    add(screen, "Cost2", TICK, "cost:2");
    add(screen, "Cost3+", TICK, "cost:3");
    add(screen, "CostX", TICK, "cost:X");
    add(screen, "AlphabetSorter", SORTER, "3");
    add(screen, "Upgrades", VIEW, "upgrades");
    static const char* const sorter_labels[4] = { "SORT_RARITY", "SORT_TYPE", "SORT_COST", "SORT_ALPHABET" };
    for (size_t i = 0; i < items_.size(); ++i) {
        Item& it = items_[i];
        if (it.kind == POOL) it.on = it.key == pool_;
        if (it.kind == SORTER) set_label(it.node->find_unique("Label"), Loc::get("gameplay_ui", sorter_labels[atoi(it.key.c_str())]));
        if (it.kind == TICK && it.key.compare(0, 7, "rarity:") == 0) {
            static const char* const rl[4][2] = { { "rarity:Common", "RARITY_COMMON" }, { "rarity:Uncommon", "RARITY_UNCOMMON" },
                                                  { "rarity:Rare", "RARITY_RARE" }, { "rarity:Other", "RARITY_OTHER" } };
            for (int k = 0; k < 4; ++k)
                if (it.key == rl[k][0]) set_label(it.node->get_node("Label"), Loc::get("card_library", rl[k][1]));
        }
        if (it.kind == VIEW) set_label(it.node->get_node("Label"), Loc::get("card_library", "VIEW_UPGRADES"));
        refresh_item(it);
    }
    /* NCardLibrary(): rarity, type, cost, alphabet */
    priority_.clear();
    for (int k = 0; k < 4; ++k) priority_.push_back(k * 2);
    root->add_child(frame_driver(this));
    return root;
}

/* The widgets' states: a pool's hsv and scale (NCardPoolFilter), a type or
 * cost tickbox's outline and hsv, a rarity / stat tickbox's visuals. */
void CardLibraryScreen::refresh_item(Item& it) {
    if (!it.node) return;
    if (it.kind == POOL) {
        Node* img = it.node->get_node("Image");
        if (img && img->is_canvas_item()) {
            CanvasItem* c = (CanvasItem*)img;
            c->shader_params["s"] = it.on ? 1.0f : 0.3f;
            c->shader_params["v"] = it.on ? 1.0f : 0.55f;
            if (img->is_control()) ((Control*)img)->scale = it.on ? Vec2(1.1f, 1.1f) : Vec2(0.95f, 0.95f);
        }
    } else if (it.kind == TICK) {
        Node* tv = it.node->find_unique("TickboxVisuals");
        if (tv) {
            set_visible(tv->get_node("Ticked"), it.on);
            set_visible(tv->get_node("NotTicked"), !it.on);
            Node* label = it.node->get_node("Label");
            if (label && label->is_canvas_item()) ((CanvasItem*)label)->self_modulate = it.on ? Color(1, 1, 1, 1) : Color(0.75f, 0.75f, 0.75f, 1);
        } else {
            set_visible(it.node->find_unique("Outline"), it.on);
            Node* img = it.node->find_unique("Image");
            if (img && img->is_canvas_item()) {
                ((CanvasItem*)img)->shader_params["s"] = it.on ? 1.0f : 0.65f;
                ((CanvasItem*)img)->shader_params["v"] = it.on ? 1.2f : 0.7f;
            }
        }
    } else if (it.kind == VIEW) {
        Node* tv = it.node->find_unique("TickboxVisuals");
        set_visible(tv ? tv->get_node("Ticked") : NULL, it.on);
        set_visible(tv ? tv->get_node("NotTicked") : NULL, !it.on);
    } else if (it.kind == SORTER) {
        Node* img = it.node->find_unique("Image");
        if (img && img->type == "TextureRect") ((TextureRect*)img)->flip_v = !it.on; /* SetFlipV(!_isDescending) */
    }
}

void CardLibraryScreen::set_cards(const Json& msg) {
    if (msg["pool"].str() != pool_) return;
    cards_ = msg["cards"];
    /* the characters' pools as unlocked */
    const Json& chars = msg["characters"];
    for (size_t i = 0; i < items_.size() && i < 5; ++i) {
        bool shown = false;
        for (size_t k = 0; k < chars.size(); ++k)
            if (chars[k].str() == items_[i].key) shown = true;
        set_visible(items_[i].node, shown);
    }
    focus_ = -1;
    rebuild();
    if (!order_.empty()) focus_card(0);
}

bool CardLibraryScreen::passes(const Json& c) const {
    /* a group with ticks lets through what one of them matches */
    static const char* const groups[3] = { "type:", "rarity:", "cost:" };
    for (int g = 0; g < 3; ++g) {
        bool any = false, match = false;
        for (size_t i = 0; i < items_.size(); ++i) {
            const Item& it = items_[i];
            if (it.kind != TICK || it.key.compare(0, strlen(groups[g]), groups[g]) != 0 || !it.on) continue;
            any = true;
            std::string v = it.key.substr(strlen(groups[g]));
            if (g == 0) {
                const std::string& t = c["type"].str();
                bool basic = t == "Attack" || t == "Skill" || t == "Power";
                if (v == "Other" ? !basic : t == v) match = true;
            } else if (g == 1) {
                const std::string& r = c["rarity"].str();
                bool basic = r == "Common" || r == "Uncommon" || r == "Rare";
                if (v == "Other" ? !basic : r == v) match = true;
            } else {
                int cost = c["canonical_cost"].i();
                bool x = c["costs_x"].b();
                if ((v == "0" && cost <= 0 && !x) || (v == "1" && cost == 1) || (v == "2" && cost == 2) ||
                    (v == "3" && cost >= 3) || (v == "X" && x))
                    match = true;
            }
        }
        if (any && !match) return false;
    }
    return true;
}

/* NCardGrid.SortingAlgorithms by the priorities, the id last; locked cards
 * after the others (CompareCardVisibility). */
int CardLibraryScreen::compare(const Json& a, const Json& b) const {
    bool la = a["visibility"].str() == "locked", lb = b["visibility"].str() == "locked";
    if (la != lb) return la ? 1 : -1;
    for (size_t k = 0; k < priority_.size(); ++k) {
        int sort = priority_[k] / 2, r = 0;
        if (sort == 0) r = a["rarity_rank"].i() - b["rarity_rank"].i();
        else if (sort == 1) r = a["type_rank"].i() - b["type_rank"].i();
        else if (sort == 2) r = a["cost"].i() - b["cost"].i();
        else {
            const std::string& x = a["title"].is_string() ? a["title"].str() : a["name"].str();
            const std::string& y = b["title"].is_string() ? b["title"].str() : b["name"].str();
            r = x < y ? -1 : (x > y ? 1 : 0);
        }
        if (priority_[k] & 1) r = -r;
        if (r != 0) return r;
    }
    const std::string& ia = a["id"].str();
    const std::string& ib = b["id"].str();
    return ia < ib ? -1 : (ia > ib ? 1 : 0);
}

void CardLibraryScreen::rebuild() {
    for (size_t i = 0; i < holders_.size(); ++i) {
        if (holders_[i]->parent) holders_[i]->parent->remove_child(holders_[i]);
        delete holders_[i];
    }
    holders_.clear();
    order_.clear();
    for (size_t i = 0; i < cards_.size(); ++i)
        if (passes(cards_[i])) order_.push_back((int)i);
    /* insertion sort (stable, a pool at most ~100) */
    for (size_t i = 1; i < order_.size(); ++i) {
        int v = order_[i];
        size_t j = i;
        while (j > 0 && compare(cards_[(size_t)order_[j - 1]], cards_[(size_t)v]) > 0) {
            order_[j] = order_[j - 1];
            --j;
        }
        order_[j] = v;
    }
    for (size_t i = 0; i < order_.size(); ++i) {
        const Json& c = cards_[(size_t)order_[i]];
        Node* h = instance(lib_, "scenes/cards/holders/grid_card_holder.tscn");
        if (!h || !h->is_control()) {
            delete h;
            h = SceneLibrary::create_node("Control");
        }
        Control* hc = (Control*)h;
        Node* card = make_card(lib_, upgrades_ && c["upgrade_preview"].is_object() ? c["upgrade_preview"] : c);
        if (card) hc->add_child(card);
        hc->scale = Vec2(SMALL_SCALE, SMALL_SCALE);
        rows_->add_child(hc);
        holders_.push_back(hc);
    }
    std::string count = Loc::get("card_library", "CARD_COUNT");
    size_t at = count.find("{Amount}");
    if (at != std::string::npos) count.replace(at, 8, format("%d", (int)order_.size()));
    set_label(count_label_, count);
    set_visible(no_results_, order_.empty());
    layout();
}

void CardLibraryScreen::layout() {
    float width = 1920.0f - GRID_LEFT - 120.0f;
    columns_ = (int)((width + PADDING) / (CARD_W + PADDING));
    if (columns_ < 1) columns_ = 1;
    int n = (int)holders_.size();
    int rows = (n + columns_ - 1) / columns_;
    float content_h = (float)rows * CARD_H + (float)(rows > 0 ? rows - 1 : 0) * PADDING;
    view_h_ = 1080.0f - GRID_TOP;
    float scroll_h = content_h + 80.0f + BOTTOM_MARGIN;
    lim_hi_ = 0;
    lim_lo_ = scroll_h > view_h_ ? view_h_ - scroll_h : 0;
    scroll_ = scroll_to_ = 0;
    for (int i = 0; i < n; ++i) {
        int r = i / columns_, c = i % columns_;
        /* in the CardGrid (already right of the sidebar) */
        place(holders_[(size_t)i], 60.0f + CARD_W * 0.5f + (float)c * (CARD_W + PADDING),
              80.0f + CARD_H * 0.5f + (float)r * (CARD_H + PADDING), 0, 0);
    }
}

void CardLibraryScreen::focus_card(int i) {
    int n = (int)holders_.size();
    if (focus_ >= 0 && focus_ < n) {
        Control* old = holders_[(size_t)focus_];
        old->z_index = 0;
        tweens_.tween(&old->scale, Vec2(SMALL_SCALE, SMALL_SCALE), 0.5f, TRANS_EXPO, EASE_OUT);
    }
    for (size_t k = 0; k < items_.size(); ++k) {
        hide(items_[k].reticle);
        if (items_[k].kind != POOL && items_[k].node) items_[k].node->scale = Vec2(1, 1);
    }
    if (n == 0) {
        focus_ = -1;
        tips_.hide();
        return;
    }
    if (i < 0) i = 0;
    if (i >= n) i = n - 1;
    focus_ = i;
    Control* h = holders_[(size_t)i];
    tweens_.kill(&h->scale);
    h->scale = Vec2(HOVER_SCALE, HOVER_SCALE);
    h->z_index = 1;
    const Json& c = cards_[(size_t)order_[(size_t)i]];
    if (c["visibility"].str() == "visible") tips_.show_for_card(lib_, root_, h, c["tips"]);
    else tips_.hide();
    float to = -h->offset[SIDE_TOP] + view_h_ * 0.5f;
    scroll_to_ = to < lim_lo_ ? lim_lo_ : (to > lim_hi_ ? lim_hi_ : to);
}

void CardLibraryScreen::focus_item(int i) {
    if (items_.empty()) return;
    int n = (int)items_.size();
    if (i < 0) i = 0;
    if (i >= n) i = n - 1;
    /* skip the hidden (locked characters' pools) */
    item_focus_ = i;
    if (focus_ >= 0 && focus_ < (int)holders_.size()) {
        Control* old = holders_[(size_t)focus_];
        old->z_index = 0;
        tweens_.tween(&old->scale, Vec2(SMALL_SCALE, SMALL_SCALE), 0.5f, TRANS_EXPO, EASE_OUT);
    }
    focus_ = -1;
    tips_.hide();
    for (size_t k = 0; k < items_.size(); ++k) {
        bool on = (int)k == i;
        if (items_[k].reticle) set_visible(items_[k].reticle, on);
        else if (items_[k].node && items_[k].kind != POOL) items_[k].node->scale = on ? Vec2(1.15f, 1.15f) : Vec2(1, 1);
        else if (items_[k].node && items_[k].kind == POOL && !items_[k].reticle) {}
    }
}

CardLibraryScreen::Action CardLibraryScreen::input(const PadState& pad, std::string* cmd) {
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
    if ((pad.pressed & PadState::A) && focus_ >= 0 && focus_ < (int)order_.size() &&
        cards_[(size_t)order_[(size_t)focus_]]["visibility"].str() == "visible") {
        /* ShowCardDetail: NInspectCardScreen over the shown, discovered cards */
        Json list = Json::make_array();
        int at = 0;
        for (size_t k = 0; k < order_.size(); ++k) {
            const Json& c = cards_[(size_t)order_[k]];
            if (c["visibility"].str() != "visible") continue;
            if ((int)k == focus_) at = (int)list.size();
            list.append(c);
        }
        inspect_ = new InspectCardScreen(lib_);
        std::string err;
        inspect_node_ = inspect_->build(list, at, upgrades_, &err);
        if (inspect_node_) {
            tips_.hide();
            root_->add_child(inspect_node_);
        } else {
            delete inspect_;
            inspect_ = NULL;
        }
        return NONE;
    }
    if (pad.pressed & PadState::Y) {
        /* View Upgrades */
        for (size_t k = 0; k < items_.size(); ++k)
            if (items_[k].kind == VIEW) {
                items_[k].on = !items_[k].on;
                upgrades_ = items_[k].on;
                refresh_item(items_[k]);
            }
        int f = focus_;
        rebuild();
        if (f >= 0) focus_card(f);
        return NONE;
    }
    if (focus_ >= 0) {
        int n = (int)holders_.size();
        int col = focus_ % columns_;
        if (pad.pressed & PadState::LEFT) {
            if (col > 0) focus_card(focus_ - 1);
            else focus_item(item_focus_);
        }
        if ((pad.pressed & PadState::RIGHT) && col + 1 < columns_ && focus_ + 1 < n) focus_card(focus_ + 1);
        if ((pad.pressed & PadState::UP) && focus_ - columns_ >= 0) focus_card(focus_ - columns_);
        if ((pad.pressed & PadState::DOWN) && focus_ / columns_ < (n - 1) / columns_)
            focus_card(focus_ + columns_ < n ? focus_ + columns_ : n - 1);
        return NONE;
    }
    /* the sidebar */
    int n = (int)items_.size();
    if (pad.pressed & (PadState::UP | PadState::DOWN)) {
        int step = (pad.pressed & PadState::UP) ? -1 : 1;
        for (int k = item_focus_ + step; k >= 0 && k < n; k += step)
            if (items_[(size_t)k].node && items_[(size_t)k].node->is_visible_in_tree()) {
                focus_item(k);
                break;
            }
    }
    if ((pad.pressed & PadState::RIGHT) && !holders_.empty()) {
        for (size_t k = 0; k < items_.size(); ++k) {
            hide(items_[k].reticle);
            if (items_[k].kind != POOL && items_[k].node) items_[k].node->scale = Vec2(1, 1);
        }
        focus_card(0);
        return NONE;
    }
    if (pad.pressed & PadState::A) {
        Item& it = items_[(size_t)item_focus_];
        if (it.kind == POOL) {
            if (it.key != pool_) {
                pool_ = it.key;
                for (size_t k = 0; k < items_.size(); ++k)
                    if (items_[k].kind == POOL) {
                        items_[k].on = items_[k].key == pool_;
                        refresh_item(items_[k]);
                    }
                *cmd = first_query();
            }
        } else if (it.kind == TICK) {
            it.on = !it.on;
            refresh_item(it);
            rebuild();
        } else if (it.kind == SORTER) {
            /* OnRelease: the direction turns; this sort leads */
            it.on = !it.on;
            refresh_item(it);
            int sort = atoi(it.key.c_str());
            for (size_t k = 0; k < priority_.size(); ++k)
                if (priority_[k] / 2 == sort) {
                    priority_.erase(priority_.begin() + (long)k);
                    break;
                }
            priority_.insert(priority_.begin(), sort * 2 + (it.on ? 1 : 0));
            rebuild();
        } else if (it.kind == VIEW) {
            it.on = !it.on;
            upgrades_ = it.on;
            refresh_item(it);
            rebuild();
        }
        focus_item(item_focus_);
    }
    return NONE;
}

void CardLibraryScreen::frame(double dt) {
    tweens_.update(dt);
    tips_.frame();
    if (!rows_) return;
    if (fabsf(scroll_ - scroll_to_) > 0.1f) {
        float k = (float)dt * 15.0f;
        scroll_ += (scroll_to_ - scroll_) * (k > 1 ? 1 : k);
        if (fabsf(scroll_ - scroll_to_) < 0.5f) scroll_ = scroll_to_;
    }
    rows_->offset[SIDE_TOP] = rows_->offset[SIDE_BOTTOM] = scroll_;
}

} /* namespace spire */

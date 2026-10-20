#include "stats_screen.h"

#include "../scene/controls.h"
#include "../scene/scene.h"
#include "../scene/value.h"

#include <math.h>

namespace spire {

static Control* control_of(Node* n) { return n && n->is_control() ? (Control*)n : NULL; }

static const Color CREAM(1.0f, 0.964706f, 0.886275f, 1.0f);

StatsScreen::StatsScreen(SceneLibrary* lib) : lib_(lib), root_(NULL), screen_(NULL), focus_(-1), tip_(NULL) {}

Node* StatsScreen::build(std::string* error) {
    Node* screen = instance(lib_, "scenes/screens/stats_screen/stats_screen.tscn");
    if (!screen) {
        if (error) *error = "no stats_screen.tscn";
        return NULL;
    }
    screen_ = screen;
    Control* root = full_rect_control("StatsScreen");
    root_ = root;
    ColorRect* back = (ColorRect*)SceneLibrary::create_node("ColorRect");
    back->name = "Backstop";
    back->anchor[SIDE_RIGHT] = back->anchor[SIDE_BOTTOM] = 1;
    back->color = Color(0, 0, 0, 0.85f);
    root->add_child(back);
    root->add_child(screen);
    show_glyph(lib_, find_named(screen->find_unique("BackButton"), "HotkeyIcon"), "b");
    /* NShareStatsButton: the platform's */
    hide(find_named(screen, "ShareButton"));
    /* NStatsTabManager.ResetTabs: Statistics selected */
    Node* tab = screen->find_unique("StatsTab");
    if (tab) {
        set_visible(tab->get_node("Outline"), true);
        Node* label = tab->get_node("Label");
        if (label && label->is_canvas_item()) ((CanvasItem*)label)->modulate = CREAM;
    }
    scroll_.attach(screen->find_unique("StatsGrid") ? screen->find_unique("StatsGrid")->get_node("ScrollableContent") : NULL);
    /* OnSubmenuOpened: the screen fades in over 0.4 s */
    if (screen->is_canvas_item()) tweens_.tween_from(&((CanvasItem*)screen)->modulate.a, 0.0f, 1.0f, 0.4f);
    root->add_child(frame_driver(this));
    return root;
}

/* NStatEntry: its icon, top and bottom texts (each shown once set). */
Control* StatsScreen::make_entry(Node* parent, const Json& e) {
    Control* n = control_of(instance(lib_, "scenes/screens/stats_screen/stats_screen_section.tscn"));
    if (!n || !parent) {
        delete n;
        return NULL;
    }
    set_texture(lib_, n->find_unique("Icon"), e["icon"].str());
    Node* top = n->find_unique("TopLabel");
    Node* bottom = n->find_unique("BottomLabel");
    if (e["top"].is_string()) {
        set_visible(top, true);
        set_label(top, e["top"].str());
    }
    if (e["bottom"].is_string()) {
        set_visible(bottom, true);
        set_label(bottom, e["bottom"].str());
    }
    hide(n->find_unique("SelectionReticle"));
    parent->add_child(n);
    Entry en = { n, e["tip"] };
    entries_.push_back(en);
    return n;
}

void StatsScreen::set_data(const Json& msg) {
    if (!screen_ || !entries_.empty()) return;
    data_ = msg;
    Node* tab = screen_->find_unique("StatsTab");
    if (tab) set_label(tab->get_node("Label"), data_["stats_tab"].str());
    Node* ach = screen_->find_unique("Achievements");
    if (ach) set_label(ach->get_node("Label"), data_["achievements_tab"].str());
    set_label(screen_->find_unique("OverallStatsHeader"), data_["overall_header"].str());
    set_label(screen_->find_unique("CharacterStatsHeader"), data_["character_header"].str());
    Node* grid = screen_->find_unique("GridContainer");
    const Json& overall = data_["overall"];
    for (size_t i = 0; i < overall.size(); ++i) make_entry(grid, overall[i]);
    Node* chars = screen_->find_unique("CharacterStatsContainer");
    const Json& cs = data_["characters"];
    for (size_t i = 0; chars && i < cs.size(); ++i) {
        Node* c = instance(lib_, "scenes/screens/stats_screen/character_stats.tscn");
        if (!c) continue;
        chars->add_child(c);
        Node* icon = instance(lib_, "scenes/ui/character_icons/" + cs[i]["id"].str() + "_icon.tscn");
        Node* holder = c->find_unique("CharacterIcon");
        if (icon && holder) holder->add_child(icon);
        else delete icon;
        Node* name = c->find_unique("NameLabel");
        set_label(name, cs[i]["name"].str());
        const Json& nc = cs[i]["name_color"];
        if (name && name->is_control() && nc.size() >= 4)
            ((Control*)name)->add_theme_color_override("font_color", Color(jv_float(nc[(size_t)0]), jv_float(nc[1]), jv_float(nc[2]),
                                                                              jv_float(nc[3])));
        hide(c->find_unique("UnlocksLabel"));
        const Json& es = cs[i]["entries"];
        for (size_t k = 0; k < es.size(); ++k) make_entry(c->find_unique("StatsContainer"), es[k]);
    }
    scroll_.to_top();
    if (!entries_.empty()) focus(0);
}

void StatsScreen::focus(int i) {
    if (i < 0 || i >= (int)entries_.size() || i == focus_) return;
    if (focus_ >= 0) {
        /* OnUnfocus */
        Control* old = entries_[(size_t)focus_].node;
        tweens_.tween(&old->scale, Vec2(1, 1), 0.5f, TRANS_EXPO, EASE_OUT);
        hide(old->find_unique("SelectionReticle"));
    }
    if (tip_) {
        if (tip_->parent) tip_->parent->remove_child(tip_);
        delete tip_;
        tip_ = NULL;
    }
    focus_ = i;
    Entry& e = entries_[(size_t)i];
    /* OnFocus: the pivot at (50, the middle) */
    e.node->pivot_offset = Vec2(50, e.node->size.y * 0.5f);
    tweens_.tween(&e.node->scale, Vec2(1.05f, 1.05f), 0.05f);
    set_visible(e.node->find_unique("SelectionReticle"), true);
    if (e.tip.is_object()) {
        tip_ = make_hover_tip(lib_, e.tip["title"].str(), e.tip["description"].str());
        if (tip_) {
            tip_->z_index = 10;
            tip_->modulate.a = 0; /* placed next frame */
            root_->add_child(tip_);
        }
    }
    scroll_.focus(e.node);
}

StatsScreen::Action StatsScreen::input(const PadState& pad, std::string* cmd) {
    cmd->clear();
    if (pad.pressed & PadState::B) return BACK;
    if (focus_ < 0) return NONE;
    Vec2 dir;
    if (pad.pressed & PadState::LEFT) dir = Vec2(-1, 0);
    else if (pad.pressed & PadState::RIGHT) dir = Vec2(1, 0);
    else if (pad.pressed & PadState::UP) dir = Vec2(0, -1);
    else if (pad.pressed & PadState::DOWN) dir = Vec2(0, 1);
    else return NONE;
    /* the nearest entry that way */
    Control* cur = entries_[(size_t)focus_].node;
    Vec2 from = cur->global_transform().origin + cur->size * 0.5f;
    int best = -1;
    float best_d = 1e30f;
    for (size_t k = 0; k < entries_.size(); ++k) {
        if ((int)k == focus_) continue;
        Control* c = entries_[k].node;
        Vec2 to = c->global_transform().origin + c->size * 0.5f;
        Vec2 d = to - from;
        float along = d.x * dir.x + d.y * dir.y;
        if (along <= 1.0f) continue;
        float across = fabsf(d.x * dir.y - d.y * dir.x);
        float score = along + across * 2.0f;
        if (score < best_d) {
            best_d = score;
            best = (int)k;
        }
    }
    if (best >= 0) focus(best);
    return NONE;
}

void StatsScreen::frame(double dt) {
    tweens_.update(dt);
    scroll_.frame(dt);
    if (tip_ && focus_ >= 0) {
        /* NStatEntry.OnFocus: 392 left of it in the left 40%, else 532 right */
        Control* e = entries_[(size_t)focus_].node;
        Vec2 at = e->global_transform().origin;
        float x = at.x < 1920.0f * 0.4f ? at.x - 392.0f : at.x + 532.0f;
        float y = at.y;
        if (y + tip_->size.y > 1080.0f) y = 1080.0f - tip_->size.y;
        tip_->set_position(Vec2(x, y));
        tip_->modulate.a = 1;
    }
}

} /* namespace spire */

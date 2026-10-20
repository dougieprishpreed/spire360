#include "inspect_screens.h"

#include "../scene/controls.h"
#include "../scene/scene.h"
#include "card_view.h"
#include "loc.h"
#include "top_bar.h"

namespace spire {

static Control* control_of(Node* n) { return n && n->is_control() ? (Control*)n : NULL; }

static void free_node(Node* n) {
    if (!n) return;
    if (n->parent) n->parent->remove_child(n);
    delete n;
}

/* A hover tip set at a rect's right edge (SetAlignment(rect, Right)). */
static Control* tips_at(SceneLibrary* lib, Node* parent, Node* rect, const Json& tips) {
    if (!parent || !tips.size()) return NULL;
    Control* t = make_tip_set(lib, tips);
    Control* r = control_of(rect);
    if (!t) return NULL;
    t->z_index = 10;
    for (int k = 0; k < 4; ++k) t->anchor[k] = r ? r->anchor[k] : 0;
    float x = r ? r->offset[SIDE_RIGHT] : 0, y = r ? r->offset[SIDE_TOP] : 0;
    t->offset[SIDE_LEFT] = t->offset[SIDE_RIGHT] = x;
    t->offset[SIDE_TOP] = t->offset[SIDE_BOTTOM] = y;
    parent->add_child(t);
    return t;
}

/* NButton arrows: slide in from 100 px further in, after 0.1 s. */
static void arrow_in(TweenSet& tw, Node* arrow, float dir) {
    Control* a = control_of(arrow);
    if (!a) return;
    a->modulate = Color(0, 0, 0, 0);
    tw.tween(&a->modulate.r, 1.0f, 0.25f, TRANS_LINEAR, EASE_IN_OUT, 0.1f);
    tw.tween(&a->modulate.g, 1.0f, 0.25f, TRANS_LINEAR, EASE_IN_OUT, 0.1f);
    tw.tween(&a->modulate.b, 1.0f, 0.25f, TRANS_LINEAR, EASE_IN_OUT, 0.1f);
    tw.tween(&a->modulate.a, 1.0f, 0.25f, TRANS_LINEAR, EASE_IN_OUT, 0.1f);
    float l = a->offset[SIDE_LEFT], r = a->offset[SIDE_RIGHT];
    tw.tween_from(&a->offset[SIDE_LEFT], l + dir * 100.0f, l, 0.25f, TRANS_BACK, EASE_OUT, 0.1f);
    tw.tween_from(&a->offset[SIDE_RIGHT], r + dir * 100.0f, r, 0.25f, TRANS_BACK, EASE_OUT, 0.1f);
}

/* ---- NInspectCardScreen ---- */

InspectCardScreen::InspectCardScreen(SceneLibrary* lib)
    : lib_(lib), screen_(NULL), holder_(NULL), card_(NULL), tips_(NULL), index_(0), ticked_(false), view_all_(false) {}

Node* InspectCardScreen::build(const Json& cards, int index, bool view_all_upgraded, std::string* error) {
    Node* screen = instance(lib_, "scenes/screens/inspect_card_screen.tscn");
    if (!screen || !screen->is_control()) {
        delete screen;
        if (error) *error = "no inspect_card_screen.tscn";
        return NULL;
    }
    screen_ = screen;
    cards_ = cards;
    view_all_ = view_all_upgraded;
    ((Control*)screen)->z_index = 20;
    /* the card: a holder at the scene card's place (centred, 2x) */
    hide(screen->get_node("Card"));
    holder_ = full_rect_control("CardHolder");
    for (int k = 0; k < 4; ++k) {
        holder_->anchor[k] = 0.5f;
        holder_->offset[k] = 0;
    }
    screen->add_child(holder_);
    set_label(screen->find_unique("ShowUpgradeLabel"), Loc::get("card_selection", "VIEW_UPGRADES"));
    show_glyph(lib_, screen->find_unique("HotkeyIcon"), "y");
    /* Open: the backstop to 0.9, the card in from 1.75x */
    Node* back = screen->get_node("Backstop");
    if (back && back->is_canvas_item()) tweens_.tween(&((CanvasItem*)back)->modulate.a, 0.9f, 0.25f);
    tweens_.tween_from(&((Control*)screen)->modulate.a, 0.0f, 1.0f, 0.25f, TRANS_EXPO, EASE_OUT);
    arrow_in(tweens_, screen->get_node("LeftArrow"), 1);
    arrow_in(tweens_, screen->get_node("RightArrow"), -1);
    holder_->scale = Vec2(1.75f, 1.75f);
    holder_->modulate.a = 0;
    tweens_.tween(&holder_->modulate.a, 1.0f, 0.25f);
    tweens_.tween(&holder_->scale, Vec2(2, 2), 0.15f, TRANS_SPRING, EASE_OUT, 0.1f);
    set_card(index, 0);
    screen->add_child(frame_driver(this));
    return screen;
}

/* SetCard: the arrows and the tickbox, ticked for an upgraded card (or all) */
void InspectCardScreen::set_card(int index, int slide) {
    int n = (int)cards_.size();
    if (!n) return;
    index_ = index < 0 ? 0 : (index >= n ? n - 1 : index);
    set_visible(screen_->get_node("LeftArrow"), index_ > 0);
    set_visible(screen_->get_node("RightArrow"), index_ < n - 1);
    const Json& c = cards_[(size_t)index_];
    const Json& in = c["inspect"];
    bool upgradable = in.is_object() ? in["upgradable"].b()
                                     : (c["upgrade_preview"].is_object() || c["inspect_upgrade"].is_object() || c["upgraded"].b());
    bool start = in.is_object() ? in["start_upgraded"].b() : c["upgraded"].b();
    set_visible(screen_->find_unique("Upgrade"), upgradable);
    ticked_ = start || view_all_;
    show();
    if (slide) {
        holder_->modulate.a = 1;
        tweens_.tween_from(&holder_->offset[SIDE_LEFT], slide * 100.0f, 0.0f, 0.25f, TRANS_EXPO, EASE_OUT);
        tweens_.tween_from(&holder_->offset[SIDE_RIGHT], slide * 100.0f, 0.0f, 0.25f, TRANS_EXPO, EASE_OUT);
    }
}

/* UpdateCardDisplay */
void InspectCardScreen::show() {
    const Json& c = cards_[(size_t)index_];
    const Json& in = c["inspect"];
    const Json& base = in.is_object() ? in["base"] : c;
    const Json& up = in.is_object() ? in["upgraded"] : c["upgrade_preview"].is_object() ? c["upgrade_preview"] : c["inspect_upgrade"];
    const Json& view = ticked_ && up.is_object() ? up : base;
    Node* tv = screen_->find_unique("TickboxVisuals");
    set_visible(tv ? tv->get_node("Ticked") : NULL, ticked_);
    set_visible(tv ? tv->get_node("NotTicked") : NULL, !ticked_);
    free_node(card_);
    card_ = make_card(lib_, view);
    if (card_) holder_->add_child(card_);
    free_node(tips_);
    tips_ = tips_at(lib_, screen_, screen_->get_node("HoverTipRect"), view["tips"]);
}

bool InspectCardScreen::input(const PadState& pad) {
    if (pad.pressed & PadState::B) return false;
    int n = (int)cards_.size();
    if ((pad.pressed & PadState::RIGHT) && index_ < n - 1) set_card(index_ + 1, 1);
    else if ((pad.pressed & PadState::LEFT) && index_ > 0) set_card(index_ - 1, -1);
    else if ((pad.pressed & PadState::Y) && screen_->find_unique("Upgrade") && screen_->find_unique("Upgrade")->is_canvas_item() &&
             ((CanvasItem*)screen_->find_unique("Upgrade"))->visible) {
        /* ToggleShowUpgrade */
        ticked_ = !ticked_;
        view_all_ = false;
        show();
    }
    return true;
}

void InspectCardScreen::frame(double dt) { tweens_.update(dt); }

/* ---- NInspectRelicScreen ---- */

InspectRelicScreen::InspectRelicScreen(SceneLibrary* lib)
    : lib_(lib), screen_(NULL), popup_(NULL), tips_(NULL), index_(0) {}

Node* InspectRelicScreen::build(const Json& relics, int index, std::string* error) {
    Node* screen = instance(lib_, "scenes/screens/inspect_relic_screen/inspect_relic_screen.tscn");
    if (!screen || !screen->is_control()) {
        delete screen;
        if (error) *error = "no inspect_relic_screen.tscn";
        return NULL;
    }
    screen_ = screen;
    relics_ = relics;
    ((Control*)screen)->z_index = 20;
    popup_ = control_of(screen->find_unique("Popup"));
    /* Open: the backstop to 0.9, the popup up from 200 px below */
    Node* back = screen->find_unique("Backstop");
    if (back && back->is_canvas_item()) {
        set_visible(back, true);
        ((CanvasItem*)back)->modulate.a = 0;
        tweens_.tween(&((CanvasItem*)back)->modulate.a, 0.9f, 0.25f);
    }
    if (popup_) {
        popup_->modulate = Color(0, 0, 0, 0);
        tweens_.tween(&popup_->modulate.r, 1.0f, 0.25f);
        tweens_.tween(&popup_->modulate.g, 1.0f, 0.25f);
        tweens_.tween(&popup_->modulate.b, 1.0f, 0.25f);
        tweens_.tween(&popup_->modulate.a, 1.0f, 0.25f);
        float t = popup_->offset[SIDE_TOP], b = popup_->offset[SIDE_BOTTOM];
        tweens_.tween_from(&popup_->offset[SIDE_TOP], t + 200, t, 0.25f, TRANS_CUBIC, EASE_OUT);
        tweens_.tween_from(&popup_->offset[SIDE_BOTTOM], b + 200, b, 0.25f, TRANS_CUBIC, EASE_OUT);
    }
    arrow_in(tweens_, screen->find_unique("LeftArrow"), 1);
    arrow_in(tweens_, screen->find_unique("RightArrow"), -1);
    set_relic(index, 0);
    screen->add_child(frame_driver(this));
    return screen;
}

/* SetRelic / UpdateRelicDisplay / SetRarityVisuals */
void InspectRelicScreen::set_relic(int index, int slide) {
    int n = (int)relics_.size();
    if (!n) return;
    index_ = index < 0 ? 0 : (index >= n ? n - 1 : index);
    set_visible(screen_->find_unique("LeftArrow"), index_ > 0);
    set_visible(screen_->find_unique("RightArrow"), index_ < n - 1);
    const Json& r0 = relics_[(size_t)index_];
    const Json& r = r0["inspect"].is_object() ? r0["inspect"] : r0;
    bool unlocked = r["unlocked"].b(r["visibility"].str() != "locked");
    bool seen = r["seen"].b(r["visibility"].str() == "visible");
    Node* name = screen_->find_unique("RelicName");
    Node* rarity = screen_->find_unique("Rarity");
    Node* image = screen_->find_unique("RelicImage");
    std::string rar = r["rarity"].str();
    std::string base = lower(relic_icon_base(r));
    std::string big = "images/relics/" + base + ".png";
    Resource* br = lib_->ext(big);
    if (!br || !br->is_texture()) big = "images/relics/beta/" + base + ".png";
    if (!unlocked) {
        set_label(name, Loc::get("inspect_relic_screen", "LOCKED_TITLE"));
        set_label(rarity, "");
        set_label(screen_->find_unique("RelicDescription"), Loc::get("inspect_relic_screen", "LOCKED_DESCRIPTION"));
        set_label(screen_->find_unique("FlavorText"), "");
        set_texture(lib_, image, "images/packed/common_ui/locked_model.png");
        if (image && image->is_canvas_item()) ((CanvasItem*)image)->self_modulate = Color(1, 1, 1, 1);
        rar = "Common";
    } else if (!seen) {
        set_label(name, Loc::get("inspect_relic_screen", "UNDISCOVERED_TITLE"));
        set_label(rarity, "");
        set_label(screen_->find_unique("RelicDescription"), Loc::get("inspect_relic_screen", "UNDISCOVERED_DESCRIPTION"));
        set_label(screen_->find_unique("FlavorText"), "");
        set_texture(lib_, image, big);
        if (image && image->is_canvas_item()) ((CanvasItem*)image)->self_modulate = Color(0, 0, 0, 0.9f);
    } else {
        set_label(name, r["title"].str());
        set_label(rarity, r["rarity_text"].str());
        set_label(screen_->find_unique("RelicDescription"), r["description"].str());
        set_label(screen_->find_unique("FlavorText"), r["flavor"].str());
        set_texture(lib_, image, big);
        if (image && image->is_canvas_item()) ((CanvasItem*)image)->self_modulate = Color(1, 1, 1, 1);
    }
    /* the rarity's label colour and frame hsv */
    Color col(1.0f, 0.964706f, 0.886275f, 1); /* cream */
    float h = 0.95f, s = 0.25f, v = 0.9f;
    if (rar == "Uncommon") { col = Color(0.529412f, 0.807843f, 0.921569f, 1); h = 0.426f; s = 0.8f; v = 1.1f; }
    else if (rar == "Rare") { col = Color(0.937255f, 0.784314f, 0.317647f, 1); h = 1.0f; s = 0.8f; v = 1.15f; }
    else if (rar == "Shop") { col = Color(0.529412f, 0.807843f, 0.921569f, 1); h = 0.525f; s = 2.5f; v = 0.85f; }
    else if (rar == "Event") { col = Color(0.498039f, 1, 0, 1); h = 0.23f; s = 0.75f; v = 0.9f; }
    else if (rar == "Ancient") { col = Color(1, 0.333333f, 0.333333f, 1); h = 0.875f; s = 3.0f; v = 0.9f; }
    if (rarity && rarity->is_canvas_item()) ((CanvasItem*)rarity)->modulate = col;
    Node* frame = screen_->find_unique("Frame");
    if (frame && frame->is_canvas_item()) {
        ((CanvasItem*)frame)->shader_params["h"] = h;
        ((CanvasItem*)frame)->shader_params["s"] = s;
        ((CanvasItem*)frame)->shader_params["v"] = v;
    }
    free_node(tips_);
    tips_ = NULL;
    if (seen) tips_ = tips_at(lib_, screen_, screen_->get_node("HoverTipRect"), r["inspect_tips"]);
    if (slide && popup_) {
        popup_->modulate = Color(1, 1, 1, 1);
        float l = popup_->offset[SIDE_LEFT], rr = popup_->offset[SIDE_RIGHT];
        tweens_.tween_from(&popup_->offset[SIDE_LEFT], l + slide * 100.0f, l, 0.25f, TRANS_EXPO, EASE_OUT);
        tweens_.tween_from(&popup_->offset[SIDE_RIGHT], rr + slide * 100.0f, rr, 0.25f, TRANS_EXPO, EASE_OUT);
    }
}

bool InspectRelicScreen::input(const PadState& pad) {
    if (pad.pressed & PadState::B) return false;
    int n = (int)relics_.size();
    if ((pad.pressed & PadState::RIGHT) && index_ < n - 1) set_relic(index_ + 1, 1);
    else if ((pad.pressed & PadState::LEFT) && index_ > 0) set_relic(index_ - 1, -1);
    return true;
}

void InspectRelicScreen::frame(double dt) { tweens_.update(dt); }

} /* namespace spire */

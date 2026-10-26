#include "bundle_select_screen.h"
#include "card_view.h"
#include "loc.h"

#include "../audio/game_audio.h"
#include "../scene/controls.h"
#include "../scene/node.h"
#include "../scene/scene.h"

namespace spire {

/* NCardBundle: smallScale 0.8, focused 0.85; the packs (and the opened
 * pack's cards) 400 px apart; the cards in a pack 45 px apart diagonally. */
static const float SMALL = 0.8f, HOVER = 0.85f, STEP = 400.0f, FAN = 45.0f;

static Control* as_control(Node* n) { return n && n->is_control() ? (Control*)n : NULL; }

BundleSelectScreen::BundleSelectScreen(SceneLibrary* lib)
    : lib_(lib), background_(NULL), banner_(NULL), row_(NULL), preview_(NULL), preview_cards_(NULL), cancel_(NULL),
      confirm_(NULL), cancel_x_(0), confirm_x_(0), focus_(-1), open_(-1) {}

/* NBackButton / NConfirmButton at the screen's corners (as the card
 * selection screens place them), off screen until the preview opens. */
static Control* corner_button(SceneLibrary* lib, Node* n, bool confirm, float* x) {
    Control* c = as_control(n);
    if (!c) return NULL;
    float w = c->offset[SIDE_RIGHT] - c->offset[SIDE_LEFT], h = c->offset[SIDE_BOTTOM] - c->offset[SIDE_TOP];
    *x = confirm ? 1920.0f - (c->offset[SIDE_RIGHT] + 120.0f) : -(c->offset[SIDE_LEFT] + 80.0f);
    float y = 1080.0f - (-c->offset[SIDE_BOTTOM] + 110.0f);
    for (int s = 0; s < 4; ++s) c->anchor[s] = 0;
    place(c, *x + (confirm ? 180.0f : -180.0f), y, w, h);
    show_glyph(lib, find_named(c, "HotkeyIcon"), confirm ? "y" : "b");
    return c;
}

void BundleSelectScreen::slide_button(Control* b, float x, bool shown) {
    if (!b) return;
    float w = b->offset[SIDE_RIGHT] - b->offset[SIDE_LEFT];
    float to = shown ? x : x + (b == confirm_ ? 180.0f : -180.0f);
    tweens_.tween(&b->offset[SIDE_LEFT], to, 0.3f, TRANS_EXPO, EASE_OUT);
    tweens_.tween(&b->offset[SIDE_RIGHT], to + w, 0.3f, TRANS_EXPO, EASE_OUT);
}

Node* BundleSelectScreen::build(const Json& d, std::string* error) {
    decision_ = d;
    bundles_.clear();
    tweens_.clear();
    focus_ = open_ = -1;
    Control* root = full_rect_control("BundleOverlay");
    if (background_) {
        root->add_child(overlay_behind(background_));
        background_ = NULL;
    }
    /* UseSharedBackstop. */
    ColorRect* back = (ColorRect*)SceneLibrary::create_node("ColorRect");
    back->name = "OverlayBackstop";
    back->anchor[SIDE_RIGHT] = back->anchor[SIDE_BOTTOM] = 1;
    back->color = Color(0, 0, 0, 0.7f);
    root->add_child(back);
    Node* screen = instance(lib_, "scenes/screens/card_selection/choose_a_bundle_selection_screen.tscn");
    if (!screen) {
        if (error) *error = "no choose_a_bundle_selection_screen.tscn";
        delete root;
        return NULL;
    }
    root->add_child(screen);
    hide(screen->find_unique("PeekButton"));
    hide(screen->find_unique("CardDetailScreen"));
    banner_ = as_control(screen->get_node("Banner"));
    if (banner_) {
        set_label(banner_->find_unique("Label"), Loc::get("gameplay_ui", "CHOOSE_A_PACK"));
        /* NCommonBanner.AnimateIn: from 50 px lower, fading in. */
        tweens_.tween_from(&banner_->modulate.a, 0.0f, 1.0f, 0.4f, TRANS_EXPO, EASE_OUT);
        float top = banner_->offset[SIDE_TOP], bottom = banner_->offset[SIDE_BOTTOM];
        tweens_.tween_from(&banner_->offset[SIDE_TOP], top + 50, top, 0.4f, TRANS_BACK, EASE_OUT);
        tweens_.tween_from(&banner_->offset[SIDE_BOTTOM], bottom + 50, bottom, 0.4f, TRANS_BACK, EASE_OUT);
    }
    row_ = ctl(screen, "BundleRow");
    preview_ = ctl(screen, "BundlePreviewContainer");
    preview_cards_ = preview_ ? ctl(preview_, "Cards") : NULL;
    if (preview_) preview_->visible = false;
    cancel_ = corner_button(lib_, preview_ ? preview_->get_node("Cancel") : NULL, false, &cancel_x_);
    confirm_ = corner_button(lib_, preview_ ? preview_->get_node("Confirm") : NULL, true, &confirm_x_);

    const Json& bs = d["bundles"];
    int n = (int)bs.size();
    for (int i = 0; row_ && i < n; ++i) {
        Node* b = instance(lib_, "scenes/cards/card_bundle.tscn");
        if (!b || !b->is_control()) {
            delete b;
            break;
        }
        Control* bc = (Control*)b;
        bc->scale = Vec2(SMALL, SMALL);
        bc->set_position(Vec2(-(float)(n - 1) * STEP * 0.5f + STEP * (float)i, 0));
        Node* holder = b->find_unique("Cards");
        const Json& cards = bs[(size_t)i]["cards"];
        int m = (int)cards.size();
        for (int k = 0; holder && k < m; ++k) {
            Node* card = make_card(lib_, cards[(size_t)k]);
            if (!card || !card->is_control()) {
                delete card;
                continue;
            }
            Control* cc = (Control*)card;
            cc->set_position(Vec2(-1, 1) * (FAN * ((float)k - (float)m / 2.0f)));
            float g = m > 1 ? 0.5f + (float)k / (float)(m - 1) * 0.5f : 1.0f;
            cc->modulate = Color(g, g, g, 1);
            holder->add_child(card);
        }
        row_->add_child(b);
        bundles_.push_back(bc);
    }
    root->add_child(frame_driver(this));
    if (!bundles_.empty()) focus(0);
    return root;
}

/* NCardBundle.OnFocused: 0.85 at once; unfocused: back to 0.8 (0.5 s). */
void BundleSelectScreen::focus(int i) {
    if (i == focus_ || i < 0 || i >= (int)bundles_.size()) return;
    if (focus_ >= 0) tweens_.tween(&bundles_[(size_t)focus_]->scale, Vec2(SMALL, SMALL), 0.5f, TRANS_EXPO, EASE_OUT);
    focus_ = i;
    tweens_.kill(&bundles_[(size_t)i]->scale);
    bundles_[(size_t)i]->scale = Vec2(HOVER, HOVER);
}

/* OnBundleClicked: the banner goes, the pack's cards spread in a row
 * (0.5 s), Cancel and Confirm come in. */
void BundleSelectScreen::open_preview(int i) {
    if (!preview_ || !preview_cards_) return;
    open_ = i;
    if (banner_) tweens_.tween(&banner_->modulate.a, 0.0f, 0.4f, TRANS_EXPO, EASE_OUT);
    while (!preview_cards_->children.empty()) {
        Node* c = preview_cards_->children.back();
        preview_cards_->remove_child(c);
        delete c;
    }
    const Json& cards = decision_["bundles"][(size_t)i]["cards"];
    int m = (int)cards.size();
    for (int k = 0; k < m; ++k) {
        Node* card = make_card(lib_, cards[(size_t)k]);
        if (!card || !card->is_control()) {
            delete card;
            continue;
        }
        Control* cc = (Control*)card;
        Vec2 from = Vec2(-(float)(bundles_.size() - 1) * STEP * 0.5f + STEP * (float)i, 0) +
                    Vec2(-1, 1) * (FAN * ((float)k - (float)m / 2.0f)) * SMALL;
        Vec2 to((float)(m - 1) * STEP * 0.5f - STEP * (float)k, 0);
        cc->set_position(from);
        tweens_.tween_from(&cc->offset[SIDE_LEFT], from.x, to.x, 0.5f, TRANS_EXPO, EASE_OUT);
        tweens_.tween_from(&cc->offset[SIDE_RIGHT], from.x + (cc->offset[SIDE_RIGHT] - cc->offset[SIDE_LEFT]),
                           to.x + (cc->offset[SIDE_RIGHT] - cc->offset[SIDE_LEFT]), 0.5f, TRANS_EXPO, EASE_OUT);
        tweens_.tween_from(&cc->offset[SIDE_TOP], from.y, to.y, 0.5f, TRANS_EXPO, EASE_OUT);
        tweens_.tween_from(&cc->offset[SIDE_BOTTOM], from.y + (cc->offset[SIDE_BOTTOM] - cc->offset[SIDE_TOP]),
                           to.y + (cc->offset[SIDE_BOTTOM] - cc->offset[SIDE_TOP]), 0.5f, TRANS_EXPO, EASE_OUT);
        preview_cards_->add_child(card);
    }
    preview_->visible = true;
    if (row_) row_->visible = false;
    slide_button(cancel_, cancel_x_, true);
    slide_button(confirm_, confirm_x_, true);
}

/* CancelSelection: back to the packs. */
void BundleSelectScreen::close_preview() {
    open_ = -1;
    if (preview_) preview_->visible = false;
    if (row_) row_->visible = true;
    if (banner_) tweens_.tween(&banner_->modulate.a, 1.0f, 0.4f, TRANS_EXPO, EASE_OUT);
    slide_button(cancel_, cancel_x_, false);
    slide_button(confirm_, confirm_x_, false);
}

std::string BundleSelectScreen::input(const PadState& pad) {
    if (bundles_.empty()) return std::string();
    /* The bundles' hitboxes play NButton's sounds; the preview's confirm
     * ui_click, its cancel (NBackButton) ui_back. */
    if (open_ >= 0) {
        if ((pad.pressed & (PadState::B | PadState::Y | PadState::A)) && lib_->audio())
            lib_->audio()->ui((pad.pressed & PadState::B) ? "event:/sfx/ui/clicks/ui_back" : "event:/sfx/ui/clicks/ui_click");
        if (pad.pressed & PadState::B) close_preview();
        else if (pad.pressed & (PadState::Y | PadState::A))
            return protocol_action("select_bundle",
                                   format("\"bundle_index\":%d", decision_["bundles"][(size_t)open_]["index"].i(open_)));
        return std::string();
    }
    int n = (int)bundles_.size();
    /* Focus neighbours wrap left / right. */
    int was = focus_;
    if (pad.pressed & PadState::LEFT) focus((focus_ + n - 1) % n);
    if (pad.pressed & PadState::RIGHT) focus((focus_ + 1) % n);
    if (focus_ != was && lib_->audio()) lib_->audio()->ui("event:/sfx/ui/clicks/ui_hover");
    if ((pad.pressed & PadState::A) && focus_ >= 0) {
        if (lib_->audio()) lib_->audio()->ui("event:/sfx/ui/clicks/ui_click");
        open_preview(focus_);
    }
    return std::string();
}

void BundleSelectScreen::frame(double dt) { tweens_.update(dt); }

} /* namespace spire */

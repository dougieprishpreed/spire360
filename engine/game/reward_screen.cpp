#include "reward_screen.h"
#include "card_view.h"
#include "loc.h"

#include "../scene/controls.h"
#include "../scene/node.h"
#include "../scene/scene.h"

namespace spire {

/* NCardRewardSelectionScreen: _cardXOffset; CardHolder SmallScale /
 * HoverScale. */
static const float CARD_X_OFFSET = 350.0f;
static const float SMALL_SCALE = 0.8f;
static const float HOVER_SCALE = 1.0f;
/* The card row's place is in card_reward_selection_screen.tscn (not in
 * the bundles yet): between the banner (bottom ~346) and the alternatives
 * (top 862). */
static const float CARD_ROW_X = 960.0f, CARD_ROW_Y = 590.0f;

CardRewardScreen::CardRewardScreen(SceneLibrary* lib)
    : lib_(lib), background_(NULL), focus_(0), can_skip_(true) {
    root_ = NULL;
}

Node* CardRewardScreen::build(const Json& d, std::string* error) {
    decision_ = d;
    holders_.clear();
    scales_.clear();
    Control* root = full_rect_control("CardRewardScreen");
    root_ = root;
    if (background_) {
        root->add_child(background_);
        background_ = NULL;
    }
    /* The overlays' shared backstop (run.tscn: colour not in the bundles). */
    ColorRect* back = (ColorRect*)SceneLibrary::create_node("ColorRect");
    back->name = "OverlayBackstop";
    back->anchor[SIDE_RIGHT] = back->anchor[SIDE_BOTTOM] = 1;
    back->color = Color(0, 0, 0, 0.7f);
    root->add_child(back);

    Control* ui = full_rect_control("UI");
    root->add_child(ui);
    /* NCommonBanner: "Choose a Card". */
    Node* banner = instance(lib_, "scenes/ui/common_banner.tscn");
    if (banner) {
        set_label(banner->find_unique("Label"), Loc::get("gameplay_ui", "CHOOSE_CARD_HEADER"));
        ui->add_child(banner);
    }
    /* The cards: x = (i - (n - 1) / 2) * 350 from the row's centre. */
    Control* row = (Control*)SceneLibrary::create_node("Control");
    row->name = "CardRow";
    place(row, CARD_ROW_X, CARD_ROW_Y, 0, 0);
    ui->add_child(row);
    const Json& cards = d["cards"];
    int n = (int)cards.size();
    for (int i = 0; i < n; ++i) {
        Node* holder = instance(lib_, "scenes/cards/holders/grid_card_holder.tscn");
        if (!holder || !holder->is_control()) {
            delete holder;
            continue;
        }
        Control* h = (Control*)holder;
        Node* card = make_card(lib_, cards[(size_t)i]);
        if (card) h->add_child(card);
        place(h, ((float)i - (float)(n - 1) / 2.0f) * CARD_X_OFFSET, 0, 0, 0);
        h->scale = Vec2(SMALL_SCALE, SMALL_SCALE);
        row->add_child(h);
        holders_.push_back(h);
        scales_.push_back(SMALL_SCALE);
    }
    /* CardRewardAlternative.Generate: Skip when the reward allows it
     * (hotkeys B and Start; the glyph shows B). */
    can_skip_ = d["can_skip"].b(true);
    if (can_skip_) {
        Node* skip = instance(lib_, "scenes/ui/card_reward_alternative_button.tscn");
        if (skip) {
            set_label(skip->get_node("Label"), Loc::get("card_reward_ui", "OPTION_SKIP.name"));
            show_glyph(lib_, skip->find_unique("HotkeyIcon"), "b");
            ui->add_child(skip);
        }
    }
    root->add_child(frame_driver(this));
    /* Default focus: the middle card. */
    focus_ = -1;
    focus(n / 2);
    if (n > 0) {
        holders_[(size_t)focus_]->scale = Vec2(HOVER_SCALE, HOVER_SCALE);
        tweens_.clear();
    }
    (void)error;
    return root;
}

/* NGridCardHolder focus: HoverScale at once, in front; unfocused cards
 * shrink back over 0.5 s (Expo Out). */
void CardRewardScreen::focus(int i) {
    if (holders_.empty()) return;
    if (focus_ >= 0 && focus_ < (int)holders_.size() && focus_ != i) {
        Control* old = holders_[(size_t)focus_];
        old->z_index = 0;
        tweens_.tween(&old->scale, Vec2(SMALL_SCALE, SMALL_SCALE), 0.5f, TRANS_EXPO, EASE_OUT);
    }
    focus_ = i;
    Control* h = holders_[(size_t)i];
    tweens_.kill(&h->scale);
    h->scale = Vec2(HOVER_SCALE, HOVER_SCALE);
    h->z_index = 1;
    tips_.show_for_card(lib_, root_, h, decision_["cards"][(size_t)i]["tips"]);
}

std::string CardRewardScreen::input(const PadState& pad) {
    int n = (int)holders_.size();
    if (n > 0 && (pad.pressed & (PadState::LEFT | PadState::RIGHT)))
        focus((pad.pressed & PadState::LEFT) ? (focus_ + n - 1) % n : (focus_ + 1) % n);
    if ((pad.pressed & PadState::A) && n > 0 && focus_ >= 0)
        return protocol_action("select_card_reward",
                               format("\"card_index\":%d", decision_["cards"][(size_t)focus_]["index"].i(focus_)));
    if ((pad.pressed & (PadState::B | PadState::START)) && can_skip_) return protocol_action("skip_card_reward");
    return std::string();
}

void CardRewardScreen::frame(double dt) {
    tweens_.update(dt);
    tips_.frame();
}

} /* namespace spire */

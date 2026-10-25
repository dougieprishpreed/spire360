#include "reward_screen.h"
#include "inspect_screens.h"
#include "card_view.h"
#include "deck_previews.h"
#include "loc.h"
#include "top_bar.h"

#include "../scene/controls.h"
#include "../scene/node.h"
#include "../scene/scene.h"

#include <math.h>

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

/* A node taken out of a scene that is then freed: its nodes no longer
 * name that scene's root as their owner. */
static void disown(Node* n, Node* root) {
    if (n->owner == root) n->owner = NULL;
    for (size_t i = 0; i < n->children.size(); ++i) disown(n->children[i], root);
}

CardRewardScreen::CardRewardScreen(SceneLibrary* lib)
    : lib_(lib), background_(NULL), focus_(0), can_skip_(true), alternative_(-1), inspect_(NULL), inspect_node_(NULL) {
    root_ = NULL;
}

/* The inspect node is under root_, freed with it by the owner. */
CardRewardScreen::~CardRewardScreen() { delete inspect_; }

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
        card_reward_glow(lib_, card, cards[(size_t)i]["rarity"].str());
        /* RefreshOptions: NCard.FlashRelicOnCard for each relic that changed
         * it (the relic's own flash is the logic's relic_flash cue) */
        const Json& mods = cards[(size_t)i]["modifying_relics"];
        for (size_t m = 0; card && m < mods.size(); ++m) {
            Resource* r = lib_->ext("images/atlases/relic_atlas.sprites/" + lower(relic_icon_base(mods[m])) + ".tres");
            flash_relic_on_card(lib_, card, r && r->is_texture() ? (Texture2DRes*)r : NULL);
        }
        place(h, ((float)i - (float)(n - 1) / 2.0f) * CARD_X_OFFSET, 0, 0, 0);
        h->scale = Vec2(SMALL_SCALE, SMALL_SCALE);
        row->add_child(h);
        holders_.push_back(h);
        scales_.push_back(SMALL_SCALE);
    }
    Node* scene = instance(lib_, "scenes/screens/card_selection/card_reward_selection_screen.tscn");
    /* CardRewardAlternative.Generate: Skip when the reward allows it (B and
     * Start; the glyph shows B), then Reroll (Driftwood) or Sacrifice (Pael's
     * Wing) on RB: buttons in the screen's UI/RewardAlternatives row */
    Node* row_alt = scene ? scene->get_node("UI/RewardAlternatives") : NULL;
    if (row_alt) {
        row_alt->parent->remove_child(row_alt);
        disown(row_alt, scene);
        ui->add_child(row_alt);
    }
    can_skip_ = d["can_skip"].b(true);
    alternative_ = -1;
    const Json& alts = d["alternatives"];
    for (int k = -1; k < (int)alts.size(); ++k) {
        if (k < 0 && !can_skip_) continue;
        Node* button = instance(lib_, "scenes/ui/card_reward_alternative_button.tscn");
        if (!button) continue;
        set_label(button->get_node("Label"), k < 0 ? Loc::get("card_reward_ui", "OPTION_SKIP.name") : alts[(size_t)k]["label"].str());
        show_glyph(lib_, button->find_unique("HotkeyIcon"), k < 0 ? "b" : "rb");
        if (k >= 0 && alternative_ < 0) alternative_ = alts[(size_t)k]["index"].i(-1);
        (row_alt ? row_alt : ui)->add_child(button);
    }
    /* %InspectPrompt (card_reward_selection_screen.tscn) */
    Node* prompt = scene ? scene->find_unique("InspectPrompt") : NULL;
    if (prompt) {
        /* its %HotkeyIcon is found through the scene: before it leaves */
        setup_inspect_prompt(lib_, prompt);
        prompt->parent->remove_child(prompt);
        disown(prompt, scene);
        ui->add_child(prompt);
    }
    delete scene;
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
    if (inspect_) {
        if (!inspect_->input(pad)) {
            if (inspect_node_->parent) inspect_node_->parent->remove_child(inspect_node_);
            delete inspect_node_;
            delete inspect_;
            inspect_node_ = NULL;
            inspect_ = NULL;
        }
        return std::string();
    }
    int n = (int)holders_.size();
    if ((pad.pressed & PadState::Y) && focus_ >= 0 && focus_ < n) {
        /* InspectCard: that card alone */
        Json list = Json::make_array();
        list.append(decision_["cards"][(size_t)focus_]);
        inspect_ = new InspectCardScreen(lib_);
        std::string err;
        inspect_node_ = inspect_->build(list, 0, false, &err);
        if (inspect_node_) {
            tips_.hide();
            root_->add_child(inspect_node_);
        } else {
            delete inspect_;
            inspect_ = NULL;
        }
        return std::string();
    }
    if (n > 0 && (pad.pressed & (PadState::LEFT | PadState::RIGHT)))
        focus((pad.pressed & PadState::LEFT) ? (focus_ + n - 1) % n : (focus_ + 1) % n);
    if ((pad.pressed & PadState::A) && n > 0 && focus_ >= 0) {
        /* CardReward.OnSelect: the card goes into the deck
         * (NGlobalUi.ReparentCard, NCardFlyVfx) */
        Control* h = holders_[(size_t)focus_];
        if (h && DeckPreviews::current()) {
            Xform2D g = h->global_transform();
            float s = sqrtf(g.x.x * g.x.x + g.x.y * g.x.y);
            DeckPreviews::current()->fly_to_deck(decision_["cards"][(size_t)focus_], g.origin.x, g.origin.y, s,
                                                 decision_["player"]["character"].str());
            h->visible = false;
        }
        return protocol_action("select_card_reward",
                               format("\"card_index\":%d", decision_["cards"][(size_t)focus_]["index"].i(focus_)));
    }
    if ((pad.pressed & (PadState::B | PadState::START)) && can_skip_) return protocol_action("skip_card_reward");
    /* the other alternative's hotkey (MegaInput.viewExhaustPileAndTabRight) */
    if ((pad.pressed & PadState::RB) && alternative_ >= 0)
        return protocol_action("choose_card_reward_alternative", format("\"alternative_index\":%d", alternative_));
    return std::string();
}

void CardRewardScreen::frame(double dt) {
    tweens_.update(dt);
    tips_.frame();
}

} /* namespace spire */

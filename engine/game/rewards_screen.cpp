#include "rewards_screen.h"
#include "loc.h"
#include "top_bar.h"

#include "../scene/controls.h"
#include "../scene/node.h"
#include "../scene/scene.h"

namespace spire {

/* NRewardButton: the background's hsv v. */
static const float HSV_DEFAULT = 0.9f, HSV_HOVER = 1.1f;
/* StsColors.cream / gold, rewardLabelOutline / rewardLabelGoldOutline. */
static const unsigned CREAM = 0xFFF6E2, GOLD = 0xEFC851, OUTLINE = 0x225157, GOLD_OUTLINE = 0x4B4511;

RewardsScreen::RewardsScreen(SceneLibrary* lib)
    : lib_(lib), background_(NULL), window_(NULL), focus_(-1), start_focus_(0), animate_in_(true), terminal_(true),
      can_skip_(true), delay_(0) {
    overlay_[0] = overlay_[1] = NULL;
}

/* Reward.CreateIcon: a texture filling the 56x56 %Icon (gold, cards),
 * the potion (NPotion at 0.8), the relic's big icon. */
Node* RewardsScreen::make_icon(const Json& r) {
    const std::string& type = r["type"].str();
    if (type == "potion" && r["potion_id"].is_string()) {
        Node* p = make_potion(lib_, r["potion_id"].str());
        if (p) {
            Control* pc = (Control*)p;
            place(pc, 0, -2, 60, 60); /* PotionReward.IconPosition */
            pc->scale = Vec2(0.8f, 0.8f);
        }
        return p;
    }
    TextureRect* t = (TextureRect*)SceneLibrary::create_node("TextureRect");
    t->name = "RewardIcon";
    t->anchor[SIDE_RIGHT] = t->anchor[SIDE_BOTTOM] = 1;
    t->expand_mode = TextureRect::EXPAND_IGNORE_SIZE;
    t->mouse_filter = 2;
    if (type == "relic" && r["relic_id"].is_string()) {
        /* RelicModel.BigIcon (ResolvedBigIconPath: the image, else its
         * beta image); the atlas sprite if the pack lacks both. */
        std::string id = lower(relic_icon_base(r, "relic_icon_base", "relic_id"));
        Resource* big = lib_->ext("images/relics/" + id + ".png");
        if (!big || !big->is_texture()) big = lib_->ext("images/relics/beta/" + id + ".png");
        if (big && big->is_texture()) t->texture = (Texture2DRes*)big;
        else set_texture(lib_, t, "images/atlases/relic_atlas.sprites/" + id + ".tres");
    } else if (r["icon"].is_string()) {
        std::string path = r["icon"].str();
        if (path.compare(0, 6, "res://") == 0) path = path.substr(6);
        set_texture(lib_, t, path);
        /* GoldReward.IconPosition. */
        if (type == "gold") {
            t->offset[SIDE_TOP] = -5;
            t->offset[SIDE_BOTTOM] = -5;
        }
    }
    return t;
}

Node* RewardsScreen::build(const Json& d, std::string* error) {
    decision_ = d;
    rows_.clear();
    tweens_.clear();
    terminal_ = d["terminal"].b(true);
    can_skip_ = d["can_skip"].b(true);
    Control* root = full_rect_control("RewardsOverlay");
    if (background_) {
        root->add_child(background_);
        background_ = NULL;
    }
    const Json& rewards = d["rewards"];
    bool complete = rewards.size() == 0;
    /* The overlays' shared backstop (its colour is in run.tscn, not in the
     * bundles); hidden once a fight's rewards are all taken. */
    ColorRect* back = (ColorRect*)SceneLibrary::create_node("ColorRect");
    back->name = "OverlayBackstop";
    back->anchor[SIDE_RIGHT] = back->anchor[SIDE_BOTTOM] = 1;
    back->color = Color(0, 0, 0, 0.7f);
    root->add_child(back);
    if (complete) back->modulate.a = 0;
    else if (animate_in_) tweens_.tween_from(&back->modulate.a, 0.0f, 1.0f, 0.5f, TRANS_CUBIC, EASE_OUT);

    Node* screen = instance(lib_, "scenes/screens/rewards_screen.tscn");
    if (!screen) {
        if (error) *error = "no rewards_screen.tscn";
        delete root;
        return NULL;
    }
    root->add_child(screen);
    overlay_[0] = back;
    overlay_[1] = screen;
    if (delay_ > 0) {
        set_visible(back, false);
        set_visible(screen, false);
    }
    hide(screen->find_unique("WaitingForOtherPlayers"));
    hide(screen->find_unique("Scrollbar"));
    set_label(screen->find_unique("HeaderLabel"), Loc::get("gameplay_ui", "COMBAT_REWARD_HEADER_LOOT"));
    Node* w = screen->get_node("Rewards");
    window_ = w && w->is_control() ? (Control*)w : NULL;

    Node* list = screen->find_unique("RewardsContainer");
    for (size_t i = 0; list && i < rewards.size(); ++i) {
        Node* b = instance(lib_, "scenes/rewards/reward_button.tscn");
        if (!b || !b->is_control()) {
            delete b;
            break;
        }
        Row row;
        row.node = (Control*)b;
        row.background = ctl(b, "Background");
        row.label = b->find_unique("Label");
        row.reticle = ctl(b, "SelectionReticle");
        row.index = rewards[i]["index"].i((int)i);
        set_label(row.label, rewards[i]["label"].str());
        if (row.label && row.label->is_control()) {
            ((Control*)row.label)->add_theme_color_override("default_color", hex(CREAM));
            ((Control*)row.label)->add_theme_color_override("font_outline_color", hex(OUTLINE));
        }
        if (row.background) row.background->shader_params["v"] = 0.8f; /* the scene's */
        if (row.reticle) row.reticle->modulate.a = 0;
        Node* icon_box = b->find_unique("Icon");
        Node* icon = make_icon(rewards[i]);
        if (icon && icon_box) icon_box->add_child(icon);
        else delete icon;
        list->add_child(b);
        rows_.push_back(row);
    }

    /* The proceed arrow: "Skip" while rewards remain, "Proceed" (pulsing)
     * once a fight's are all taken; none when skipping is not allowed. */
    proceed_.attach(lib_, screen->get_node("ProceedButton"));
    proceed_.set_text(Loc::get("gameplay_ui", complete ? "PROCEED_BUTTON" : "CHOOSE_CARD_SKIP_BUTTON"));
    bool all_skipped = !complete;
    for (size_t i = 0; i < rewards.size(); ++i)
        if (!rewards[i]["skipped"].b()) all_skipped = false;
    proceed_.set_pulse(complete || all_skipped);
    if (can_skip_ || complete) proceed_.enable(!animate_in_);

    if (window_) {
        if (complete) {
            window_->modulate.a = 0;
        } else if (animate_in_) {
            /* AfterOverlayShown: modulate to white (0.5 s), position from
             * 100 px lower (0.5 s, Back Out). */
            window_->modulate = Color(0, 0, 0, 0);
            tween_color(tweens_, &window_->modulate, Color(1, 1, 1, 1), 0.5f);
            float top = window_->offset[SIDE_TOP], bottom = window_->offset[SIDE_BOTTOM];
            tweens_.tween_from(&window_->offset[SIDE_TOP], top + 100, top, 0.5f, TRANS_BACK, EASE_OUT);
            tweens_.tween_from(&window_->offset[SIDE_BOTTOM], bottom + 100, bottom, 0.5f, TRANS_BACK, EASE_OUT);
        }
    }
    root->add_child(frame_driver(this));
    focus_ = -1;
    if (!rows_.empty()) {
        int f = start_focus_ < 0 ? 0 : start_focus_;
        if (f >= (int)rows_.size()) f = (int)rows_.size() - 1;
        focus(f);
    }
    return root;
}

/* NRewardButton.OnFocus / OnUnfocus. */
Control* RewardsScreen::row_for(int reward_index) const {
    for (size_t i = 0; i < rows_.size(); ++i)
        if (rows_[i].index == reward_index) return rows_[i].node;
    return NULL;
}

void RewardsScreen::focus(int i) {
    if (i < 0 || i >= (int)rows_.size() || i == focus_) return;
    if (focus_ >= 0 && focus_ < (int)rows_.size()) {
        Row& o = rows_[(size_t)focus_];
        if (o.label && o.label->is_control()) {
            ((Control*)o.label)->add_theme_color_override("default_color", hex(CREAM));
            ((Control*)o.label)->add_theme_color_override("font_outline_color", hex(OUTLINE));
        }
        if (o.background)
            tweens_.tween(&o.background->shader_params["v"], HSV_DEFAULT, 0.5f, TRANS_EXPO, EASE_OUT);
        if (o.reticle) {
            tweens_.tween(&o.reticle->modulate.a, 0.0f, 0.2f, TRANS_SINE, EASE_OUT);
            tweens_.tween(&o.reticle->scale, Vec2(1.05f, 1.05f), 0.2f, TRANS_SINE, EASE_OUT);
        }
    }
    focus_ = i;
    Row& r = rows_[(size_t)i];
    if (r.label && r.label->is_control()) {
        ((Control*)r.label)->add_theme_color_override("default_color", hex(GOLD));
        ((Control*)r.label)->add_theme_color_override("font_outline_color", hex(GOLD_OUTLINE));
    }
    if (r.background) {
        tweens_.kill(&r.background->shader_params["v"]);
        r.background->shader_params["v"] = HSV_HOVER;
    }
    if (r.reticle) {
        /* NSelectionReticle.OnSelect. */
        tweens_.kill(&r.reticle->modulate.a);
        r.reticle->modulate.a = 1;
        tweens_.tween_from(&r.reticle->scale, Vec2(0.9f, 0.9f), Vec2(1, 1), 0.5f, TRANS_EXPO, EASE_OUT);
    }
}

std::string RewardsScreen::input(const PadState& pad) {
    int n = (int)rows_.size();
    /* Up / down: the neighbours, no wrapping. */
    if (n > 0 && (pad.pressed & PadState::UP) && focus_ > 0) focus(focus_ - 1);
    if (n > 0 && (pad.pressed & PadState::DOWN) && focus_ < n - 1) focus(focus_ + 1);
    if ((pad.pressed & PadState::A) && focus_ >= 0 && focus_ < n)
        return protocol_action("take_reward", format("\"reward_index\":%d", rows_[(size_t)focus_].index));
    if ((pad.pressed & PadState::Y) && proceed_.enabled()) return protocol_action("proceed");
    return std::string();
}

void RewardsScreen::frame(double dt) {
    if (delay_ > 0) {
        delay_ -= dt;
        if (delay_ > 0) return;
        delay_ = 0;
        set_visible(overlay_[0], true);
        set_visible(overlay_[1], true);
    }
    tweens_.update(dt);
    proceed_.frame(dt);
}

} /* namespace spire */

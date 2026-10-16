#include "treasure_screen.h"
#include "loc.h"
#include "top_bar.h"

#include "../scene/controls.h"
#include "../scene/node.h"
#include "../scene/particles.h"
#include "../scene/scene.h"
#include "../scene/spine_sprite.h"
#include "../spine/spine.h"

namespace spire {

/* NTreasureRoom.OpenChest: Skip comes after 2.5 s (EnableSkipAfterDelay). */
static const double SKIP_DELAY = 2.5;
/* SetAlignmentForRelic: the tip below the holder's relic (the singleplayer
 * holder at the screen's centre, 81 px up; the relic at 2x, 120 px). */
static const float TIP_X = 926.0f, TIP_Y = 555.0f;

static Control* as_control(Node* n) { return n && n->is_control() ? (Control*)n : NULL; }

static std::string strip_res(const std::string& p) { return p.compare(0, 6, "res://") == 0 ? p.substr(6) : p; }

TreasureScreen::TreasureScreen(SceneLibrary* lib)
    : lib_(lib), chest_(NULL), chest_visual_(NULL), holder_(NULL), relic_(NULL), tip_(NULL), t_(0), skip_at_(-1),
      sent_(false) {}

/* NTreasureButton.Setup: the act's skeleton and skin, "animation" (the
 * opening) waiting at its start (time scale 0), "shine_fade" after it.
 * Opened: played (AnimOut / AnimIn set the time scale back to 1). */
void TreasureScreen::setup_chest(const Json& chest, bool opened) {
    if (!chest_visual_ || !chest.is_object()) return;
    Resource* r = lib_->ext(strip_res(chest["skeleton"].str()));
    if (!r || r->kind != Resource::RES_SPINE_DATA) {
        warn_missing(strip_res(chest["skeleton"].str()));
        return;
    }
    chest_visual_->set_data(lib_, (SpineDataRes*)r);
    /* Focused while closed: the outline skin (UpdateChestSkin(true)). */
    chest_visual_->set_skin(chest[opened ? "skin" : "skin_outline"].str());
    if (chest_visual_->set_animation("animation", false, 0)) {
        SpAnimationState* st = chest_visual_->animation_state();
        SpSkeleton* sk = chest_visual_->skeleton();
        const SpAnimation* shine = sk ? sk->data()->find_animation("shine_fade") : NULL;
        if (st && shine) st->add_animation(0, shine, false, 0);
    }
    chest_visual_->time_scale = opened ? 1.0f : 0.0f;
}

/* NTreasureRoomRelicHolder.Initialize: the relic fading in from black, the
 * rarity's glow; focused (the default focus): at 2.1 with its hover tip. */
void TreasureScreen::show_relic(const Json& relic, bool animate) {
    if (!holder_) return;
    holder_->visible = true;
    relic_ = as_control(holder_->find_unique("Relic"));
    if (!relic_) return;
    set_relic_icon(lib_, relic_->find_unique("Icon"), relic["id"].str());
    const std::string& rarity = relic["rarity"].str();
    Node* glow = NULL;
    if (rarity == "Rare") glow = holder_->find_unique("RareGlow");
    if (rarity == "Uncommon") glow = holder_->find_unique("UncommonGlow");
    if (glow) {
        set_visible(glow, true);
        if (animate && glow->type == "GPUParticles2D")
            tweens_.tween_from(&((Particles2D*)glow)->modulate.a, 0.0f, 1.0f, 0.25f);
    }
    if (animate) {
        relic_->modulate = Color(0, 0, 0, 0);
        tween_color(tweens_, &relic_->modulate, Color(1, 1, 1, 1), 0.25f);
    }
    relic_->scale = Vec2(2.1f, 2.1f);
}

Node* TreasureScreen::build(const Json& d, std::string* error) {
    decision_ = d;
    tweens_.clear();
    sent_ = false;
    t_ = 0;
    skip_at_ = -1;
    Node* room = instance(lib_, "scenes/rooms/treasure_room.tscn");
    if (!room) {
        if (error) *error = "no treasure_room.tscn";
        return NULL;
    }
    stage_ = d["stage"].str();
    bool closed = stage_ == "closed", open = stage_ == "open", done = !closed && !open;
    bool opening = open && previous_ == "closed";
    bool finishing = done && previous_ == "open";

    hide(room->find_unique("SkipMultiplayerVoteContainer"));
    hide(room->find_unique("FightBackstop"));
    hide(room->find_unique("HandsContainer"));
    Control* empty = ctl(room, "EmptyLabel");
    if (empty) empty->visible = false;

    chest_ = ctl(room, "Chest");
    chest_visual_ = NULL;
    Node* cv = chest_ ? chest_->find_unique("ChestVisual") : NULL;
    if (cv && cv->type == "SpineSprite") chest_visual_ = (SpineSprite*)cv;
    setup_chest(d["chest"], !closed);
    if (chest_) {
        Node* hotkey = chest_->find_unique("HotkeyIcon");
        if (closed) show_glyph(lib_, hotkey, "a");
        else hide(hotkey);
        /* AnimOut: half transparent while the relic is offered; AnimIn:
         * white again. */
        if (opening) tween_color(tweens_, &chest_->modulate, Color(1, 1, 1, 0.5f), 0.4f);
        else if (open) chest_->modulate = Color(1, 1, 1, 0.5f);
        else if (finishing) {
            chest_->modulate = Color(1, 1, 1, 0.5f);
            tween_color(tweens_, &chest_->modulate, Color(1, 1, 1, 1), 0.3f);
        }
    }
    /* The gold burst (GoldExplosion: one coin per gold). */
    Node* gold = room->find_unique("GoldExplosion");
    if (gold && gold->type == "GPUParticles2D" && opening && d["gold"].i() > 0) {
        ((Particles2D*)gold)->amount = d["gold"].i();
        ((Particles2D*)gold)->emitting = true;
    }

    /* The banner: in (0.4 s, from 50 px lower) when the chest opens, out
     * once the relic is taken. */
    Control* banner = ctl(room, "Banner");
    if (banner) {
        set_label(banner->find_unique("Label"), Loc::get("gameplay_ui", "TREASURE_BANNER"));
        banner->modulate.a = open ? 1.0f : 0.0f;
        if (opening) {
            tweens_.tween_from(&banner->modulate.a, 0.0f, 1.0f, 0.4f, TRANS_EXPO, EASE_OUT);
            float top = banner->offset[SIDE_TOP], bottom = banner->offset[SIDE_BOTTOM];
            tweens_.tween_from(&banner->offset[SIDE_TOP], top + 50, top, 0.4f, TRANS_BACK, EASE_OUT);
            tweens_.tween_from(&banner->offset[SIDE_BOTTOM], bottom + 50, bottom, 0.4f, TRANS_BACK, EASE_OUT);
        } else if (finishing) {
            tweens_.tween_from(&banner->modulate.a, 1.0f, 0.0f, 0.4f, TRANS_EXPO, EASE_OUT);
        }
    }

    /* The relic collection: the singleplayer holder only. */
    Control* collection = ctl(room, "RelicCollection");
    holder_ = ctl(room, "SingleplayerRelicHolder");
    for (int i = 1; i <= 4; ++i) hide(room->find_unique(format("MultiplayerRelicHolder%d", i).c_str()));
    hide(holder_ ? holder_->find_unique("MultiplayerVoteContainer") : NULL);
    const Json& relics = d["relics"];
    tip_ = NULL;
    if (collection) {
        collection->visible = open || finishing;
        if (open && relics.size() > 0) {
            show_relic(relics[(size_t)0], opening);
            tip_ = make_hover_tip(lib_, relics[(size_t)0]["title"].str(), relics[(size_t)0]["description"].str());
            if (tip_) {
                tip_->set_position(Vec2(TIP_X, TIP_Y));
                room->add_child(tip_);
                if (opening) tweens_.tween_from(&tip_->modulate.a, 0.0f, 1.0f, 0.4f);
            }
        }
        /* AnimIn: from transparent (0.4 s); AnimOut: to transparent white
         * (0.3 s). The relic just taken is not in the decision any more:
         * the holder is empty while it fades. */
        if (opening) {
            collection->modulate = Color(0, 0, 0, 0);
            tween_color(tweens_, &collection->modulate, Color(1, 1, 1, 1), 0.4f);
        } else if (finishing) {
            if (holder_) holder_->visible = false;
            tweens_.tween_from(&collection->modulate.a, 1.0f, 0.0f, 0.3f);
        }
    }

    proceed_.attach(lib_, room->find_unique("ProceedButton"));
    proceed_.set_pulse(true);
    if (open) {
        proceed_.set_text(Loc::get("gameplay_ui", "CHOOSE_CARD_SKIP_BUTTON"));
        if (opening) skip_at_ = SKIP_DELAY;
        else proceed_.enable(true);
    } else if (done) {
        proceed_.set_text(Loc::get("gameplay_ui", "PROCEED_BUTTON"));
        proceed_.enable(!finishing);
    }
    room->add_child(frame_driver(this));
    return room;
}

std::string TreasureScreen::input(const PadState& pad) {
    if (sent_) return std::string();
    if (stage_ == "closed") {
        if (pad.pressed & PadState::A) {
            sent_ = true;
            return protocol_action("open_chest");
        }
        return std::string();
    }
    if (stage_ == "open" && (pad.pressed & PadState::A) && decision_["relics"].size() > 0) {
        /* OnPress: the relic sinks (1.9). */
        if (relic_) tweens_.tween(&relic_->scale, Vec2(1.9f, 1.9f), 0.4f, TRANS_EXPO, EASE_OUT);
        sent_ = true;
        return protocol_action("pick_relic", format("\"relic_index\":%d", decision_["relics"][(size_t)0]["index"].i()));
    }
    if ((pad.pressed & PadState::Y) && proceed_.enabled()) {
        sent_ = true;
        return protocol_action("proceed");
    }
    return std::string();
}

void TreasureScreen::frame(double dt) {
    t_ += dt;
    if (skip_at_ >= 0 && t_ >= skip_at_) {
        skip_at_ = -1;
        proceed_.enable();
    }
    tweens_.update(dt);
    proceed_.frame(dt);
}

} /* namespace spire */

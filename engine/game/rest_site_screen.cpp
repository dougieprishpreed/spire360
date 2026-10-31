#include "rest_site_screen.h"
#include "deck_previews.h"
#include "../core/cue_ledger.h"
#include "loc.h"
#include "top_bar.h"
#include "../audio/game_audio.h"

#include "../scene/controls.h"
#include "../scene/node.h"
#include "../scene/particles.h"
#include "../scene/scene.h"
#include "../scene/spine_sprite.h"
#include "../scene/script.h"

#include <math.h>

namespace spire {

namespace {
/* NRestSiteCharacter.DoShake: ScreenPunchInstance(15, 0.4, 0), a cosine
 * wiggle sideways easing out over 0.4 s. */
class RestShake : public ScriptBehavior {
public:
    RestShake(Node2D* who) : who_(who), origin_(who->position), t_(0.4) {}
    virtual void process(Node* n, double dt) {
        if (n->queued_for_deletion()) return;
        t_ -= dt;
        if (t_ <= 0) {
            who_->position = origin_;
            n->queue_free();
            return;
        }
        float u = (float)(t_ / 0.4), ease = 1 - (1 - u) * (1 - u) * (1 - u);
        who_->position = origin_ + Vec2(cosf((float)t_ * 60.0f) * 15.0f * ease, 0);
    }

private:
    Node2D* who_;
    Vec2 origin_;
    double t_;
};

/* NRelicFlashVfx at the character's origin: its three icons 0.2 s apart,
 * each growing to 1.25 (Cubic Out, 1 s) as it fades (1.5 s), the effect
 * rising 64 px (Expo Out, 1 s). */
class RestRelicFlash : public ScriptBehavior {
public:
    explicit RestRelicFlash(const Vec2& at) : t_(0), at_(at) {}
    virtual void process(Node* n, double dt) {
        t_ += (float)dt;
        if (n->is_control()) {
            float k = t_ < 1 ? t_ : 1;
            ((Control*)n)->set_position(at_ - Vec2(0, 64 * (k >= 1 ? 1 : 1 - powf(2, -10 * k))));
        }
        for (int i = 0; i < 3; ++i) {
            Node* img = n->get_node(format("Image%d", i + 1));
            if (!img || !img->is_control()) continue;
            Control* c = (Control*)img;
            float lt = t_ - 0.2f * (float)i;
            if (lt < 0) {
                c->modulate.a = 0;
                continue;
            }
            float g = lt < 1 ? lt : 1, u = 1 - g;
            float s = 1 + 0.25f * (1 - u * u * u);
            c->scale = Vec2(s, s);
            c->modulate.a = lt < 1.5f ? 1 - lt / 1.5f : 0.0f;
        }
        if (t_ >= 1.95f && !n->queued_for_deletion()) n->queue_free();
    }

private:
    float t_;
    Vec2 at_;
};
} /* namespace */

/* Colors.DarkGray: the characters once the fire is out. */
static const float DARK_GRAY = 0.6627451f;

static Control* as_control(Node* n) { return n && n->is_control() ? (Control*)n : NULL; }

RestSiteScreen::RestSiteScreen(SceneLibrary* lib)
    : lib_(lib), choices_screen_(NULL), description_(NULL), lighting_(NULL), focus_(-1), start_focus_(0),
      done_(false), passive_(false), chosen_(false), t_(0), proceed_at_(-1), again_at_(-1), room_(NULL), smoke_(NULL) {}

/* NRestSiteButton per option (Create, Reload): the icon (OptionId's
 * images/ui/rest_site/option_<id>.png), the title; a disabled option
 * greyed (s 0, v 0.6). AnimateIn: from transparent black to white. */
void RestSiteScreen::make_options(Node* room, const Json& options, bool animate_in) {
    Node* box = room->find_unique("ChoicesContainer");
    for (size_t i = 0; box && i < options.size(); ++i) {
        const Json& o = options[i];
        Node* b = instance(lib_, "scenes/rest_site/rest_site_button.tscn");
        if (!b || !b->is_control()) {
            delete b;
            break;
        }
        Option op;
        op.node = (Control*)b;
        op.visuals = ctl(b, "Visuals");
        op.outline = ctl(b, "Outline");
        op.icon = ctl(b, "Icon");
        op.label = ctl(b, "Label");
        op.label_top = op.label ? op.label->offset[SIDE_TOP] : 0;
        op.label_bottom = op.label ? op.label->offset[SIDE_BOTTOM] : 0;
        op.index = o["index"].i((int)i);
        op.enabled = o["is_enabled"].b(true);
        op.description = o["description"].str();
        set_texture(lib_, op.icon, "images/ui/rest_site/option_" + lower(o["option_id"].str()) + ".png");
        set_label(op.label, o["title"].is_string() ? o["title"].str() : o["option_id"].str());
        if (op.icon) {
            op.icon->shader_params["s"] = op.enabled ? 1.0f : 0.0f;
            op.icon->shader_params["v"] = op.enabled ? 1.0f : 0.6f;
        }
        if (animate_in) {
            op.node->modulate = Color(0, 0, 0, 0);
            tween_color(tweens_, &op.node->modulate, Color(1, 1, 1, 1), 0.5f, TRANS_CUBIC, EASE_OUT);
        }
        box->add_child(b);
        options_.push_back(op);
    }
}

Node* RestSiteScreen::build(const Json& d, std::string* error) {
    decision_ = d;
    options_.clear();
    character_boxes_.clear();
    character_spines_.clear();
    tweens_.clear();
    t_ = 0;
    proceed_at_ = -1;
    again_at_ = -1;
    smoke_ = NULL;
    focus_ = -1;
    chosen_ = false;
    /* The rest's jingles (1.2 MB each, read as the rest was chosen: late
     * on the console, rp104) and its blanket, read while the options show */
    if (lib_->audio()) {
        for (const char* v = "abc"; *v; ++v) lib_->audio()->prefetch(format("STS_SleepJingle_1%c_NewMix_v1.mp3", *v));
        lib_->audio()->prefetch("SOTE_SFX_SleepBlanket_v1.mp3");
    }
    Node* room = instance(lib_, "scenes/rooms/rest_site_room.tscn");
    if (!room) {
        if (error) *error = "no rest_site_room.tscn";
        return NULL;
    }
    room_ = room;
    const Json& options = d["options"];
    done_ = d["done"].b() || options.size() == 0;
    /* an option was just taken (the decision before showed them): with
     * none left, or with some (the synchronizer kept them: Miniature Tent) */
    bool after_choice = before_["options"].size() > 0 && (done_ || d["proceed"].b());

    /* The act's background (Act.CreateRestSiteBackground) behind the
     * characters, and its %RestSiteLighting. */
    Node* bg_box = room->get_node("BgContainer");
    std::string bg_path = d["background"].str();
    if (bg_path.compare(0, 6, "res://") == 0) bg_path = bg_path.substr(6);
    Node* bg = bg_path.empty() ? NULL : instance(lib_, bg_path);
    if (bg && bg_box) {
        bg_box->add_child(bg);
        bg_box->children.pop_back();
        bg_box->children.insert(bg_box->children.begin(), bg);
        lighting_ = bg->find_unique("RestSiteLighting");
    } else {
        delete bg;
    }
    /* The player (NRestSiteCharacter) in Character_1, playing the act's
     * loop from a random point of it. */
    static const char* const BOXES[] = { "BgContainer/Character_1", "BgContainer/Character_2",
                                         "BgContainer/Character_3", "BgContainer/Character_4" };
    for (int i = 0; i < 4; ++i) {
        Control* c = as_control(room->get_node(BOXES[i]));
        if (c) character_boxes_.push_back(c);
    }
    std::string character = lower(d["player"]["character"].is_string() ? d["player"]["character"].str() : "ironclad");
    Node* who = instance(lib_, "scenes/rest_site/characters/" + character + "_rest_site.tscn");
    if (who && !character_boxes_.empty()) {
        character_boxes_[0]->add_child(who);
        int act = d["context"]["act"].i(1);
        const char* anim = act >= 3 ? "glory_loop" : act == 2 ? "hive_loop" : "overgrowth_loop";
        for (size_t i = 0; i < who->children.size(); ++i) {
            Node* n = who->children[i];
            if (n->type != "SpineSprite") continue;
            SpineSprite* s = (SpineSprite*)n;
            if (s->set_animation(anim, true, 0)) s->set_track_fraction(0, (float)(chaotic_rand() % 1000) / 1000.0f);
            character_spines_.push_back(n);
        }
        hide(who->find_unique("SelectionReticle"));
    } else {
        delete who;
    }

    set_label(room->find_unique("Header"), Loc::get("rest_site_ui", "PROMPT"));
    choices_screen_ = ctl(room, "ChoicesScreen");
    description_ = ctl(room, "Description");
    if (description_) description_->modulate = Color(1, 1, 1, 0);

    proceed_.attach(lib_, room->find_unique("ProceedButton"));
    proceed_.set_text(Loc::get("gameplay_ui", "PROCEED_BUTTON"));
    proceed_.set_pulse(true);

    if (!done_ && after_choice && !passive_) {
        /* AfterSelectingOptionAsync with options left: the choices fade
         * (HideChoices, 0.5 s) as the option's effect plays, the fire
         * stays; then the ones left (UpdateRestSiteOptions) and the proceed
         * arrow (frame) */
        make_options(room, before_["options"], false);
        if (choices_screen_) tweens_.tween_from(&choices_screen_->modulate.a, 1.0f, 0.0f, 0.5f);
        double wait = post_select(room, d["steps"]);
        again_at_ = wait > 0.5 ? wait : 0.5;
        chosen_ = true;
    } else if (!done_) {
        make_options(room, options, true);
        if (!passive_ && !options_.empty()) {
            int f = start_focus_ < 0 ? 0 : start_focus_;
            focus(f >= (int)options_.size() ? (int)options_.size() - 1 : f);
        }
        if (d["proceed"].b() && !passive_) proceed_.enable(true);
    } else {
        /* AfterSelectingOptionAsync: the choices (the ones just shown)
         * fade out, the fire goes out, the proceed arrow comes in. */
        if (after_choice) {
            make_options(room, before_["options"], false);
            if (choices_screen_) tweens_.tween_from(&choices_screen_->modulate.a, 1.0f, 0.0f, 0.5f);
        } else if (choices_screen_) {
            choices_screen_->modulate.a = 0;
        }
        extinguish();
        double wait = after_choice && !passive_ ? post_select(room, d["steps"]) : 0;
        if (wait > 0) proceed_at_ = wait;
        else if (!passive_) proceed_.enable(!after_choice);
    }
    room->add_child(frame_driver(this));
    shake_target_ = room->is_control() ? (Control*)room : NULL; /* NRestSiteRoom */
    return room;
}

/* ExtinguishFireIfAble: the flame glow off (the light_off track where the
 * skeleton has it), the characters' containers dark grey, no lighting. */
void RestSiteScreen::extinguish() {
    for (size_t i = 0; i < character_spines_.size(); ++i) {
        SpineSprite* s = (SpineSprite*)character_spines_[i];
        if (s->has_animation("_tracks/light_off")) s->set_animation("_tracks/light_off", true, 1);
    }
    for (size_t i = 0; i < character_boxes_.size(); ++i)
        character_boxes_[i]->modulate = Color(DARK_GRAY, DARK_GRAY, DARK_GRAY, 1);
    set_visible(lighting_, false);
}

/* OnFocus: the visuals at 1.1, the outline at 1, the label 6 px lower
 * (0.05 s), brighter (v 1.2); the room shows the option's description. */
void RestSiteScreen::focus(int i) {
    if (i == focus_ || i < 0 || i >= (int)options_.size()) return;
    if (focus_ >= 0) unfocus(focus_);
    focus_ = i;
    Option& o = options_[(size_t)i];
    if (o.visuals) tweens_.tween(&o.visuals->scale, Vec2(1.1f, 1.1f), 0.05f);
    if (o.outline) tweens_.tween(&o.outline->scale, Vec2(1, 1), 0.05f);
    if (o.label) {
        tweens_.tween(&o.label->offset[SIDE_TOP], o.label_top + 6, 0.05f);
        tweens_.tween(&o.label->offset[SIDE_BOTTOM], o.label_bottom + 6, 0.05f);
    }
    if (o.icon && o.enabled) {
        tweens_.kill(&o.icon->shader_params["v"]);
        o.icon->shader_params["v"] = 1.2f;
    }
    /* SetText. */
    if (description_) {
        tweens_.kill(&description_->modulate.a);
        description_->modulate = Color(1, 1, 1, 1);
        set_label(description_, o.description);
    }
}

/* OnUnfocus: back over a second (Expo Out); the description fades. */
void RestSiteScreen::unfocus(int i) {
    Option& o = options_[(size_t)i];
    if (o.visuals) tweens_.tween(&o.visuals->scale, Vec2(1, 1), 1.0f, TRANS_EXPO, EASE_OUT);
    if (o.outline) tweens_.tween(&o.outline->scale, Vec2(0.9f, 0.9f), 1.0f, TRANS_EXPO, EASE_OUT);
    if (o.label) {
        tweens_.tween(&o.label->offset[SIDE_TOP], o.label_top, 1.0f, TRANS_EXPO, EASE_OUT);
        tweens_.tween(&o.label->offset[SIDE_BOTTOM], o.label_bottom, 1.0f, TRANS_EXPO, EASE_OUT);
    }
    if (o.icon && o.enabled) tweens_.tween_from(&o.icon->shader_params["v"], 1.2f, 1.0f, 1.0f, TRANS_EXPO, EASE_OUT);
    if (description_) tweens_.tween_from(&description_->modulate.a, 1.0f, 0.0f, 1.0f, TRANS_EXPO, EASE_OUT);
}

std::string RestSiteScreen::input(const PadState& pad) {
    if (passive_) return std::string();
    if (done_) {
        if ((pad.pressed & PadState::Y) && proceed_.enabled()) return protocol_action("leave_room");
        return std::string();
    }
    /* the proceed arrow beside options left (Miniature Tent) */
    if ((pad.pressed & PadState::Y) && proceed_.enabled() && !chosen_) return protocol_action("leave_room");
    if (chosen_ || options_.empty()) return std::string();
    int n = (int)options_.size();
    /* Focus neighbours wrap left / right. NRestSiteButton plays NButton's
     * sounds; the proceed button (Y) none. */
    int was = focus_;
    if (pad.pressed & PadState::LEFT) focus((focus_ + n - 1) % n);
    if (pad.pressed & PadState::RIGHT) focus((focus_ + 1) % n);
    if (focus_ != was && lib_->audio()) lib_->audio()->ui("event:/sfx/ui/clicks/ui_hover");
    if ((pad.pressed & PadState::A) && focus_ >= 0) {
        Option& o = options_[(size_t)focus_];
        if (!o.enabled) return std::string(); /* _isUnclickable */
        if (lib_->audio()) lib_->audio()->ui("event:/sfx/ui/clicks/ui_click");
        /* OnPress: the button sinks (0.9); the options disabled while the
         * choice runs. */
        if (o.visuals) tweens_.tween(&o.visuals->scale, Vec2(0.9f, 0.9f), 1.0f, TRANS_EXPO, EASE_OUT);
        if (o.outline) tweens_.tween(&o.outline->scale, Vec2(0.9f, 0.9f), 1.0f, TRANS_EXPO, EASE_OUT);
        chosen_ = true;
        return protocol_action("choose_option", format("\"option_index\":%d", o.index));
    }
    return std::string();
}

/* HealRestSiteOption: PlayRestSiteHealSfx, NRestSmokeVfx (its clouds at
 * the screen's middle), NDesaturateTransitionVfx (the world environment
 * to brightness 0.1 in 1 s, held 1 s, back in 1 s: here the room's
 * colour). SmithRestSiteOption: NCardSmithVfx over the screens. */
double RestSiteScreen::post_select(Node* room, const Json& steps) {
    const Json* cue = NULL;
    for (size_t i = 0; i < steps.size(); ++i)
        for (size_t k = 0; k < steps[i].size(); ++k)
            if (steps[i][k]["t"].str() == "rest_vfx") {
                cue = &steps[i][k];
                CueLedger::used(*cue);
            }
    if (!cue) return 0;
    const std::string& option = (*cue)["option"].str();
    if (option == "HEAL") {
        if (lib_->audio()) {
            /* sleep.tres: an AudioStreamRandomizer of the three jingles */
            lib_->audio()->ui(format("STS_SleepJingle_1%c_NewMix_v1.mp3", "abc"[chaotic_rand() % 3]));
            lib_->audio()->ui("SOTE_SFX_SleepBlanket_v1.mp3");
        }
        smoke_ = instance(lib_, "scenes/vfx/rest_smoke_vfx.tscn");
        if (smoke_) {
            if (smoke_->is_node2d()) ((Node2D*)smoke_)->position = Vec2(960, 540);
            Node* clouds = smoke_->get_node("Clouds");
            if (clouds && (clouds->type == "GPUParticles2D" || clouds->type == "CPUParticles2D")) ((Particles2D*)clouds)->emitting = true;
            room->add_child(smoke_);
        }
        if (room->is_canvas_item()) {
            CanvasItem* r = (CanvasItem*)room;
            tween_color(tweens_, &r->modulate, Color(0.1f, 0.1f, 0.1f, 1), 1.0f);
            tween_color(tweens_, &r->modulate, Color(1, 1, 1, 1), 1.0f, TRANS_LINEAR, EASE_IN_OUT, 2.0f);
        }
    } else if (option == "KINDLE" && !character_boxes_.empty() && !character_boxes_[0]->children.empty()) {
        /* KindleRestSiteOption.PlayKindleVfx: the character shakes, Pumpkin
         * Candle's flash on it (its fire sound is a cue of its own) */
        Node* who = character_boxes_[0]->children[0];
        if (who->is_node2d()) {
            Node* shake = SceneLibrary::create_node("Node");
            shake->behavior = new RestShake((Node2D*)who);
            room->add_child(shake);
        }
        Node* fx = instance(lib_, "scenes/vfx/relic_flash_vfx.tscn");
        if (fx && fx->is_control()) {
            for (int k = 1; k <= 3; ++k) {
                Node* img = fx->get_node(format("Image%d", k));
                set_relic_icon(lib_, img, "pumpkin_candle");
                if (img && img->is_canvas_item()) ((CanvasItem*)img)->modulate.a = 0;
            }
            Control* c = (Control*)fx;
            for (int s = 0; s < 4; ++s) c->anchor[s] = 0;
            c->set_position(Vec2(0, 0));
            fx->behavior = new RestRelicFlash(Vec2(0, 0));
            who->add_child(fx);
        } else {
            delete fx;
        }
    } else if (option == "SMITH" && DeckPreviews::current()) {
        Json c = *cue;
        c.set("op", Json::make_string("smith"));
        DeckPreviews::current()->play_cue(c, decision_["player"]["character"].str());
    }
    return (*cue)["wait"].num(0);
}

void RestSiteScreen::frame(double dt) {
    t_ += dt;
    tweens_.update(dt);
    proceed_.frame(dt);
    if (proceed_at_ >= 0 && t_ >= proceed_at_) {
        proceed_at_ = -1;
        proceed_.enable();
    }
    if (again_at_ >= 0 && t_ >= again_at_) {
        /* UpdateRestSiteOptions, ShowProceedButton, ShowChoices (0.5 s
         * Cubic Out), the first option focused */
        again_at_ = -1;
        for (size_t i = 0; i < options_.size(); ++i) options_[i].node->queue_free();
        options_.clear();
        focus_ = -1;
        make_options(room_, decision_["options"], false);
        proceed_.enable();
        if (choices_screen_) {
            tweens_.kill(&choices_screen_->modulate.a);
            tweens_.tween(&choices_screen_->modulate.a, 1.0f, 0.5f, TRANS_CUBIC, EASE_OUT);
        }
        if (!options_.empty()) focus(0);
        chosen_ = false;
    }
    if (smoke_ && t_ >= 4.0) {
        smoke_->queue_free();
        smoke_ = NULL;
    }
}

} /* namespace spire */

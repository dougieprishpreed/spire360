#include "rest_site_screen.h"
#include "loc.h"

#include "../scene/controls.h"
#include "../scene/node.h"
#include "../scene/scene.h"
#include "../scene/spine_sprite.h"

namespace spire {

/* Colors.DarkGray: the characters once the fire is out. */
static const float DARK_GRAY = 0.6627451f;

static Control* as_control(Node* n) { return n && n->is_control() ? (Control*)n : NULL; }

RestSiteScreen::RestSiteScreen(SceneLibrary* lib)
    : lib_(lib), choices_screen_(NULL), description_(NULL), lighting_(NULL), focus_(-1), start_focus_(0),
      done_(false), passive_(false), chosen_(false) {}

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
    focus_ = -1;
    chosen_ = false;
    Node* room = instance(lib_, "scenes/rooms/rest_site_room.tscn");
    if (!room) {
        if (error) *error = "no rest_site_room.tscn";
        return NULL;
    }
    const Json& options = d["options"];
    done_ = d["done"].b() || options.size() == 0;
    bool after_choice = done_ && before_["options"].size() > 0;

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

    if (!done_) {
        make_options(room, options, true);
        if (!passive_ && !options_.empty()) {
            int f = start_focus_ < 0 ? 0 : start_focus_;
            focus(f >= (int)options_.size() ? (int)options_.size() - 1 : f);
        }
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
        if (!passive_) proceed_.enable(!after_choice);
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
    if (chosen_ || options_.empty()) return std::string();
    int n = (int)options_.size();
    /* Focus neighbours wrap left / right. */
    if (pad.pressed & PadState::LEFT) focus((focus_ + n - 1) % n);
    if (pad.pressed & PadState::RIGHT) focus((focus_ + 1) % n);
    if ((pad.pressed & PadState::A) && focus_ >= 0) {
        Option& o = options_[(size_t)focus_];
        if (!o.enabled) return std::string(); /* _isUnclickable */
        /* OnPress: the button sinks (0.9); the options disabled while the
         * choice runs. */
        if (o.visuals) tweens_.tween(&o.visuals->scale, Vec2(0.9f, 0.9f), 1.0f, TRANS_EXPO, EASE_OUT);
        if (o.outline) tweens_.tween(&o.outline->scale, Vec2(0.9f, 0.9f), 1.0f, TRANS_EXPO, EASE_OUT);
        chosen_ = true;
        return protocol_action("choose_option", format("\"option_index\":%d", o.index));
    }
    return std::string();
}

void RestSiteScreen::frame(double dt) {
    tweens_.update(dt);
    proceed_.frame(dt);
}

} /* namespace spire */

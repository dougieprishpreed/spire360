#include "logo_screen.h"

#include "../audio/game_audio.h"
#include "../scene/controls.h"
#include "../scene/scene.h"
#include "../scene/spine_sprite.h"
#include "../spine/spine.h"
#include "loc.h"

namespace spire {

LogoScreen::LogoScreen(SceneLibrary* lib)
    : lib_(lib), phase_(FADE_IN), t_(0), cancelled_(false), ready_(true), bg_(NULL), container_(NULL), logo_(NULL), black_(NULL), rest_y_(0) {}

bool LogoScreen::skip_intro() { return Loc::settings_save()["skip_intro_logo"].b(); }

Node* LogoScreen::build(std::string* error) {
    Node* screen = instance(lib_, "scenes/screens/main_menu/logo_animation.tscn");
    if (!screen) {
        if (error) *error = "no logo_animation.tscn";
        return NULL;
    }
    Control* root = full_rect_control("LogoAnimation");
    root->add_child(screen);
    /* NTransition's fade, over everything */
    black_ = (ColorRect*)SceneLibrary::create_node("ColorRect");
    black_->name = "Transition";
    black_->anchor[SIDE_RIGHT] = black_->anchor[SIDE_BOTTOM] = 1;
    for (int i = 0; i < 4; ++i) black_->offset[i] = 0;
    black_->mouse_filter = 2;
    black_->color = Color(0, 0, 0, 1);
    root->add_child(black_);
    Node* bg = screen->find_unique("Bg");
    Node* c = screen->find_unique("Container");
    Node* s = c ? c->get_node("SpineSprite") : NULL;
    bg_ = bg && bg->is_control() ? (Control*)bg : NULL;
    container_ = c && c->is_control() ? (Control*)c : NULL;
    logo_ = s && s->type == "SpineSprite" ? (SpineSprite*)s : NULL;
    if (logo_) {
        logo_->visible = false;
        /* _Ready: the skeleton's bounds a third of the screen, centred */
        Rect2 b = logo_->bounds();
        if (b.size.x > 0 && b.size.y > 0) {
            float k = minf(1920.0f * 0.33f / b.size.x, 1080.0f * 0.33f / b.size.y);
            logo_->scale = Vec2(k, k);
            logo_->position = Vec2(-b.size.x * k * 0.5f, -b.size.y * k * 0.5f);
        }
        rest_y_ = logo_->position.y;
    }
    root->add_child(frame_driver(this));
    enter(FADE_IN);
    return root;
}

void LogoScreen::enter(Phase p) {
    phase_ = p;
    t_ = 0;
    switch (p) {
    case FADE_IN:
        black_->modulate.a = 1;
        tweens_.tween(&black_->modulate.a, 0.0f, 0.8f, TRANS_QUAD, EASE_OUT);
        break;
    case PLAY:
        if (!logo_ || !container_) {
            enter(FADE_OUT);
            return;
        }
        logo_->visible = true;
        logo_->set_animation("animation", false, 0);
        if (lib_->audio()) lib_->audio()->ui("SOTE_Logo_Echoing_ShortTail.mp3");
        tweens_.tween_from(&logo_->position.y, rest_y_ - 800.0f, rest_y_, 0.5f, TRANS_BACK, EASE_OUT);
        tweens_.tween(&container_->modulate.a, 1.0f, 0.5f);
        break;
    case CANCEL:
        if (container_) tweens_.tween(&container_->modulate.a, 0.0f, 0.25f, TRANS_EXPO, EASE_OUT);
        break;
    case TEAL:
        if (bg_) {
            /* modulate to #074254 */
            tweens_.tween(&bg_->modulate.r, 7 / 255.0f, 2.0f, TRANS_CUBIC, EASE_OUT);
            tweens_.tween(&bg_->modulate.g, 66 / 255.0f, 2.0f, TRANS_CUBIC, EASE_OUT);
            tweens_.tween(&bg_->modulate.b, 84 / 255.0f, 2.0f, TRANS_CUBIC, EASE_OUT);
        }
        break;
    case FADE_OUT:
        tweens_.tween(&black_->modulate.a, 1.0f, 0.8f, TRANS_QUAD, EASE_IN);
        break;
    default:
        break;
    }
}

void LogoScreen::input(const PadState& pad) {
    /* NGame._Input: select or cancel cancels the logo */
    if (pad.pressed & (PadState::A | PadState::B)) cancelled_ = true;
}

void LogoScreen::frame(double dt) {
    tweens_.update(dt);
    t_ += dt;
    switch (phase_) {
    case FADE_IN:
        if (cancelled_) {
            /* the transition's token: the fade ends, the animation does not play */
            if (ready_) enter(FADE_OUT);
        } else if (t_ >= 0.8) {
            enter(WAIT);
        }
        break;
    case WAIT:
        if (cancelled_) {
            if (ready_) enter(FADE_OUT);
        } else if (t_ >= 1.0) {
            enter(PLAY);
        }
        break;
    case PLAY: {
        SpAnimationState* st = logo_ ? logo_->animation_state() : NULL;
        if (cancelled_) enter(CANCEL);
        else if (!st || st->track_complete(0)) enter(TEAL);
        break;
    }
    case CANCEL:
        if (t_ >= 0.25 && ready_) enter(FADE_OUT);
        break;
    case TEAL:
        if (t_ >= 3.0 && ready_) enter(FADE_OUT);
        break;
    case FADE_OUT:
        if (t_ >= 0.8) phase_ = DONE;
        break;
    default:
        break;
    }
}

} /* namespace spire */

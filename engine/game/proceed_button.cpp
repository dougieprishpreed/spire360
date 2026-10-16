#include "proceed_button.h"
#include "ui_util.h"

#include "../scene/controls.h"
#include "../scene/node.h"
#include "../scene/scene.h"

#include <math.h>

namespace spire {

/* NProceedButton._showPosRatio / _hidePosRatio at 1920x1080. */
static const float SHOW_X = 1583.0f, HIDE_X = 1983.0f, POS_Y = 764.0f;
static const float WIDTH = 269.0f, HEIGHT = 108.0f;

void tween_color(TweenSet& tw, Color* c, const Color& to, float seconds, TweenTrans trans, TweenEase ease,
                 float delay) {
    tw.tween(&c->r, to.r, seconds, trans, ease, delay);
    tw.tween(&c->g, to.g, seconds, trans, ease, delay);
    tw.tween(&c->b, to.b, seconds, trans, ease, delay);
    tw.tween(&c->a, to.a, seconds, trans, ease, delay);
}

ProceedButton::ProceedButton()
    : root_(NULL), image_(NULL), outline_(NULL), enabled_(false), pulse_(true), glow_t_(0) {}

bool ProceedButton::create(SceneLibrary* lib, Node* parent) {
    Node* n = instance(lib, "scenes/ui/proceed_button.tscn");
    if (!n || !n->is_control()) {
        delete n;
        return false;
    }
    parent->add_child(n);
    return attach(lib, n);
}

bool ProceedButton::attach(SceneLibrary* lib, Node* n) {
    if (!n || !n->is_control()) return false;
    root_ = (Control*)n;
    image_ = ctl(root_, "Image");
    outline_ = ctl(root_, "Outline");
    for (int s = 0; s < 4; ++s) root_->anchor[s] = 0;
    place(root_, HIDE_X, POS_Y, WIDTH, HEIGHT);
    show_glyph(lib, root_->find_unique("HotkeyIcon"), "y");
    disable(true);
    return true;
}

void ProceedButton::set_text(const std::string& text) {
    if (root_) set_label(root_->find_unique("Label"), text);
}

void ProceedButton::move_to(float x, float seconds, bool at_once) {
    if (at_once) {
        tweens_.kill(&root_->offset[SIDE_LEFT]);
        tweens_.kill(&root_->offset[SIDE_RIGHT]);
        root_->offset[SIDE_LEFT] = x;
        root_->offset[SIDE_RIGHT] = x + WIDTH;
        return;
    }
    tweens_.tween(&root_->offset[SIDE_LEFT], x, seconds, TRANS_EXPO, EASE_OUT);
    tweens_.tween(&root_->offset[SIDE_RIGHT], x + WIDTH, seconds, TRANS_EXPO, EASE_OUT);
}

/* OnEnable: scale 1, the image from gray to white and the outline shown
 * (0.5 s), in at ShowPos (0.5 s, Expo Out); the glow loop if pulsing. */
void ProceedButton::enable(bool at_once) {
    if (!root_) return;
    enabled_ = true;
    root_->scale = Vec2(1, 1);
    move_to(SHOW_X, 0.5f, at_once);
    if (image_) {
        if (at_once) image_->self_modulate = Color(1, 1, 1, 1);
        else {
            image_->self_modulate = Color(0.5f, 0.5f, 0.5f, 1);
            tween_color(tweens_, &image_->self_modulate, Color(1, 1, 1, 1), 0.5f);
        }
    }
    if (outline_) {
        if (at_once) outline_->modulate = Color(1, 1, 1, 1);
        else tween_color(tweens_, &outline_->modulate, Color(1, 1, 1, 1), 0.5f);
    }
    glow_t_ = 0;
}

/* OnDisable: out to HidePos (0.8 s, Expo Out), image gray, outline gone. */
void ProceedButton::disable(bool at_once) {
    if (!root_) return;
    enabled_ = false;
    move_to(HIDE_X, 0.8f, at_once);
    if (image_) {
        if (at_once) image_->self_modulate = Color(0.5f, 0.5f, 0.5f, 1);
        else tween_color(tweens_, &image_->self_modulate, Color(0.5f, 0.5f, 0.5f, 1), 0.8f);
    }
    if (outline_) {
        if (at_once) outline_->modulate = Color(1, 1, 1, 0);
        else tween_color(tweens_, &outline_->modulate, Color(1, 1, 1, 0), 0.8f);
    }
}

void ProceedButton::set_pulse(bool pulsing) {
    if (pulsing == pulse_) return;
    pulse_ = pulsing;
    glow_t_ = 0;
    /* StopGlowTween: the outline's alpha to 0 in 0.5 s. */
    if (!pulsing && outline_) tweens_.tween(&outline_->self_modulate.a, 0.0f, 0.5f);
}

void ProceedButton::frame(double dt) {
    tweens_.update(dt);
    if (!root_ || !outline_ || !enabled_ || !pulse_) return;
    /* StartGlowTween: self_modulate:a to 0.25 (0.5 s), to 0.75 (0.5 s),
     * looping; from where it was the first time. */
    glow_t_ += dt;
    float a;
    if (glow_t_ < 0.5) {
        a = 0.25f * (float)(glow_t_ / 0.5); /* from 0 (at rest) */
    } else {
        double phase = fmod(glow_t_ - 0.5, 1.0);
        a = phase < 0.5 ? (float)(0.25 + phase) : (float)(0.75 - (phase - 0.5));
    }
    tweens_.kill(&outline_->self_modulate.a);
    outline_->self_modulate.a = a;
}

} /* namespace spire */

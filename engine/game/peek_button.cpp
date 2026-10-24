#include "peek_button.h"

#include <math.h>

#include "../audio/game_audio.h"
#include "../scene/node.h"
#include "../scene/controls.h"
#include "../scene/scene.h"
#include "ui_util.h"

namespace spire {

PeekButton::PeekButton()
    : lib_(NULL), node_(NULL), visuals_(NULL), flash_(NULL), enabled_(false), peeking_(false), wiggle_t_(-1) {}

void PeekButton::setup(SceneLibrary* lib, Node* button, bool enabled) {
    lib_ = lib;
    node_ = button && button->is_control() ? (Control*)button : NULL;
    visuals_ = node_ ? ctl(node_, "Visuals") : NULL;
    flash_ = node_ ? ctl(node_, "Flash") : NULL;
    targets_.clear();
    hidden_.clear();
    tweens_.clear();
    enabled_ = enabled;
    peeking_ = false;
    wiggle_t_ = -1;
    if (!node_) return;
    /* OnEnable / OnDisable: Visible */
    set_visible(node_, enabled);
    show_glyph(lib, node_->find_unique("HotkeyIcon"), "ls");
    if (visuals_) {
        visuals_->shader_params["pulse_strength"] = 0;
        visuals_->scale = Vec2(1, 1);
        visuals_->rotation = 0;
    }
}

void PeekButton::add_target(Node* target) {
    if (target) targets_.push_back(target);
}

void PeekButton::toggle() {
    if (!enabled()) return;
    if (visuals_) tweens_.tween_from(&visuals_->scale, Vec2(0.95f, 0.95f), Vec2(1, 1), 0.15f);
    set_peeking(!peeking_);
}

void PeekButton::set_peeking(bool on) {
    if (peeking_ == on) return;
    peeking_ = on;
    if (on) {
        for (size_t i = 0; i < targets_.size(); ++i) {
            Node* t = targets_[i];
            if (!t->is_canvas_item() || !((CanvasItem*)t)->visible) continue;
            hidden_.push_back(t);
            set_visible(t, false);
        }
    } else {
        for (size_t i = 0; i < hidden_.size(); ++i) set_visible(hidden_[i], true);
        hidden_.clear();
    }
    if (visuals_) visuals_->shader_params["pulse_strength"] = on ? 1.0f : 0.0f;
}

/* NPeekButton.Wiggle: the visuals shake (10 deg * sin(3t) sin(t/2), t over
 * 2 pi in 0.5 s) and swell (0.15 * sin(t) sin(t/2), t over pi in 0.25 s);
 * the flash comes in (0.1 s) and goes (0.3 s, Quad Out); deny.mp3 at half
 * volume, Medium pitch variance (NDebugAudioManager.Play). */
void PeekButton::wiggle() {
    if (!node_) return;
    if (flash_) {
        set_visible(flash_, true);
        flash_->modulate.a = 0;
        tweens_.tween(&flash_->modulate.a, 1, 0.1f);
    }
    wiggle_t_ = 0;
    if (visuals_) visuals_->rotation = 0;
    if (lib_ && lib_->audio()) {
        Json args = Json::make_array();
        args.append(Json::make_string("deny.mp3"));
        args.append(Json::make_number(0.5));
        args.append(Json::make_string("Medium"));
        Json cue;
        cue.set("t", Json::make_string("ui"));
        cue.set("call", Json::make_string("NDebugAudioManager.Play"));
        cue.set("args", args);
        lib_->audio()->cue(cue);
    }
}

void PeekButton::frame(double dt) {
    tweens_.update(dt);
    if (wiggle_t_ < 0 || !visuals_) return;
    wiggle_t_ += dt;
    const double pi = 3.14159265358979;
    double r = wiggle_t_ < 0.5 ? wiggle_t_ / 0.5 * 2.0 * pi : 2.0 * pi;
    visuals_->rotation = (float)(10.0 * sin(r * 3.0) * sin(r * 0.5) * pi / 180.0);
    double s = wiggle_t_ < 0.25 ? wiggle_t_ / 0.25 * pi : pi;
    float k = (float)(1.0 + 0.15 * sin(s) * sin(s * 0.5));
    visuals_->scale = Vec2(k, k);
    if (wiggle_t_ >= 0.5) {
        /* Chain(): the flash goes once the shake is over */
        wiggle_t_ = -1;
        if (flash_) tweens_.tween(&flash_->modulate.a, 0, 0.3f, TRANS_QUAD, EASE_OUT);
    }
}

} /* namespace spire */

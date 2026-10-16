/* The proceed arrow (NProceedButton, scenes/ui/proceed_button.tscn) the
 * rewards, rest site, treasure and shop screens share: it slides in from
 * the right when enabled and out when disabled, its outline glows in a
 * loop while it pulses ("go on"), and Y (ui_confirm) presses it. The text
 * is "Skip" while there is something left to take, "Proceed" after. */
#ifndef SPIRE_PROCEED_BUTTON_H
#define SPIRE_PROCEED_BUTTON_H

#include "../core/base.h"
#include "../scene/tween.h"

namespace spire {

class Control;
class Node;
class SceneLibrary;

class ProceedButton {
public:
    ProceedButton();
    /* Uses the scene's ProceedButton node (or a new instance under
     * `parent` when it has none); hidden off screen, disabled. */
    bool attach(SceneLibrary* lib, Node* scene_button);
    bool create(SceneLibrary* lib, Node* parent);
    void set_text(const std::string& text);
    /* OnEnable / OnDisable; at_once: no slide (a screen rebuilt). */
    void enable(bool at_once = false);
    void disable(bool at_once = false);
    bool enabled() const { return enabled_; }
    void set_pulse(bool pulsing);
    void frame(double dt);
    Control* node() const { return root_; }

private:
    void move_to(float x, float seconds, bool at_once);
    Control* root_;
    Control* image_;
    Control* outline_;
    TweenSet tweens_;
    bool enabled_, pulse_;
    double glow_t_;
};

/* Each colour channel tweened (Godot tweens a Color property as one). */
void tween_color(TweenSet& tweens, Color* c, const Color& to, float seconds, TweenTrans trans = TRANS_LINEAR,
                 TweenEase ease = EASE_IN_OUT, float delay = 0);

} /* namespace spire */

#endif

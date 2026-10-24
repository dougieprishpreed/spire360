/* NPeekButton (scenes/combat/peek_button.tscn, a selection's %PeekButton):
 * while a fight is in progress it shows (OnEnable) with the left stick's
 * glyph (MegaInput.peek). Peeking hides its targets that are visible (the
 * selection's parts and backstop), showing the fight, and pulses the
 * button (button_pulse's pulse_strength); again brings them back. */
#ifndef SPIRE_PEEK_BUTTON_H
#define SPIRE_PEEK_BUTTON_H

#include "../core/base.h"
#include "../scene/tween.h"

namespace spire {

class Control;
class Node;
class SceneLibrary;

class PeekButton {
public:
    PeekButton();
    /* The scene's button, shown when enabled (a fight in progress); the
     * targets are cleared. NULL: none. */
    void setup(SceneLibrary* lib, Node* button, bool enabled);
    void add_target(Node* target);
    bool enabled() const { return node_ != NULL && enabled_; }
    bool peeking() const { return peeking_; }
    /* The hotkey: OnPress / OnRelease (the visuals dip to 0.95 and come
     * back), then SetPeeking(!IsPeeking). */
    void toggle();
    /* SetPeeking: the visible targets hidden, or the hidden ones back. */
    void set_peeking(bool on);
    /* Wiggle: a card pressed while peeking. */
    void wiggle();
    void frame(double dt);

private:
    SceneLibrary* lib_;
    Control* node_;
    Control* visuals_; /* %Visuals */
    Control* flash_;   /* %Flash */
    std::vector<Node*> targets_, hidden_;
    bool enabled_, peeking_;
    double wiggle_t_; /* < 0: not wiggling */
    TweenSet tweens_;
};

} /* namespace spire */

#endif

/* NCreditsScreen (scenes/screens/credits_screen.tscn), from the settings:
 * the credits table's sections in their labels (two-column role / name
 * lists, the playtesters shuffled into three columns), fading in over two
 * seconds and scrolling up at 80 px a second (up / down nudge it 50 px).
 * After two seconds B or A fades it out (a second) and closes it. */
#ifndef SPIRE_CREDITS_SCREEN_H
#define SPIRE_CREDITS_SCREEN_H

#include "../scene/tween.h"
#include "run_screen.h"
#include "ui_util.h"

namespace spire {

class Control;
class SceneLibrary;

class CreditsScreen : public FrameListener {
public:
    explicit CreditsScreen(SceneLibrary* lib);
    Node* build(std::string* error);
    /* false once it has faded out (the caller frees it). */
    bool input(const PadState& pad);
    virtual void frame(double dt);

private:
    SceneLibrary* lib_;
    Control* contents_;
    double t_;
    float target_;
    bool exiting_;
    double exit_t_;
    TweenSet tweens_;
};

} /* namespace spire */

#endif

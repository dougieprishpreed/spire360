/* The rewards screen (NRewardsScreen, scenes/screens/rewards_screen.tscn)
 * from a rewards decision: the "Loot!" window listing the rewards
 * (scenes/rewards/reward_button.tscn rows: gold, potion, relic, card...)
 * over the room, and the proceed arrow ("Skip", or "Proceed" once all are
 * taken). Up / down move between rows, A takes the focused reward (a card
 * reward opens the card reward screen), Y presses the arrow.
 *
 * The client rebuilds it for every decision: after a reward is taken the
 * new one continues (no slide-in, the focus kept near where it was). */
#ifndef SPIRE_REWARDS_SCREEN_H
#define SPIRE_REWARDS_SCREEN_H

#include "../scene/tween.h"
#include "proceed_button.h"
#include "run_screen.h"
#include "ui_util.h"

namespace spire {

class Control;
class SceneLibrary;

class RewardsScreen : public RunScreen, public FrameListener {
public:
    explicit RewardsScreen(SceneLibrary* lib);
    /* What shows behind the overlay (the room); the tree takes it. */
    void set_background(Node* room) { background_ = room; }
    /* Before build: false when the screen was already up (a reward taken):
     * no slide-in; the row to focus first. */
    void set_continuing(bool animate_in, int focus) {
        animate_in_ = animate_in;
        start_focus_ = focus;
    }
    int focused() const { return focus_; }
    virtual Node* build(const Json& decision, std::string* error);
    virtual std::string input(const PadState& pad);
    virtual void frame(double dt);

private:
    struct Row {
        Control* node;
        Control* background; /* hsv v: 0.9 at rest, 1.1 focused */
        Node* label;
        Control* reticle;
        int index; /* the reward's index in the decision */
    };
    Node* make_icon(const Json& reward);
    void focus(int i);
    SceneLibrary* lib_;
    Node* background_;
    Json decision_;
    std::vector<Row> rows_;
    Control* window_;
    ProceedButton proceed_;
    int focus_, start_focus_;
    bool animate_in_, terminal_, can_skip_;
    TweenSet tweens_;
};

} /* namespace spire */

#endif

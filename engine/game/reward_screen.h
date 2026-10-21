/* The card reward (NCardRewardSelectionScreen at rest): the cards of a
 * card_reward decision in a row under the "Choose a Card" banner, the Skip
 * alternative, over the room the fight was in (dimmed by the overlays'
 * backstop). Left / right move the focus (wrapping), A takes the focused
 * card, B or Start skips (CardRewardAlternative's hotkeys: ui_cancel and
 * mega_pause_and_back). It opens from the rewards screen (RewardsScreen),
 * which comes back after it. */
#ifndef SPIRE_REWARD_SCREEN_H
#define SPIRE_REWARD_SCREEN_H

#include "../scene/tween.h"
#include "run_screen.h"
#include "ui_util.h"

namespace spire {

class Control;
class SceneLibrary;

class CardRewardScreen : public RunScreen, public FrameListener {
public:
    explicit CardRewardScreen(SceneLibrary* lib);
    /* What shows behind the overlay (the room after the fight); the tree
     * takes it. Set before build. */
    void set_background(Node* room) { background_ = room; }
    virtual Node* build(const Json& decision, std::string* error);
    virtual std::string input(const PadState& pad);
    virtual void frame(double dt);
    /* The offer's card holders (NGridCardHolder), in order */
    Control* holder(size_t i) const { return i < holders_.size() ? holders_[i] : NULL; }

private:
    void focus(int i);
    SceneLibrary* lib_;
    Node* background_;
    Json decision_;
    std::vector<Control*> holders_;
    Node* root_;
    TipFollower tips_;
    std::vector<float> scales_;
    int focus_;
    bool can_skip_;
    TweenSet tweens_;
    class InspectCardScreen* inspect_; /* InspectCard (Y) */
    Node* inspect_node_;

public:
    ~CardRewardScreen();
};

} /* namespace spire */

#endif

/* The game over screen (NGameOverScreen, scenes/screens/game_over_screen
 * .tscn) from a game_over decision, over the room the run ended in: the
 * dark red backstop fades in, the banner ("Defeat", "Victory...?") and a
 * quote; A (Continue) opens the run summary (the encounter's quote, the
 * score lines one by one), then A (Main Menu) returns to the main menu.
 *
 * Badges, the unlock score bar and the discoveries need the profile's
 * progress (not kept by the preview): not shown. */
#ifndef SPIRE_GAME_OVER_SCREEN_H
#define SPIRE_GAME_OVER_SCREEN_H

#include "../scene/tween.h"
#include "run_screen.h"
#include "ui_util.h"

namespace spire {

class Control;
class SceneLibrary;

class GameOverScreen : public RunScreen, public FrameListener {
public:
    explicit GameOverScreen(SceneLibrary* lib);
    void set_background(Node* room) { background_ = room; }
    virtual Node* build(const Json& decision, std::string* error);
    /* A on Continue / Main Menu; never a command for the logic. */
    virtual std::string input(const PadState& pad);
    virtual void frame(double dt);
    /* Main Menu was pressed. */
    bool done() const { return done_; }

private:
    enum Phase { INTRO, TITLE, SUMMARY };
    void enable_button(Control* b, bool from_below);
    void animate_quote(float delay);
    Node* score_line(const Json& line);
    SceneLibrary* lib_;
    Node* background_;
    Json decision_;
    Control* backstop_;
    Control* ui_;
    Control* banner_;
    Control* quote_;
    Control* victory_;
    Control* summary_backstop_;
    Control* summary_;
    Control* continue_;
    Control* main_menu_;
    std::vector<Control*> lines_;
    Phase phase_;
    double t_;
    int step_; /* what of the phase has happened */
    size_t lines_started_;
    bool continue_on_, menu_on_, done_;
    TweenSet tweens_;
};

} /* namespace spire */

#endif

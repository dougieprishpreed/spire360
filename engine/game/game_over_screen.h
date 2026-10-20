/* The game over screen (NGameOverScreen, scenes/screens/game_over_screen
 * .tscn) from a game_over decision, over the room the run ended in: the
 * dark red backstop fades in, the banner ("Defeat", "Victory...?") and a
 * quote; A (Continue) opens the run summary (the encounter's quote, the
 * score lines one by one), then A (Main Menu) returns to the main menu.
 *
 * With the profile's progress (a run of the game's flow: the decision's
 * "progress"), the score bar follows the lines (AnimateScoreBar): the
 * score so far towards the next unlock, the run's score added over 1 s,
 * and when the bar fills "Epoch Unlocked!" springs up and the bar starts
 * again towards the next one. Main Menu then opens the timeline when the
 * run discovered an epoch. Badges and discoveries are not shown yet. */
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
    /* The run discovered epochs: the menu opens the timeline. */
    bool open_timeline() const { return decision_["progress"]["discovered_epochs"].size() > 0; }

private:
    enum Phase { INTRO, TITLE, SUMMARY };
    void enable_button(Control* b, bool from_below);
    void animate_quote(float delay);
    void start_score_bar();
    void set_score_text(float value);
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
    Control* bar_;        /* %ScoreBar */
    Control* bar_fg_;     /* %ScoreFg */
    float bar_score_;     /* the number the bar shows */
    int bar_threshold_;
    double bar_t_;        /* time in the bar's current step */
    int bar_step_;        /* 0 not started, 1 fading in, 2 counting, 3 after an unlock, 4 done */
    /* AnimateBadges: NBadge in %BadgeContainer; AnimateInDiscoveries:
     * NDiscoveredItem in %DiscoveredContents; the d-pad focuses them, each
     * with its tip */
    std::vector<Control*> badges_;
    size_t badges_started_;
    std::vector<Control*> discoveries_;
    size_t discoveries_started_;
    double discoveries_t_; /* when they began (< 0 not yet) */
    std::vector<Control*> focusables_;
    std::vector<const Json*> focus_tips_;
    int badge_focus_;
    Control* badge_tip_;
    void focus_badge(int i);
    Node* root_;   /* the overlay */
    Node* screen_; /* game_over_screen.tscn */
    TweenSet tweens_;
};

} /* namespace spire */

#endif

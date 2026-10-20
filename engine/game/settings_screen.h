/* NSettingsScreen (scenes/screens/settings_screen.tscn) from the pause menu
 * or the main menu, with the logic's settings answer:
 *
 * - General: screen shake (a paginator), fast mode, phobia mode, the run
 *   timer, hand card numbers, long press confirmations, skip intro logo,
 *   text effects (tickboxes), Credits and Reset to Default (a
 *   confirmation first);
 * - Sound: the master, music, sound effects and ambience volumes (sliders,
 *   5% a press).
 *
 * Up / down move between the lines, left / right change a paginator or a
 * slider, A ticks or presses, LB / RB change the tab, B goes back. Each
 * change is a set_setting command; its toast shows as the game's do. The
 * Graphics and Input tabs, the language, multiplayer, background and data
 * upload lines are the PC's and stay hidden (the console is fullscreen,
 * with a pad). */
#ifndef SPIRE_SETTINGS_SCREEN_H
#define SPIRE_SETTINGS_SCREEN_H

#include "../scene/tween.h"
#include "run_screen.h"
#include "ui_util.h"

namespace spire {

class ConfirmPopup;
class CreditsScreen;
class Control;
class SceneLibrary;

class SettingsScreen : public FrameListener {
public:
    enum Action { NONE, BACK, CREDITS };
    explicit SettingsScreen(SceneLibrary* lib);
    ~SettingsScreen();
    /* settings: the logic's answer; in_run: from the pause menu. */
    Node* build(const Json& settings, std::string* error);
    /* A newer answer (after a change): the controls follow it. */
    void set_settings(const Json& settings);
    /* One frame of pad input; *cmd: a command for the logic ("" none). */
    Action input(const PadState& pad, std::string* cmd);
    virtual void frame(double dt);

private:
    enum Kind { TICKBOX, PAGINATOR, SLIDER, BUTTON };
    struct Line {
        Kind kind;
        const char* key;   /* the setting (or the button's action) */
        Control* line;     /* the MarginContainer */
        Control* control;  /* the tickbox / paginator / slider / button */
        Node* reticle;
        int tab;
    };
    void add(int tab, Node* panel, const char* line, const char* control, Kind kind, const char* key);
    void refresh();
    void focus(int i);
    void show_tab(int tab);
    void toast(const std::string& text);
    std::string change(Line& l, int step);
    SceneLibrary* lib_;
    Json settings_;
    Node* root_;
    std::vector<Line> lines_;
    int focus_; /* index into lines_ */
    int tab_;   /* 0 General, 1 Sound */
    Control* panels_[2];
    Control* boxes_[2];
    Control* tabs_[2];
    Control* toast_;
    double toast_t_;
    ConfirmPopup* confirm_;
    Node* confirm_node_;
    CreditsScreen* credits_;
    Node* credits_node_;
    TweenSet tweens_;
};

} /* namespace spire */

#endif

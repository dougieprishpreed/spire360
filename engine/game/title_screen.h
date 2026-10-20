/* The main menu (NMainMenu, scenes/screens/main_menu.tscn) of the current
 * profile, from the logic's main_menu answer (NMainMenu.RefreshButtons and
 * UpdateTimelineButtonBehavior): Continue and Abandon Run while a run is
 * saved (Singleplayer otherwise), Timeline once the profile has epochs (its
 * notification dot while one waits to be revealed; then it alone is
 * enabled), Quit. Multiplayer, Settings and the Compendium are not ported.
 *
 * Up / down move between the enabled buttons, with the gold carets beside
 * the focused one; the run info (NContinueRunInfo) rises in while Continue
 * has the focus. A presses; Start (the profile button's hotkey) opens the
 * profile screen. Disabled buttons are dimmed to a quarter. */
#ifndef SPIRE_TITLE_SCREEN_H
#define SPIRE_TITLE_SCREEN_H

#include "../scene/tween.h"
#include "run_screen.h"
#include "ui_util.h"

namespace spire {

class Control;
class SceneLibrary;

class TitleScreen : public FrameListener {
public:
    enum Action { NONE, CONTINUE, ABANDON, SINGLEPLAYER, TIMELINE, QUIT, PROFILES };
    explicit TitleScreen(SceneLibrary* lib);
    /* A new menu tree (the caller owns it); NULL without main_menu.tscn. */
    Node* build(const std::string& version, std::string* error);
    /* The profile's menu (the logic's main_menu answer); null: only
     * Singleplayer and Quit, as before a profile is read. */
    void set_menu(const Json& menu);
    Action input(const PadState& pad);
    /* While another screen is over it (character select, a run starting):
     * no input, no carets. */
    void set_covered(bool covered);
    /* A submenu is open over it (NMainMenu.OnSubmenuStackChanged and
     * EnableBackstop): the buttons and the profile button hide, the logo
     * fades out over 1 s (Cubic Out); back again when it closes. */
    void set_submenu(bool open);
    virtual void frame(double dt);
    /* Its tree was deleted (a run started): forget the nodes; build()
     * makes a new one. */
    void detach();

private:
    struct Button {
        Control* node;
        Control* label;
        Action action;
        bool enabled;
    };
    void focus(int i);
    void show_run_info(bool show);
    void place_carets();
    SceneLibrary* lib_;
    Node* menu_;
    std::vector<Button> buttons_; /* the visible ones, top to bottom */
    Control* reticle_l_;
    Control* reticle_r_;
    Control* run_info_;
    Control* profile_;
    Node* buttons_box_;
    Node* logo_;
    bool menu_known_;
    bool submenu_;
    bool has_profile_;
    float run_info_top_;
    bool run_info_shown_;
    int focus_;
    bool covered_;
    Action last_action_;
    int carets_pending_; /* frames until the carets are placed (0: none) */
    TweenSet tweens_;
};

} /* namespace spire */

#endif

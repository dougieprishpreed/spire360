/* The pause menu (NPauseMenu, scenes/screens/pause_menu/pause_menu.tscn)
 * opened with Start during a run: "Paused", then Resume, Give Up and Save
 * and Quit (Settings and Compendium are not ported; Disconnect is for
 * multiplayer clients). Up / down move the focus (NPauseMenuButton: the
 * image to s 1.1 / v 1.1 and 0.95 scale, the label gold); A presses; B or
 * Start resumes, as the back button and the pause hotkey do. */
#ifndef SPIRE_PAUSE_MENU_H
#define SPIRE_PAUSE_MENU_H

#include "../scene/tween.h"
#include "run_screen.h"
#include "ui_util.h"

namespace spire {

class Control;
class SceneLibrary;

class PauseMenu : public FrameListener {
public:
    enum Action { NONE, RESUME, GIVE_UP, SAVE_AND_QUIT, SETTINGS, COMPENDIUM };
    explicit PauseMenu(SceneLibrary* lib);
    /* A new tree (the caller owns it); give_up: the button is enabled
     * (NPauseMenu.Initialize: not once the run is over). */
    Node* build(bool give_up, std::string* error);
    Action input(const PadState& pad);
    virtual void frame(double dt);

private:
    struct Button {
        Control* node;
        Control* image;
        Control* label;
        Action action;
    };
    void focus(int i);
    void unfocus(Button& b);
    SceneLibrary* lib_;
    std::vector<Button> buttons_;
    int focus_;
    TweenSet tweens_;
};

} /* namespace spire */

#endif

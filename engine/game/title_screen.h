/* The main menu (NMainMenu, scenes/screens/main_menu.tscn) as the preview
 * has it: Singleplayer and Quit (the other buttons need screens not ported
 * yet). Up / down move between them, with the gold carets beside the
 * focused one; A on Singleplayer opens the character select screen (as the
 * game does for a profile's first run), A on Quit quits. */
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
    enum Action { NONE, SINGLEPLAYER, QUIT };
    explicit TitleScreen(SceneLibrary* lib);
    /* A new menu tree (the caller owns it); NULL without main_menu.tscn. */
    Node* build(const std::string& version, std::string* error);
    Action input(const PadState& pad);
    /* While another screen is over it (character select, a run starting):
     * no input, no carets. */
    void set_covered(bool covered);
    virtual void frame(double dt);

private:
    void focus(int i);
    SceneLibrary* lib_;
    std::vector<Control*> buttons_; /* Singleplayer, Quit */
    std::vector<Control*> labels_;
    Control* reticle_l_;
    Control* reticle_r_;
    int focus_;
    bool covered_;
    TweenSet tweens_;
};

} /* namespace spire */

#endif

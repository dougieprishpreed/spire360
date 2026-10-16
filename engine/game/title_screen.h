/* The main menu (NMainMenu, scenes/screens/main_menu.tscn) as the preview
 * has it: Singleplayer and Quit (the other buttons need screens not ported
 * yet). Up / down move between them, with the gold carets beside the
 * focused one; on Singleplayer, left / right choose the character (until
 * the character select screen is ported) and A starts the run. */
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
    enum Action { NONE, START_RUN, QUIT };
    explicit TitleScreen(SceneLibrary* lib);
    /* A new menu tree (the caller owns it); NULL without main_menu.tscn. */
    Node* build(const std::string& version, std::string* error);
    Action input(const PadState& pad);
    /* The chosen character's id for start_run ("Ironclad", ...). */
    const char* character() const;
    void set_character(const std::string& id);
    /* While the run starts (the logic's first decision takes seconds):
     * the menu stays, dimmed, without the carets, and takes no input. */
    void set_starting();
    virtual void frame(double dt);

private:
    void focus(int i);
    void refresh_label();
    SceneLibrary* lib_;
    std::vector<Control*> buttons_; /* Singleplayer, Quit */
    std::vector<Control*> labels_;
    Control* reticle_l_;
    Control* reticle_r_;
    int focus_;
    int character_;
    TweenSet tweens_;
};

} /* namespace spire */

#endif

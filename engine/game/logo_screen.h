/* NLogoAnimation (scenes/screens/main_menu/logo_animation.tscn) at start-up,
 * before the main menu (NGame.LaunchMainMenu) unless the settings skip the
 * intro logo: faded in from black (0.8 s), a second's wait, the studio
 * logo's animation dropping in (its sound with it), then the background
 * turning teal (2 s) and a second more; faded out to black (0.8 s). A or B
 * cuts the animation short (the logo fades out in 0.25 s). */
#ifndef SPIRE_LOGO_SCREEN_H
#define SPIRE_LOGO_SCREEN_H

#include "../scene/tween.h"
#include "run_screen.h"
#include "ui_util.h"

namespace spire {

class Control;
class ColorRect;
class SceneLibrary;
class SpineSprite;

class LogoScreen : public FrameListener {
public:
    explicit LogoScreen(SceneLibrary* lib);
    Node* build(std::string* error);
    void input(const PadState& pad);
    bool done() const { return phase_ == DONE; }
    /* Whether the menu can show: until then the logo (or, cancelled, its
     * background) stays instead of fading to black. */
    void set_ready(bool ready) { ready_ = ready; }
    virtual void frame(double dt);
    /* SettingsSave.SkipIntroLogo as the game reads it at start-up
     * (data/user/settings.save); false when there is none. */
    static bool skip_intro();

private:
    enum Phase { FADE_IN, WAIT, PLAY, CANCEL, TEAL, FADE_OUT, DONE };
    void enter(Phase p);
    SceneLibrary* lib_;
    Phase phase_;
    double t_;
    bool cancelled_, ready_;
    Control* bg_;
    Control* container_;
    SpineSprite* logo_;
    ColorRect* black_;
    float rest_y_;
    TweenSet tweens_;
};

} /* namespace spire */

#endif

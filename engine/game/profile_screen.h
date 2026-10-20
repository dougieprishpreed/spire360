/* The profile screen (NProfileScreen, scenes/screens/profiles/
 * profile_screen.tscn) from the main menu's profile button: the three save
 * profiles (NProfileButton: "Save Profile N", playtime and last update, or
 * "Empty"; the current one marked). Left / right move the focus (the
 * button grows to 1.03 and brightens, v 1.3), A switches to that profile
 * (or goes back on the current one), B goes back.
 *
 * Spire360 adds one option per profile under its buttons: Y turns
 * "Unlock Everything" on or off for the current profile (its runs then
 * start with every character, card, relic, event and act unlocked and any
 * ascension up to 10; progress is still recorded). Deleting profiles is
 * not ported. */
#ifndef SPIRE_PROFILE_SCREEN_H
#define SPIRE_PROFILE_SCREEN_H

#include "../core/json.h"
#include "../scene/tween.h"
#include "run_screen.h"
#include "ui_util.h"

namespace spire {

class Control;
class SceneLibrary;

class ProfileScreen : public FrameListener {
public:
    enum Action { NONE, BACK };
    explicit ProfileScreen(SceneLibrary* lib);
    Node* build(std::string* error);
    /* The logic's profiles answer. */
    void set_profiles(const Json& profiles);
    /* The current profile's menu (after a switch or a toggle). */
    void set_menu(const Json& menu);
    /* cmd: a command for the logic ("" for none). */
    Action input(const PadState& pad, std::string* cmd);
    virtual void frame(double dt);

private:
    void focus(int i);
    void refresh();
    SceneLibrary* lib_;
    Json profiles_, menu_;
    Control* buttons_[3];
    Control* option_; /* the Unlock Everything line */
    int focus_;
    TweenSet tweens_;
};

} /* namespace spire */

#endif

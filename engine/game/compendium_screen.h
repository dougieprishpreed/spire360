/* NCompendiumSubmenu (scenes/screens/compendium_submenu.tscn) from the
 * title or the pause menu: the Card Library, Relic Collection, Potion Lab
 * and Bestiary buttons, then Statistics and Run History (Leaderboards and
 * Achievements are the platform's and stay hidden); each opens its screen
 * over it. Left / right / up / down move as the submenu's focus
 * neighbours, A opens, B goes back. The screens ask the logic through
 * *cmd and get their answers through set_answer. */
#ifndef SPIRE_COMPENDIUM_SCREEN_H
#define SPIRE_COMPENDIUM_SCREEN_H

#include "../scene/tween.h"
#include "run_screen.h"
#include "ui_util.h"

namespace spire {

class CardLibraryScreen;
class CollectionScreen;
class StatsScreen;
class Control;
class SceneLibrary;

class CompendiumScreen : public FrameListener {
public:
    enum Action { NONE, BACK };
    explicit CompendiumScreen(SceneLibrary* lib);
    ~CompendiumScreen();
    /* character: the run's ("" outside a run). */
    Node* build(const std::string& character, std::string* error);
    Action input(const PadState& pad, std::string* cmd);
    /* An answer to one of its queries (library, ...). */
    void set_answer(const Json& msg);
    static bool is_answer(const std::string& type);
    virtual void frame(double dt);

private:
    struct Button {
        Control* node;
        const char* key; /* its screen */
    };
    void focus(int i);
    void close_sub();
    SceneLibrary* lib_;
    std::string character_;
    Node* root_;
    std::vector<Button> buttons_;
    int focus_;
    CardLibraryScreen* library_;
    CollectionScreen* collection_;
    StatsScreen* stats_;
    Node* sub_node_;
    TweenSet tweens_;
};

} /* namespace spire */

#endif

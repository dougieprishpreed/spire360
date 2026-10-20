/* The game outside a run, and the way into and out of one, as NGame and
 * NMainMenu run it: the main menu of the last profile (select_profile at
 * start-up, as SaveManager.InitProfileId), its submenus (character select,
 * the abandon confirmation, the timeline, the profiles) and the run itself
 * (GameClient), until the run returns to the menu (game over, Save and
 * Quit). The console and the PC tools both drive it with the pad. */
#ifndef SPIRE_MENU_FLOW_H
#define SPIRE_MENU_FLOW_H

#include "../core/json.h"
#include "run_screen.h"

namespace spire {

class CharacterSelectScreen;
class ConfirmPopup;
class GameClient;
class Node;
class ProfileScreen;
class SettingsScreen;
class SceneLibrary;
class SceneTree;
class TimelineScreen;
class TitleScreen;

class MenuFlow {
public:
    MenuFlow(SceneLibrary* lib, SceneTree* tree, GameClient* game, const std::string& version);
    ~MenuFlow();
    /* One frame: the menus' input, then the run's (GameClient::update).
     * False once Quit was chosen. */
    bool update(const PadState& pad);
    /* A run is shown (its first decision arrived). */
    bool in_run() const { return in_run_; }
    /* A run was asked for and has not shown its first screen yet. */
    bool starting() const { return starting_; }
    /* The last main menu answer (null before the logic's). */
    const Json& menu() const { return menu_; }
    /* What happened last, for the host's log ("" when nothing new). */
    std::string take_event();

private:
    enum Screen { TITLE, CHARACTERS, ABANDON, TIMELINE, PROFILES, SETTINGS };
    void handle_answer(const Json& a);
    void show_title();
    void close_screen();
    void begin_run();
    void open_timeline();
    void note(const std::string& e);
    SceneLibrary* lib_;
    SceneTree* tree_;
    GameClient* game_;
    std::string version_;
    TitleScreen* title_;
    Node* title_node_;
    CharacterSelectScreen* chars_;
    ConfirmPopup* confirm_;
    TimelineScreen* timeline_;
    ProfileScreen* profiles_;
    SettingsScreen* settings_;
    int settings_seen_;
    Node* screen_node_; /* the submenu over the title */
    Screen screen_;
    Json menu_;
    int chars_version_, decisions_seen_;
    bool starting_, in_run_, quit_, leaving_run_;
    std::string last_character_, event_;
};

} /* namespace spire */

#endif

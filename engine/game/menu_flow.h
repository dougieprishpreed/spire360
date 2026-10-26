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
class CompendiumScreen;
class SceneLibrary;
class SceneTree;
class TimelineScreen;
class TitleScreen;
class LogoScreen;

class MenuFlow {
public:
    MenuFlow(SceneLibrary* lib, SceneTree* tree, GameClient* game, const std::string& version);
    ~MenuFlow();
    /* One frame: the menus' input, then the run's (GameClient::update).
     * False once Quit was chosen. */
    bool update(const PadState& pad);
    /* A run is shown (its first decision arrived). */
    bool in_run() const { return in_run_; }
    /* The intro logo still playing (the title comes after it). */
    bool in_logo() const { return logo_ != NULL; }
    /* A run was asked for and has not shown its first screen yet. */
    bool starting() const { return starting_; }
    /* The last main menu answer (null before the logic's). */
    const Json& menu() const { return menu_; }
    /* What happened last, for the host's log ("" when nothing new). */
    std::string take_event();

private:
    enum Screen { TITLE, CHARACTERS, ABANDON, TIMELINE, PROFILES, SETTINGS, COMPENDIUM };
    void handle_answer(const Json& a);
    void show_title();
    void relocalize();
    void close_screen();
    void begin_run();
    /* CharacterSelectScreen's Embark: the run starts */
    void embark();
    void open_timeline();
    void note(const std::string& e);
    SceneLibrary* lib_;
    SceneTree* tree_;
    GameClient* game_;
    std::string version_;
    LogoScreen* logo_; /* NLogoAnimation before the title */
    Node* logo_node_;
    TitleScreen* title_;
    Node* title_node_;
    CharacterSelectScreen* chars_;
    ConfirmPopup* confirm_;
    /* NAcceptTutorialsFtue over the character select (the first Embark) */
    ConfirmPopup* tutorials_;
    Node* tutorials_node_;
    /* NAscensionSingleplayerFtue over the character select (once a
     * character has an ascension) */
    class FtuePopup* ascension_;
    Node* ascension_node_;
    void close_ascension();
    TimelineScreen* timeline_;
    ProfileScreen* profiles_;
    SettingsScreen* settings_;
    int settings_seen_, language_seen_;
    CompendiumScreen* compendium_;
    Node* screen_node_; /* the submenu over the title */
    Screen screen_;
    Json menu_;
    int chars_version_, decisions_seen_;
    /* frames the run's first screen has waited for the warm-up */
    int embark_wait_;
    bool starting_, in_run_, quit_, leaving_run_;
    /* NGame.LaunchMainMenu reads the profile before the menu shows: the
     * title waits (after the logo, on black) for the logic's first
     * main_menu answer, or its error when there is no profile. */
    bool menu_ready_, title_waiting_;
    std::string last_character_, event_;
};

} /* namespace spire */

#endif

/* The game: the logic's decisions shown as the game's screens, the
 * controller's input turned into protocol commands.
 *
 * Each decision from the logic (combat_play, map_select, card_reward, ...)
 * becomes a screen: a ported one where it exists (CombatScreen), otherwise
 * a plain list of the decision's choices. The choices (what the player can
 * do, as protocol commands) come from the decision the same way for every
 * screen, so a ported screen only changes how they are shown and picked.
 *
 * Auto-play (PC tools, tests) picks choices itself, like
 * tools/random_runs.py. */
#ifndef SPIRE_GAME_CLIENT_H
#define SPIRE_GAME_CLIENT_H

#include "../core/base.h"
#include "../core/json.h"
#include "potion_menu.h"
#include "run_screen.h"

#include <map>

namespace spire {

class CanvasItem;
class CombatScreen;
class ConfirmPopup;
class DeckPreviews;
class FtuePopup;
class LogicLink;
class PauseMenu;
class PileScreen;
class SettingsScreen;
class CompendiumScreen;
class MapScreen;
class Node;
class SceneLibrary;
class SceneTree;
class RoomTransition;

/* Something the player can do now: a label for the plain screen and the
 * protocol command. */
struct Choice {
    std::string label;
    std::string command;
    int card;   /* combat: hand index, -1 otherwise */
    int target; /* combat: enemy index, -1 otherwise */
};

class GameClient {
public:
    GameClient(SceneLibrary* lib, SceneTree* tree, LogicLink* link);
    ~GameClient();

    void start_run(const std::string& character, const std::string& seed);
    /* A run of the current profile, as the game starts one (unlocks,
     * ascension, saves): start_game_run; seed "" for the game's own. */
    void start_game_run(const std::string& character, int ascension, const std::string& seed = std::string());
    /* The profile's saved run (the main menu's Continue). */
    void continue_run();
    /* A main menu command (select_profile, main_menu, timeline, ...);
     * answer() is the logic's reply to the last one once answers() has
     * counted it (an error reply included). */
    void request(const std::string& command);
    const Json& answer() const { return answer_; }
    int answers() const { return answers_; }
    /* The answers not taken yet, oldest first (several may arrive in one
     * frame); false when there are none. */
    bool take_answer(Json* out);
    /* Asks the logic for the character select screen's data (it also loads
     * the game data, so the run starts sooner); characters() has it once
     * the answer is in (characters_version() counts answers). */
    void request_characters();
    const Json& characters() const { return characters_; }
    int characters_version() const { return characters_version_; }
    /* Takes the run's screen out of the tree (back to the main menu); a
     * new start_run follows. */
    void clear_screen();
    /* Plays by itself (tests): choices picked by a seeded generator. */
    void set_auto_play(unsigned seed) { auto_ = true; rng_ = seed ? seed : 1; }
    /* Auto play answers only the decisions before the until-th (0: all). */
    void set_auto_play_until(int until) { auto_until_ = until; }
    /* The seed of runs started from the menus ("": the game's own; tests) */
    void set_run_seed(const std::string& seed) { run_seed_ = seed; }
    const std::string& run_seed() const { return run_seed_; }

    /* One frame: reads the logic, applies input, updates the screen. */
    void update(const PadState& pad);

    /* The current decision (null before the first one). */
    const Json& decision() const { return decision_; }
    /* Increments with each new decision shown. */
    int decision_count() const { return decisions_; }
    /* Waiting for the logic's answer. */
    bool busy() const;
    bool game_over() const { return decision_["decision"].str() == "game_over"; }
    /* Back to the main menu: the game over screen's Main Menu, or Save
     * and Quit from the pause menu once the logic has let the run go. */
    bool menu_requested() const { return menu_requested_; }
    /* The pause menu or its Give Up confirmation is open. */
    bool paused() const { return pause_ != NULL; }
    /* The decision shown has no ported screen: the plain list of choices. */
    bool showing_plain_list() const { return plain_; }
    /* The logic's error for the last command sent, empty when it had none. */
    const std::string& last_error() const { return error_; }

    static std::vector<Choice> choices(const Json& d);

private:
    void send(const std::string& command);
    void show(const Json& d);
    Node* crystal_sphere_behind(const Json& sphere, const Json& before);
    Node* overlay_background(const Json& d, bool aftermath);
    void pick(size_t i);
    unsigned next_rand();
    void choose_automatically();
    void handle_input(const PadState& pad);
    void combat_input(const PadState& pad);
    Node* plain_screen(const Json& d);
    void refresh_plain_screen();
    void start_room_transition();
    void queue_or_send(const std::string& command);
    void open_pause();
    void close_pause();
    bool pause_input(const PadState& pad);
    /* NDeckViewScreen / NCardPileScreen: asked of the logic (view_pile),
     * shown over the run until closed. */
    bool piles_input(const PadState& pad);
    void open_piles(const Json& msg);
    void close_piles();
    /* NTopBarMapButton (Back): the map over the room, to look at (view_map);
     * Back or B closes it. */
    void open_map_view(const Json& msg);
public:
    /* NSettingsScreen's values (the logic's settings answer; null until
     * it came), applied as they arrive: the bus volumes
     * (NAudioManager.Set*Vol: the value squared), the screen shake. */
    const Json& settings() const { return settings_; }
    int settings_version() const { return settings_version_; }
    /* Bumped when the settings answer brings another language (Loc follows). */
    int language_version() const { return language_version_; }
    void request_settings();
    /* The profile's FTUEs not seen yet (the logic's ftues answer, asked
     * with the main menu; none in test runs). */
    bool ftue_pending(const std::string& id) const;
    /* SaveManager.MarkFtueAsComplete (the game marks a tip as it shows). */
    void ftue_seen(const std::string& id);
    /* NAcceptTutorialsFtue's answer: tutorials on (yes) or off. */
    void accept_tutorials(bool yes);
    /* The FTUE open over the run ("" none) and for how many frames. */
    std::string ftue_open() const;
    int ftue_frames() const { return ftue_frames_; }
    /* A query for the logic outside the decisions (settings): no answer
     * is waited for. */
    void send_query(const std::string& command);
    void apply_settings(const Json& s);
    void add_capstone(Node* n);
    /* NAudioManager.SetMasterVol / SetBgmVol / SetSfxVol / SetAmbienceVol:
     * the settings' volumes (0..1), squared, on the buses */
    static void apply_volumes(SceneLibrary* lib, float master, float bgm, float sfx, float ambience);
private:
    bool settings_input(const PadState& pad);
    /* An FTUE over the run (NModalContainer): it takes the input until it
     * closes. */
    void open_ftue(const std::string& id, const std::vector<CanvasItem*>& spotlights, const Vec2* at = NULL);
    void close_ftue();
    /* The FTUEs that come with a screen (combat rules, map, rest site,
     * rewards' relic, card reward's power, potion, merchant's rug). */
    bool check_ftues(const PadState& pad); /* true: the input waits for one */
    /* The FTUEs that come instead of a command (rewards skipped, the shop
     * left unopened, End Turn with cards to play): true when one did. */
    bool ftue_instead(const std::string& command);
    CanvasItem* screen_item(const char* name);
    FtuePopup* ftue_;
    Node* ftue_node_;
    int ftue_frames_;
    std::vector<std::string> ftues_, ftues_marked_;
    bool combat_rules_wait_;   /* the fight's start waits for the combat rules FTUE */
    int ftue_end_turns_;       /* turns ended with nothing to play (can_play_cards) */
    int ftue_cards_played_;    /* cards played (cannot_play_card's auto-disable) */
    int ftue_potions_;         /* the potions held at the last decision */

    SceneLibrary* lib_;
    SceneTree* tree_;
    LogicLink* link_;
    PotionMenu potions_; /* X: the top bar's potions */
    Node* screen_;
    CombatScreen* combat_; /* the combat screen's state while it is shown */
    RunScreen* ported_;    /* another ported screen while it is shown */
    CombatScreen* room_;   /* the room behind an overlay (after a fight) */
    RunScreen* behind_;    /* another screen behind an overlay (an event's) */
    Json last_combat_;     /* the last combat_play decision (its room) */
    std::map<int, Vec2> enemy_places_; /* where the fight's enemies stand */
    Json finishing_;       /* a decision after the fight's killing blow, shown once it has played */
    Json last_event_;      /* the last event_choice decision (its room) */
    Json last_rest_;       /* the last rest_site decision (its room) */
    Json last_shop_;       /* the last shop decision (its room) */
    Json last_crystal_;    /* the Crystal Sphere's decision before (its reveals animate) */
    int reward_focus_;     /* the rewards screen's focus, kept while it is up */
    bool shop_open_;       /* the shop's inventory open, and its focused slot */
    std::string shop_kind_;
    int shop_index_;
    int hand_focus_;       /* kept across decisions, as the game's _lastFocusedHolderIdx */
    int target_;           /* combat targeting: enemy index, -1 when not targeting */
    int last_target_;      /* the enemy last aimed at (combat_id; NCombatRoom.LastTargetedCreature) */
    Json decision_;
    std::vector<Choice> choices_;
    size_t focus_;
    bool waiting_, ready_, auto_;
    int auto_until_;
    std::string run_seed_;
    bool menu_requested_;
    unsigned rng_;
    int decisions_, errors_;
    std::string error_, pending_start_;
    bool want_characters_;
    Json characters_;
    int characters_version_;
    RoomTransition* transition_;
    DeckPreviews* deck_previews_; /* NRun.GlobalUi's card previews (over the screens) */
    bool event_finishing_;        /* finishing_ waits for the event's room */
    std::vector<std::string> room_vfx_; /* the event room's own effects (ui calls) */
    std::string room_vfx_event_;
    Json event_portrait_;                /* the event's portrait as a page set it */
    void collect_room_cues(const Json& decision);
    Json transition_pending_;
    std::vector<std::string> queued_; /* sent after the logic's "ready" */
    Json answer_;
    int answers_;
    std::vector<Json> unread_;
    PauseMenu* pause_;
    Node* pause_node_;
    PileScreen* piles_;
    Node* piles_node_;
    MapScreen* map_view_;
    Json settings_;
    int settings_version_, language_version_;
    SettingsScreen* settings_screen_;
    Node* settings_node_;
    CompendiumScreen* compendium_;
    Node* compendium_node_;
    unsigned piles_button_; /* the button that opened it (closes it too) */
    bool piles_asked_;
    ConfirmPopup* confirm_;
    Node* confirm_node_;
    bool quitting_; /* Save and Quit sent, waiting for the menu */
    bool plain_;    /* the screen shown is the plain list */
};

} /* namespace spire */

#endif

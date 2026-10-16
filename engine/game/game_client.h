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

#include "../core/json.h"
#include "potion_menu.h"
#include "run_screen.h"

namespace spire {

class CombatScreen;
class LogicLink;
class Node;
class SceneLibrary;
class SceneTree;

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
    /* Takes the run's screen out of the tree (back to the main menu); a
     * new start_run follows. */
    void clear_screen();
    /* Plays by itself (tests): choices picked by a seeded generator. */
    void set_auto_play(unsigned seed) { auto_ = true; rng_ = seed ? seed : 1; }

    /* One frame: reads the logic, applies input, updates the screen. */
    void update(const PadState& pad);

    /* The current decision (null before the first one). */
    const Json& decision() const { return decision_; }
    /* Increments with each new decision shown. */
    int decision_count() const { return decisions_; }
    /* Waiting for the logic's answer. */
    bool busy() const { return waiting_; }
    bool game_over() const { return decision_["decision"].str() == "game_over"; }
    /* The last error the logic reported ("" if none). */
    const std::string& last_error() const { return error_; }

    static std::vector<Choice> choices(const Json& d);

private:
    void send(const std::string& command);
    void show(const Json& d);
    void pick(size_t i);
    unsigned next_rand();
    void choose_automatically();
    void handle_input(const PadState& pad);
    void combat_input(const PadState& pad);
    Node* plain_screen(const Json& d);
    void refresh_plain_screen();

    SceneLibrary* lib_;
    SceneTree* tree_;
    LogicLink* link_;
    PotionMenu potions_; /* X: the top bar's potions */
    Node* screen_;
    CombatScreen* combat_; /* the combat screen's state while it is shown */
    RunScreen* ported_;    /* another ported screen while it is shown */
    CombatScreen* room_;   /* the room behind an overlay (after a fight) */
    Json last_combat_;     /* the last combat_play decision (its room) */
    int hand_focus_;       /* kept across decisions, as the game's _lastFocusedHolderIdx */
    int target_;           /* combat targeting: enemy index, -1 when not targeting */
    int last_target_;      /* the enemy last aimed at (combat_id; NCombatRoom.LastTargetedCreature) */
    Json decision_;
    std::vector<Choice> choices_;
    size_t focus_;
    bool waiting_, ready_, auto_;
    unsigned rng_;
    int decisions_, errors_;
    std::string error_, pending_start_;
};

} /* namespace spire */

#endif

/* The combat screen, built from a combat_play decision the way the game's
 * NCombatRoom builds it: the room scene, the background the game picked
 * (view.background: the scene plus its layers), the encounter's camera, and
 * one creature scene per combatant with its visuals
 * (scenes/creature_visuals/<id>.tscn), placed by NCombatRoom's
 * PositionEnemies / PositionPlayersAndPets (ported), with each creature's
 * hitbox, intents and state display (NCreature.UpdateBounds,
 * NCreatureStateDisplay, NHealthBar: ported, driven by the decision's hp,
 * block and powers). */
#ifndef SPIRE_COMBAT_SCREEN_H
#define SPIRE_COMBAT_SCREEN_H

#include "../core/base.h"
#include "../core/json.h"

#include <map>

namespace spire {

class Node;
class Control;
class SceneLibrary;

class CombatScreen {
public:
    explicit CombatScreen(SceneLibrary* lib);
    /* A new node tree for the decision (the caller owns it); NULL on error. */
    Node* build(const Json& decision, std::string* error);
    /* The controller's focused card in the hand (-1: the middle one); set
     * before build, or after it to move the focus (relays the hand). */
    void set_hand_focus(int index);
    int hand_focus() const { return hand_focus_; }
    int hand_size() const { return (int)holders_.size(); }
    /* The targeting reticle on an enemy (its index in the decision's
     * enemies), -1: none. */
    void show_target(int enemy);

private:
    struct Creature {
        Control* node;  /* the creature scene (NCreature) */
        Node* visuals;  /* its NCreatureVisuals */
        float width;    /* Visuals.Bounds.Size.X */
        bool player;
        Rect2 hitbox;            /* in the creature's coordinates */
        float hp_bar_reduction;  /* MonsterModel.HpBarSizeReduction */
        float max_fg_width;      /* NHealthBar._expectedMaxFgWidth */
    };
    Node* make_background(const Json& bg);
    bool make_creature(const std::string& visuals_id, bool player, Creature& out);
    void update_bounds(Creature& c);
    void setup_combat_ui(Node* root, const Json& d);
    void build_hand(Node* ui, const Json& d);
    void layout_hand();
    void refresh_health_bar(Creature& c, const Json& data, const Json& powers);
    void position_enemies(std::vector<Creature>& c, float scaling);
    void position_players(std::vector<Creature>& c, float scaling, bool fully_center);
    SceneLibrary* lib_;
    int hand_focus_;
    std::vector<Control*> holders_;   /* in the built tree (not owned) */
    std::vector<Control*> reticles_;  /* the enemies' SelectionReticle */
};

} /* namespace spire */

#endif

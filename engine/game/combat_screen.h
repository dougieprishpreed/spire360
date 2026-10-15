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
    void refresh_health_bar(Creature& c, const Json& data, const Json& powers);
    void position_enemies(std::vector<Creature>& c, float scaling);
    void position_players(std::vector<Creature>& c, float scaling, bool fully_center);
    SceneLibrary* lib_;
};

} /* namespace spire */

#endif

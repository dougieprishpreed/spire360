/* The game's scripted combat effects (MegaCrit.Sts2.Core.Nodes.Vfx.N*Vfx):
 * the logic records their Create calls (and NCombatRoom.PlaySplashVfx) as
 * ui cues, with creatures as "player" / "osty" / "eN", colours as
 * "#rrggbbaa" and enums by name. spawn_combat_vfx builds each one as its
 * Create and _Ready do; the effect's script (registered by its res:// path)
 * plays it and frees it. */
#ifndef SPIRE_COMBAT_VFX_H
#define SPIRE_COMBAT_VFX_H

#include "../core/base.h"
#include "../core/json.h"

namespace spire {

class Node;
class SceneLibrary;

/* Where a creature is on screen (NCreature): VfxSpawnPosition (%CenterPos),
 * GetTopOfHitbox / GetBottomOfHitbox (the hitbox's top and bottom middle). */
struct VfxCreature {
    Vec2 center, top, bottom;
    Rect2 hitbox; /* on screen */
    bool enemy;
    bool interactable;
    bool defect; /* the Defect's eyeline offset applies to its beams */
};

class VfxAnchors {
public:
    virtual ~VfxAnchors() {}
    virtual bool creature(const std::string& who, VfxCreature* out) = 0;
    /* VfxCmd.GetSideCenter: the middle of a side's creatures. */
    virtual bool side_center(bool enemies, Vec2* out) = 0;
    /* NCombatRoom.BackCombatVfxContainer (behind the creatures). */
    virtual Node* back_box() = 0;
    /* The creature's NCreatureVisuals, taken by an effect that keeps its
     * body (the doom kill): the screen forgets it. */
    virtual Node* take_visuals(const std::string& who) = 0;
    /* The creature's NCreatureVisuals and NCreature's position (its node's
     * origin), for the calls on its special nodes; none by default. */
    virtual Node* visuals_of(const std::string& who) {
        (void)who;
        return NULL;
    }
    virtual bool origin_of(const std::string& who, Vec2* out) {
        (void)who;
        (void)out;
        return false;
    }
};

/* A call the game made on a creature's special node (NCreature.
 * GetSpecialNode / Visuals.GetCurrentBody().GetNode: the logic names it
 * "<who>#<path>", "body:<path>" for the current body's): the node found in
 * the creature's visuals and its script's port run (the Kin Priest's beam,
 * the Decimillipede's regeneration and shake, the Myte's and Snapping
 * Jaxfruit's target, the Flyconid's spore kind). False when it is no such
 * call. */
bool special_node_call(const std::string& call, const Json& args, VfxAnchors& anchors);
/* NCreature.AnimDie's IDeathDelayer wait: true while a script in the
 * creature's visuals holds its death back (the Ceremonial Beast's death
 * particles still running). */
bool death_delayed(Node* visuals);

/* True when call is one of the scripted effects (shown or not). box:
 * NCombatRoom.CombatVfxContainer. */
bool spawn_combat_vfx(SceneLibrary* lib, Node* box, const std::string& call, const Json& args, VfxAnchors& anchors);
bool is_combat_vfx_call(const std::string& call);
/* NCreature.AnimDie's fade (MonsterModel.ShouldFadeAfterDeath), added to
 * parent (the creature's container): after delay (its death animation's
 * rest and half a second), NMonsterDeathVfx takes the creature's body
 * into its viewport and dissolves its picture. padding:
 * ExtraDeathVfxPadding. */
void spawn_monster_death(SceneLibrary* lib, Node* parent, VfxAnchors& anchors, const std::string& who, double delay,
                         const Vec2& padding);
/* CardPileCmd's exhaust of a card (an NCard, or the hand holder showing
 * one), which the effect takes and frees. quick (from the hand):
 * NCardExhaustQuickVfx over the card where it is, then smoke and embers;
 * else NCardExhaustVfx: the card moved into its viewport at its place and
 * dissolved as embers rise. ui: NCombatRoom.Ui. */
void spawn_card_exhaust(SceneLibrary* lib, Node* ui, Node* card, bool quick);

void install_combat_vfx_scripts();

} /* namespace spire */

#endif

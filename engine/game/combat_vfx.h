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

#include <vector>

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
    /* VfxCmd.GetSideCenterFloor: the mean of the side's hittable creatures'
     * hitbox bottoms in x, the lowest of them in y; none by default. */
    virtual bool side_floor(bool enemies, Vec2* out) {
        (void)enemies;
        (void)out;
        return false;
    }
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
/* NSpeechBubbleVfx.Create(line, side, position, seconds, color) in box at a
 * global position: right (DialogueSide.Right), the bubble to the speaker's
 * left (an enemy's, NMerchantButton.PlayDialogue's); color VfxColor's name
 * ("" the default). */
void speech_bubble_at(SceneLibrary* lib, Node* box, const std::string& line, const Vec2& at, bool right, double seconds,
                      const std::string& color);
/* NRollingBoulderVfx.PlayAnim's path for these targets: when the boulder
 * reaches each (its hitbox's left edge; the HitCreature signal) and when it
 * is done (all hit, then 1 s), from its start. */
void rolling_boulder_times(float damage, const std::vector<VfxCreature>& targets, std::vector<double>* hits, double* done);
bool is_combat_vfx_call(const std::string& call);
/* The node an exported NodePath names ("_overlay"), NULL if none; the
 * nodes of a NodePath array. */
Node* vfx_exported(Node* root, const char* key);
void vfx_exported_all(Node* root, const char* key, std::vector<Node*>& out);
/* GpuParticles2D.Restart, or NParticlesContainer.Restart (its _particles). */
void vfx_restart(Node* n);
/* NParticlesContainer.SetEmitting (or one particle system's). */
void vfx_emitting(Node* n, bool on);
/* NDamageNumVfx.Create(globalPosition, damage) (scenes/vfx/vfx_damage_num.tscn)
 * at a position in its parent's (screen) coordinates: thrown up (gravity
 * 2000 px/s2), red to cream, shrinking from 2.5, fading out over 2 s, then
 * freed. NULL without the scene. */
Node* make_damage_number(SceneLibrary* lib, const Vec2& at, int amount);
/* NCreature.AnimDie's fade (MonsterModel.ShouldFadeAfterDeath), added to
 * parent (the creature's container): after delay (its death animation's
 * rest and half a second), NMonsterDeathVfx takes the creature's body
 * into its viewport and dissolves its picture. padding:
 * ExtraDeathVfxPadding. */
void spawn_monster_death(SceneLibrary* lib, Node* parent, VfxAnchors& anchors, const std::string& who, double delay,
                         const Vec2& padding);
/* NMonsterDeathVfx.Create(creatureNodes): one fade over all their bodies
 * (ReattachPower.DoFadeOutOnAllSegments, the Decimillipede's last
 * segment). */
void spawn_monster_death_group(SceneLibrary* lib, Node* parent, VfxAnchors& anchors, const std::vector<std::string>& who,
                               double delay, const std::vector<Vec2>& padding);
/* CardPileCmd's exhaust of a card (an NCard, or the hand holder showing
 * one), which the effect takes and frees. quick (from the hand):
 * NCardExhaustQuickVfx over the card where it is, then smoke and embers;
 * else NCardExhaustVfx: the card moved into its viewport at its place and
 * dissolved as embers rise. ui: NCombatRoom.Ui. */
void spawn_card_exhaust(SceneLibrary* lib, Node* ui, Node* card, bool quick);
/* NCreature.OnPowerIncreased (a power gained or raised, not silently) at
 * the creature's VfxSpawnPosition `at` (box: the combat VFX container):
 * NPowerAppliedVfx, the power's big icon growing from 0.4 to 0.8 as it
 * fades and its title rising (a buff, green) or falling (a debuff, red)
 * 100 px from 200 px above; NPowerAppliedBuffVfx / NPowerAppliedDebuffVfx
 * (vfx_buff_applied / vfx_debuff_applied) 200 px above, freed after 2 s. */
/* NCreature.OnPowerRemoved: NPowerRemovedVfx ("wears off") at the top of
 * the creature's hitbox, with the power's title and big icon. */
void spawn_power_removed(SceneLibrary* lib, Node* box, const Vec2& at, const std::string& title, const std::string& icon);
void spawn_power_applied(SceneLibrary* lib, Node* box, const Vec2& at, const std::string& title, const std::string& icon,
                         bool buff);

void install_combat_vfx_scripts();

} /* namespace spire */

#endif

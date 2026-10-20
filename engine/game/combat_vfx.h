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
};

/* True when call is one of the scripted effects (shown or not). box:
 * NCombatRoom.CombatVfxContainer. */
bool spawn_combat_vfx(SceneLibrary* lib, Node* box, const std::string& call, const Json& args, VfxAnchors& anchors);
bool is_combat_vfx_call(const std::string& call);

void install_combat_vfx_scripts();

} /* namespace spire */

#endif

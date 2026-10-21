/* NSovereignBladeVfx (scenes/vfx/sovereign_blade.tscn): the Regent's
 * Sovereign Blades orbiting the player in a fight, from the logic's blade
 * cues:
 *
 *  forge   a new blade (under the player's creature node) or a refined
 *          one: it grows with the damage (0.9 .. 2 for 0 .. 200), glows,
 *          sparks, the new one in flames
 *  orbit   where on the orbit it is (the blades share it evenly)
 *  attack  it flies at the target and slashes, then returns
 *  remove  it shrinks away (exhausted, transformed) */
#ifndef SPIRE_SOVEREIGN_BLADE_H
#define SPIRE_SOVEREIGN_BLADE_H

#include "../core/base.h"
#include "../core/json.h"

namespace spire {

class Node;
class SceneLibrary;

/* player: the player's creature node; target: the attack's target centre
 * (VfxSpawnPosition), NULL for the other cues. */
void sovereign_blade_cue(SceneLibrary* lib, Node* player, const Json& cue, const Vec2* target);

void install_sovereign_blade_script();

} /* namespace spire */

#endif

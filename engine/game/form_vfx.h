/* The powers' forms (NFormVfx: Demon, Void, Serpent, Reaper, Echo Form) in
 * the creature visuals' %FormVfx holder. The logic records their Create and
 * the calls on the power's form ("player#form/NDemonFormVfx"):
 *
 *  Create              the form replaces the holder's, following the
 *                      character's bone for it (NSpineSpriteBoneFollower)
 *  SetActive           Void / Echo ramp their glow up or down
 *                      (NValueRamp), their particles on or off
 *  OnEffectTriggered   Demon / Reaper / Serpent burst; the Serpent's snakes
 *                      flare and fade
 *
 * The Echo Form's copy of the character (NSpineSpriteCopier) plays the
 * body's animations as they start. */
#ifndef SPIRE_FORM_VFX_H
#define SPIRE_FORM_VFX_H

#include "../core/json.h"

#include <string>

namespace spire {

class Node;
class SceneLibrary;

/* XFormVfx.Create(creature) into visuals' %FormVfx; character: the owner's
 * ("ironclad"), for the bone; false if call is not a form's Create. */
bool form_vfx_create(SceneLibrary* lib, const std::string& call, Node* visuals, const std::string& character);
/* A call on the form in visuals' holder (NFormVfx.SetActive [bool],
 * OnEffectTriggered); type: the form's class ("NDemonFormVfx"). */
void form_vfx_call(Node* visuals, const std::string& type, const std::string& call, const Json& args);

void install_form_vfx_scripts();

} /* namespace spire */

#endif

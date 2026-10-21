/* The cues of a response outside a fight (an event, the merchant, the rest
 * site): what the game shows over the room while the logic resolves the
 * choice. The fights play their own (CombatScreen::play_steps).
 *
 *  PlayerHurtVignetteHelper.Play   the low HP border
 *  NDamageNumVfx.Create            the local player's number at a quarter
 *                                  of the screen's width, half its height
 *                                  (no interactable creature node)
 *  PlayerFullscreenHealVfx.Play    the green smoky vignette and the rising
 *                                  crosses (vfx_cross_heal_fullscreen)
 *  relic_flash                     the relic's holder in the top bar
 *                                  (NRelicInventoryHolder.DoFlash)
 *  NGame.ScreenShake / ScreenRumble / ScreenShakeTrauma
 *                                  the room's shake (its own target) */
#ifndef SPIRE_ROOM_CUES_H
#define SPIRE_ROOM_CUES_H

#include "../core/json.h"

namespace spire {

class Node;
class SceneLibrary;

/* screen: the new screen's root (the top bar already added). */
void play_room_cues(SceneLibrary* lib, Node* screen, const Json& decision);

/* An event room's own effect (EventModel.OnRoomEnter: NMirrorVfx.Create,
 * NRainVfx.Create) on the screen; false if call is none of them. */
bool add_room_vfx(SceneLibrary* lib, Node* screen, const std::string& call);

} /* namespace spire */

#endif

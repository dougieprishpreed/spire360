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

#include "../core/base.h"
#include "../core/json.h"

namespace spire {

class Node;
class SceneLibrary;

/* screen: the new screen's root (the top bar already added). */
void play_room_cues(SceneLibrary* lib, Node* screen, const Json& decision);

/* An event room's own effect (EventModel.OnRoomEnter: NMirrorVfx.Create,
 * NRainVfx.Create) on the screen; false if call is none of them. */
bool add_room_vfx(SceneLibrary* lib, Node* screen, const std::string& call);

/* NSmokyVignetteVfx.Create(tint, highlight) under parent: the vignette in
 * the tint, its highlights in the other colour, in (0.1 s) and out (1 s);
 * under_bar: below the screen's top bar (parent: the screen's root). */
void spawn_smoky_vignette(SceneLibrary* lib, Node* parent, const Color& tint, const Color& highlight,
                          bool under_bar = false);

/* NGainEpochVfx (NGame's child, over every screen): "Epoch Discovered"
 * with the epoch's portrait sliding in at the left; epoch: its id
 * ("IRONCLAD2_EPOCH"). */
void show_epoch_gain(SceneLibrary* lib, Node* parent, const std::string& epoch);
/* A response's cues over every screen (NGainEpochVfx.Create(epoch), the
 * logic giving the epoch's class name, "Ironclad2Epoch"). */
void play_global_cues(SceneLibrary* lib, Node* layer, const Json& steps);

} /* namespace spire */

#endif

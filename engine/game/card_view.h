/* A card as NCard.UpdateVisuals draws it (scenes/cards/card.tscn), from a
 * card of the logic's protocol (the hand's entries: title, type, rarity,
 * description_bbcode, portrait, frame_material, energy_icon, cost and star
 * displays and colours, glow; see logic RunDriver.AddCardView). */
#ifndef SPIRE_CARD_VIEW_H
#define SPIRE_CARD_VIEW_H

#include "../core/json.h"

namespace spire {

class Node;
class SceneLibrary;

/* A new card node (the caller owns it), NULL if card.tscn is missing. */
Node* make_card(SceneLibrary* lib, const Json& card);

} /* namespace spire */

#endif

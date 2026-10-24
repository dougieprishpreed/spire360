/* A card as NCard.UpdateVisuals draws it (scenes/cards/card.tscn), from a
 * card of the logic's protocol (the hand's entries: title, type, rarity,
 * description_bbcode, portrait, frame_material, energy_icon, cost and star
 * displays and colours, glow; see logic RunDriver.AddCardView). */
#ifndef SPIRE_CARD_VIEW_H
#define SPIRE_CARD_VIEW_H

#include "../core/json.h"

#include <string>

namespace spire {

class Node;
class SceneLibrary;

/* A new card node (the caller owns it), NULL if card.tscn is missing. */
Node* make_card(SceneLibrary* lib, const Json& card);

/* NCardHighlight.AnimShow / AnimHide in its default (cyan) colour: a card
 * picked on a selection screen. */
void set_card_highlight(Node* card, bool on);

/* NCard.ActivateRewardScreenGlow (the card reward and choose-a-card screens,
 * the timeline's card unlocks): a rare card sparkles with NCardRareGlow
 * behind it and highlights gold; an uncommon one has NCardUncommonGlow. */
void card_reward_glow(SceneLibrary* lib, Node* card, const std::string& rarity);

} /* namespace spire */

#endif

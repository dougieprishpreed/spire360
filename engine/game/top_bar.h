/* The run's top bar (NTopBar, scenes/ui/top_bar.tscn) and the relic row
 * under it (NRelicInventory), at rest, from a decision's player summary
 * and run context: portrait and ascension, HP, gold, potion slots, the
 * room / floor / boss icons, the map, deck and pause buttons with their
 * pad glyphs. Shown over every screen of a run. */
#ifndef SPIRE_TOP_BAR_H
#define SPIRE_TOP_BAR_H

#include "../core/json.h"

namespace spire {

class Node;
class SceneLibrary;

/* A new node (the caller owns it) holding the bar and the relic row, NULL
 * if top_bar.tscn is missing. in_map_room: the act-start map (the map
 * button is disabled there). */
Node* make_top_bar(SceneLibrary* lib, const Json& decision, bool in_map_room);

} /* namespace spire */

#endif

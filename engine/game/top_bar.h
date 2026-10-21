/* The run's top bar (NTopBar, scenes/ui/top_bar.tscn) and the relic row
 * under it (NRelicInventory), at rest, from a decision's player summary
 * and run context: portrait and ascension, HP, gold, potion slots, the
 * room / floor / boss icons, the map, deck and pause buttons with their
 * pad glyphs. Shown over every screen of a run. */
#ifndef SPIRE_TOP_BAR_H
#define SPIRE_TOP_BAR_H

#include "../core/json.h"

#include <string>

namespace spire {

class Control;
class Node;
class SceneLibrary;

/* A new node (the caller owns it) holding the bar and the relic row, NULL
 * if top_bar.tscn is missing. in_map_room: the act-start map (the map
 * button is disabled there). before: the player as the decision before
 * had them (null for none): the potions gained or used since animate. */
Node* make_top_bar(SceneLibrary* lib, const Json& decision, bool in_map_room, const Json& before = Json());
/* NPotion (scenes/potions/potion.tscn, 60x60, pivot at its centre) showing
 * a potion's atlas image and outline; NULL without the scene. */
Node* make_potion(SceneLibrary* lib, const std::string& potion_id);
/* A relic's atlas icon and outline on relic.tscn's %Icon (or any
 * TextureRect with an %Outline child). */
void set_relic_icon(SceneLibrary* lib, Node* icon, const std::string& relic_id);
/* NRelicInventoryHolder.DoFlash: relic_inventory_flash_vfx.tscn over a
 * relic holder (relic.tscn) with its icon as the particles, freeing itself
 * after 3 s; position in the holder's canvas. NULL without the scene. */
Node* make_relic_holder_flash(SceneLibrary* lib, Control* holder);
/* A relic's icon name from the logic's relic: its icon_key ("icon_base":
 * RelicModel.IconBaseName), else its id. */
std::string relic_icon_base(const Json& relic, const char* icon_key = "icon_base", const char* id_key = "id");

} /* namespace spire */

#endif

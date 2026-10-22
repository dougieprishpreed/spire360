/* NRun.GlobalUi's card previews: the deck_card cues of a response (cards
 * added to the deck, removed, upgraded, transformed or enchanted outside a
 * fight) shown in the preview containers, then flying into the top bar's
 * deck.
 *
 *  CardPreviewContainer       a row through the middle, 325 px apart
 *                             (NCardPreviewContainer)
 *  GridCardPreviewContainer   rows of cards 25 px apart
 *  EventCardPreviewContainer  the same over the event's left side
 *                             (NGridCardPreviewContainer)
 *  MessyCardPreviewContainer  a Poisson-disc scatter, 150 px apart
 *                             (NMessyCardPreviewContainer)
 *
 *  add / show   CardCmd.PreviewInternal: the card grows in (0.25 s), then
 *               after its time NCardFlyVfx takes it to the deck
 *  remove       CardPileCmd.RemoveFromDeck: grows in, NCardRemoveVfx burns
 *               it away after 0.25 s, gone 0.4 s later
 *  upgrade      NCardUpgradeVfx: grows in with its sparkles, flies after
 *               1.75 s
 *  transform    NCardTransformVfx: the original grows in; after 0.75 s
 *               NCardTransformShineVfx turns it into the new card; it flies
 *               0.8 s after the shine
 *  enchant      NCardEnchantVfx: the enchanted card with the enchantment's
 *               sparkles, flying after 2 s
 *
 * The layer outlives the screens: the next page or room comes at once,
 * the previews play on over it. */
#ifndef SPIRE_DECK_PREVIEWS_H
#define SPIRE_DECK_PREVIEWS_H

#include "../core/json.h"

#include <string>
#include <vector>

namespace spire {

class Node;
class SceneLibrary;

class DeckPreviews {
public:
    explicit DeckPreviews(SceneLibrary* lib);
    ~DeckPreviews();
    /* The layer (a new node the scene tree owns once the caller adds it, over
     * the screens). */
    Node* layer();
    /* A response's deck_card cues; character: the run's (its card trail).
     * The ones to a combat pile ("pile": draw, discard, exhaust) wait for
     * the fight's timeline (play_cue). */
    void play(const Json& steps, const std::string& character);
    /* One deck_card cue now: a card into a combat pile flies to that pile's
     * button (PileType.GetTargetPosition). */
    void play_cue(const Json& cue, const std::string& character);
    /* The run's previews (the client's), NULL before. */
    static DeckPreviews* current();
    /* Ends every preview (leaving the run). */
    void clear();

private:
    SceneLibrary* lib_;
    Node* layer_;
};

/* The card VFX scripts the previews use (NCardRemoveVfx,
 * NCardTransformShineVfx). */
void install_deck_preview_scripts();

} /* namespace spire */

#endif

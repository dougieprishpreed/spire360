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
class Texture2DRes;

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
    /* NGlobalUi.ReparentCard after a card reward's pick: that card, from
     * where it was shown (global x, y and scale), into the deck at once. */
    void fly_to_deck(const Json& card, float x, float y, float scale, const std::string& character);
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

class CanvasItem;
class Control;
class ScriptBehavior;
class TweenSet;

/* NCardTransformShineVfx on a card in the hand (CardCmd.Transform's): the
 * overlay comes up over the old card as it swells
 * (_anticipationScaleCurve); then the new card's node takes its place
 * (move_to), the overlay goes, the border glows out and the particles
 * burst (_revealScaleCurve). */
class CardShine {
public:
    CardShine(SceneLibrary* lib, Node* card);
    ~CardShine();
    /* dt on; *scale the card's (by the curves). 1 once: the new card is due
     * (move_to it); 2: done (delete it); else 0. */
    int step(float dt, float* scale);
    /* The shine onto the new card's node. */
    void move_to(Node* card);
    /* Its node went with the card it was on: not freed again. */
    void release() { shine_ = NULL; }
    /* the card node it is on */
    Node* card() const { return card_; }

private:
    CardShine(const CardShine&);
    CardShine& operator=(const CardShine&);
    TweenSet* tweens_;
    Node* card_;
    Node* shine_;
    ScriptBehavior* data_; /* NCardTransformShineVfx's */
    CanvasItem* overlay_;
    Control* glow_;
    int stage_; /* 1 overlay in, 2 revealed, 3 done */
    float t_, swap_at_;
    bool shined_;
};
/* NCard.FlashRelicOnCard: NRelicFlashVfx at twice its size in the card's
 * middle (its CardContainer), rising as its three icons grow and fade;
 * icon: the relic's (NULL: none). */
void flash_relic_on_card(SceneLibrary* lib, Node* card, Texture2DRes* icon);

} /* namespace spire */

#endif

/* Looking at a pile of cards over the run (the logic's view_pile answer):
 *
 * - the deck (NDeckViewScreen, LB): the cards in a scrolling grid
 *   (NCardGrid) in the order obtained, sorted by the four sorters above it
 *   (UP from the top row reaches them; A turns one; the last turned leads),
 *   View Upgrades (Y) showing each card upgraded;
 * - the draw, discard and exhaust piles in combat (NCardPileScreen, LT, RT,
 *   RB), the draw pile by rarity so its order stays hidden.
 *
 * The pile's info text at the bottom. B (or the button that opened it)
 * closes it. Nothing reaches the logic. */
#ifndef SPIRE_PILE_SCREEN_H
#define SPIRE_PILE_SCREEN_H

#include "../scene/tween.h"
#include "run_screen.h"
#include "ui_util.h"

namespace spire {

class Control;
class SceneLibrary;
class TextureRect;

class PileScreen : public FrameListener {
public:
    explicit PileScreen(SceneLibrary* lib);
    ~PileScreen();
    /* msg: {"type":"pile","pile":...,"cards":[...],"info":...}; the tree
     * takes the node. */
    Node* build(const Json& msg, std::string* error);
    /* One frame of pad input; false: it closes (close_buttons were
     * pressed, or B). */
    bool input(const PadState& pad, unsigned close_buttons);
    virtual void frame(double dt);
    const std::string& pile() const { return pile_; }

private:
    enum Sort { OBTAINED, TYPE, COST, ALPHABET, SORTS };
    struct Sorter {
        Control* node;
        TextureRect* arrow;
        Node* reticle;
        bool descending;
    };
    void layout();
    void make_cards();
    void focus(int i);
    void focus_sorter(int i);
    void sort();
    int compare(int a, int b, int key) const;

    SceneLibrary* lib_;
    Json msg_;
    std::string pile_;
    bool deck_;
    bool upgrades_;  /* View Upgrades */
    Node* root_;
    Control* rows_;
    std::vector<Control*> holders_; /* by card */
    std::vector<int> order_;        /* the cards in the order shown */
    int focus_;                     /* in order_; -1: none */
    int sorter_focus_;              /* -1: the grid has the focus */
    Sorter sorters_[SORTS];
    std::vector<int> priority_; /* NDeckViewScreen._sortingPriority (key * 2 + descending) */
    int columns_;
    float top_;
    float scroll_, scroll_to_, lim_lo_, lim_hi_, grid_h_;
    Node* tickbox_;
    class InspectCardScreen* inspect_; /* NCardsViewScreen.ShowCardDetail */
    Node* inspect_node_;
    TipFollower tips_;
    TweenSet tweens_;
};

} /* namespace spire */

#endif

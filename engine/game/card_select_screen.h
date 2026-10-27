/* Choosing cards (card_select decisions), with the game's screen for the
 * selection's kind:
 *
 * - upgrade, transform, enchant, remove and other deck choices
 *   (NDeckUpgradeSelectScreen, NDeckTransformSelectScreen, ...): the cards
 *   in a scrolling grid (NCardGrid), the prompt at the bottom. A picks or
 *   unpicks the focused card; at the selection's size a preview opens (the
 *   upgrade shows the card before and after), where A or Y confirms and B
 *   goes back. With a range to choose from, Y confirms early; B closes
 *   when the selection can be cancelled.
 * - combat pile and simple grids (NCombatPileCardSelectScreen,
 *   NSimpleCardSelectScreen): the same grid, done at the selection's size.
 * - choose a card (NChooseACardSelectionScreen): a row of cards under
 *   "Choose a Card"; A takes the focused one, B skips when allowed.
 * - the hand (NPlayerHand's selection mode in the game) shows as a grid for
 *   now. */
#ifndef SPIRE_CARD_SELECT_SCREEN_H
#define SPIRE_CARD_SELECT_SCREEN_H

#include "../scene/tween.h"

#include <map>
#include "peek_button.h"
#include "run_screen.h"
#include "ui_util.h"

namespace spire {

class Control;
class SceneLibrary;

class CardSelectScreen : public RunScreen, public FrameListener {
public:
    explicit CardSelectScreen(SceneLibrary* lib);
    /* What shows behind (the fight, for in-combat choices); the tree takes
     * it. Set before build. */
    void set_background(Node* room) { background_ = room; }
    virtual Node* build(const Json& decision, std::string* error);
    virtual std::string input(const PadState& pad);
    virtual bool own_ui_sounds() const { return true; }
    virtual void frame(double dt);

private:
    struct Button {
        Control* node;
        float x, y;         /* its place shown; hidden 180 px out */
        float hide_dx;
        bool shown;
    };
    Node* build_row(const Json& d);
    void focus(int i);
    void toggle(int i);
    void open_preview();
    void close_preview();
    std::string confirm();
    void show_button(Button& b, bool shown, bool at_once = false);
    Button button(Node* n, bool confirm);
    /* each button's place, worked out once (a preview's buttons come
     * again each time it opens, already moved) */
    std::map<Control*, Button> buttons_;

    SceneLibrary* lib_;
    Node* background_;
    Json decision_;
    std::string kind_;
    bool row_; /* choose a card: one row, no grid */
    int min_, max_;
    bool cancelable_;
    bool manual_confirm_;
    std::vector<Control*> holders_;
    Node* root_;
    TipFollower tips_;
    std::vector<Node*> cards_;
    std::vector<bool> picked_;
    std::vector<int> order_; /* picked, in the order picked */
    int focus_;
    int columns_;
    Control* rows_;  /* the grid's cards, moved to scroll */
    float scroll_, scroll_to_, lim_lo_, lim_hi_, grid_h_;
    bool preview_;
    Control* preview_box_; /* the open preview's container */
    Control* single_box_;
    Control* multi_box_;
    Button close_, confirm_, preview_cancel_, preview_confirm_;
    PeekButton peek_; /* in a fight (MegaInput.peek) */
    TweenSet tweens_;
    class InspectCardScreen* inspect_; /* OpenPreviewScreen (Y on the row) */
    Node* inspect_node_;

public:
    ~CardSelectScreen();
};

} /* namespace spire */

#endif

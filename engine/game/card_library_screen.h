/* NCardLibrary (scenes/screens/card_library/card_library.tscn) from the
 * compendium, with the logic's library answers (one card pool each): the
 * sidebar's pool filters (one at a time; the characters' as unlocked),
 * card type, rarity and cost tickboxes (none ticked: all), the four sorters
 * (the last turned leads; locked cards last) and View Upgrades; the grid of
 * cards (NCardLibraryGrid: unseen and locked ones as such) with the count.
 *
 * Left from the grid's first column reaches the sidebar, right goes back;
 * up / down move in either; A selects, ticks or turns; Y views upgrades;
 * B goes back. The search bar (a keyboard's) and the multiplayer cards
 * and stats tickboxes are not shown. */
#ifndef SPIRE_CARD_LIBRARY_SCREEN_H
#define SPIRE_CARD_LIBRARY_SCREEN_H

#include "../scene/tween.h"
#include "run_screen.h"
#include "ui_util.h"

namespace spire {

class Control;
class InspectCardScreen;
class SceneLibrary;

class CardLibraryScreen : public FrameListener {
public:
    enum Action { NONE, BACK };
    explicit CardLibraryScreen(SceneLibrary* lib);
    ~CardLibraryScreen();
    /* character: the run's (its pool first), "" outside a run. */
    Node* build(const std::string& character, std::string* error);
    /* The logic's library answer for a pool. */
    void set_cards(const Json& msg);
    /* *cmd: a library query when a pool is selected ("" none). */
    Action input(const PadState& pad, std::string* cmd);
    virtual void frame(double dt);
    /* The query for the pool shown first. */
    std::string first_query() const;

private:
    enum Kind { POOL, TICK, SORTER, VIEW };
    struct Item {
        Kind kind;
        Control* node;
        std::string key; /* pool name, or the filter's group:value */
        bool on;
        Node* reticle;
    };
    void add(Node* screen, const char* unique, Kind kind, const std::string& key);
    void refresh_item(Item& it);
    void rebuild();
    void layout();
    void focus_card(int i);
    void focus_item(int i);
    bool passes(const Json& c) const;
    int compare(const Json& a, const Json& b) const;

    SceneLibrary* lib_;
    Node* root_;
    std::string character_, pool_;
    Json cards_;
    std::vector<Item> items_;
    std::vector<int> priority_; /* sort * 2 + descending: 0 rarity, 1 type, 2 cost, 3 alphabet */
    bool upgrades_;
    Control* rows_;
    std::vector<Control*> holders_;
    std::vector<int> order_; /* indices into cards_ shown */
    int focus_;              /* in order_; -1: the sidebar */
    int item_focus_;
    int columns_;
    float scroll_, scroll_to_, lim_lo_, lim_hi_, view_h_;
    Node* count_label_;
    Node* no_results_;
    TipFollower tips_;
    TweenSet tweens_;
    InspectCardScreen* inspect_;
    Node* inspect_node_;
};

} /* namespace spire */

#endif

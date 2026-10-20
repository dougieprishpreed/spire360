/* NRelicCollection (scenes/screens/relic_collection) and NPotionLab
 * (scenes/screens/potion_lab) from the compendium, with the logic's
 * relic_collection / potion_lab answers: each category's header and grid
 * (the Starter and Ancient relics in subcategories, an Ancient's with its
 * icon), each entry locked, unseen or seen (its character pool's outline
 * colour), its hover tips beside it while focused. The grids' rows run on
 * through the categories as the game's focus neighbours: left / right wrap
 * in a row, up / down keep the column (or the row's last). The content
 * scrolls to the focused entry (NScrollableContainer). B goes back. */
#ifndef SPIRE_COLLECTION_SCREEN_H
#define SPIRE_COLLECTION_SCREEN_H

#include "../scene/tween.h"
#include "run_screen.h"
#include "scroll_area.h"
#include "ui_util.h"

namespace spire {

class Control;
class SceneLibrary;

class CollectionScreen : public FrameListener {
public:
    enum Kind { RELICS, POTIONS };
    enum Action { NONE, BACK };
    CollectionScreen(SceneLibrary* lib, Kind kind);
    Node* build(std::string* error);
    /* The query for its data. */
    std::string query() const;
    static bool is_answer(const std::string& type);
    void set_data(const Json& msg);
    Action input(const PadState& pad, std::string* cmd);
    virtual void frame(double dt);

private:
    struct Entry {
        Control* node;  /* the focusable entry */
        Control* shown; /* what scales on focus */
        size_t item;    /* in items_ */
    };
    void fill_grid(Node* grid, const Json& items);
    Control* make_entry(const Json& item, Control** shown);
    void focus(int row, int col);

    SceneLibrary* lib_;
    Kind kind_;
    Node* root_;
    Node* screen_;
    Control* contents_;
    Json data_;
    std::vector<const Json*> items_;
    std::vector<std::vector<Entry> > rows_;
    int row_, col_;
    bool loaded_;
    ScrollArea scroll_;
    TipFollower tips_;
    TweenSet tweens_;
};

} /* namespace spire */

#endif

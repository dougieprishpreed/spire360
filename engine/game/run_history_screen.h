/* NRunHistory (scenes/screens/run_history_screen) from the compendium, with
 * the logic's run_history answers (one run each, 0 the newest): the
 * player's icon, HP, gold, potions, floor and run time, the date, seed,
 * mode and build, the closing quote and badges, the map points of each act
 * (their NMapPointHistoryHoverTip while focused), the relics and the deck
 * (the floors they came from highlighted). LB / RB step to the older /
 * newer run (the arrows), the d-pad moves among the entries (the nearest
 * that way, as Godot's default focus), B goes back. */
#ifndef SPIRE_RUN_HISTORY_SCREEN_H
#define SPIRE_RUN_HISTORY_SCREEN_H

#include "../scene/tween.h"
#include "run_screen.h"
#include "scroll_area.h"
#include "ui_util.h"

namespace spire {

class Control;
class InspectCardScreen;
class InspectRelicScreen;
class SceneLibrary;

class RunHistoryScreen : public FrameListener {
public:
    enum Action { NONE, BACK };
    explicit RunHistoryScreen(SceneLibrary* lib);
    ~RunHistoryScreen();
    Node* build(std::string* error);
    static std::string query(int index);
    static bool is_answer(const std::string& type) { return type == "run_history"; }
    void set_data(const Json& msg);
    Action input(const PadState& pad, std::string* cmd);
    virtual void frame(double dt);

private:
    enum Kind { POINT, RELIC, CARD, BADGE, PLAYER };
    struct Entry {
        Kind kind;
        Control* node;
        Control* icon; /* the point's icon, the card image */
        Control* outline;
        Control* label;     /* a card's label box */
        float angle;        /* a point's resting rotation */
        float label_x;
        int floor;          /* a point's floor */
        const Json* data;
    };
    void clear();
    void focus(int i);
    void unfocus();
    void highlight(Entry& e, bool on, bool instant = false);
    void highlight_floor(int floor, bool on);
    Control* make_point_tip(const Json& tip);

    SceneLibrary* lib_;
    Node* root_;
    Node* screen_;
    Control* contents_;
    Json data_;
    int index_, count_;
    std::vector<Entry> entries_;
    int focus_;
    Control* tip_;
    float tip_offset_y_;
    TipFollower tips_;
    ScrollArea scroll_;
    TweenSet tweens_;
    InspectCardScreen* inspect_card_;
    InspectRelicScreen* inspect_relic_;
    Node* inspect_node_;
    void close_inspect();
};

} /* namespace spire */

#endif

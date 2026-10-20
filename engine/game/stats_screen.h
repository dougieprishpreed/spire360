/* NStatsScreen (scenes/screens/stats_screen) from the compendium, with the
 * logic's statistics answer: the Statistics tab selected (Achievements is
 * the platform's and stays locked), the overall entries (NGeneralStatsGrid)
 * and each played character's (NCharacterStats: icon, coloured name,
 * playtime, ascensions and wins, streaks). The d-pad moves the focus to the
 * nearest entry that way (Godot's default focus neighbours); a focused
 * entry grows (1.05) with its reticle and hover tip. B goes back. */
#ifndef SPIRE_STATS_SCREEN_H
#define SPIRE_STATS_SCREEN_H

#include "../scene/tween.h"
#include "run_screen.h"
#include "scroll_area.h"
#include "ui_util.h"

namespace spire {

class Control;
class SceneLibrary;

class StatsScreen : public FrameListener {
public:
    enum Action { NONE, BACK };
    explicit StatsScreen(SceneLibrary* lib);
    Node* build(std::string* error);
    static std::string query() { return "{\"cmd\":\"statistics\"}"; }
    static bool is_answer(const std::string& type) { return type == "statistics"; }
    void set_data(const Json& msg);
    Action input(const PadState& pad, std::string* cmd);
    virtual void frame(double dt);

private:
    struct Entry {
        Control* node;
        Json tip; /* null: none */
    };
    Control* make_entry(Node* parent, const Json& e);
    void focus(int i);

    SceneLibrary* lib_;
    Node* root_;
    Node* screen_;
    Json data_;
    std::vector<Entry> entries_;
    int focus_;
    Control* tip_;
    ScrollArea scroll_;
    TweenSet tweens_;
};

} /* namespace spire */

#endif

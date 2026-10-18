/* The act map (NMapScreen open to choose the next room), from a map_select
 * decision: the act's background, the paths as dotted lines, the points
 * with their icons and states, the legend; scrolled to the current row.
 * Left / right move between the rooms that can be chosen (the pulsing
 * ones), A travels.
 *
 * Not yet: the boss node's Spine silhouette, drawing
 * on and scrolling the map. */
#ifndef SPIRE_MAP_SCREEN_H
#define SPIRE_MAP_SCREEN_H

#include "../scene/tween.h"
#include "run_screen.h"
#include "ui_util.h"

namespace spire {

class Control;
class SceneLibrary;

class MapScreen : public RunScreen, public FrameListener {
public:
    explicit MapScreen(SceneLibrary* lib);
    virtual Node* build(const Json& decision, std::string* error);
    virtual std::string input(const PadState& pad);
    virtual void frame(double dt);
    bool busy() const { return opening_ < 0.45 || travel_t_ >= 0; }
    void begin_travel(int col, int row);

private:
    struct Point {
        int col, row;
        Control* node;
        Control* icon_container; /* normal points: %IconContainer (pulses) */
        Control* icon;
        Control* reticle;
        bool choice;
        float elapsed; /* the pulse's phase */
    };
    struct Placed {
        const Json* data;
        Vec2 pos, size, endpoint;
        Control* node;
    };
    void focus(int i);
    SceneLibrary* lib_;
    std::vector<Point> points_;
    std::vector<int> choices_; /* indices into points_, left to right */
    int focus_;                /* index into choices_ */
    TweenSet tweens_;
    struct Path {
        int from_col, from_row, to_col, to_row;
        std::vector<Control*> ticks;
    };
    std::vector<Path> paths_;
    std::vector<Control*> travel_ticks_;
    size_t next_tick_;
    int current_col_, current_row_;
    double opening_, travel_t_;
    Control* marker_;
    Vec2 marker_to_;
};

} /* namespace spire */

#endif

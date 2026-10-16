/* The act map (NMapScreen open to choose the next room), from a map_select
 * decision: the act's background, the paths as dotted lines, the points
 * with their icons and states, the legend; scrolled to the current row.
 * Left / right move between the rooms that can be chosen (the pulsing
 * ones), A travels.
 *
 * Not yet: the boss node's Spine silhouette, the travel animation, drawing
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
};

} /* namespace spire */

#endif

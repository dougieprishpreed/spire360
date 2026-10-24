/* The act map (NMapScreen open to choose the next room), from a map_select
 * decision: the act's background, the paths as dotted lines, the points
 * with their icons and states, the legend; scrolled to the current row.
 * Left / right move between the rooms that can be chosen (the pulsing
 * ones), A travels.
 *
 * Up / down scroll the map. RB: the drawing tools (NMapDrawButton,
 * NMapEraseButton, NMapClearButton); drawing or erasing, a cursor the D-pad
 * moves 700 px a second (NControllerMapDrawingInput), A held draws into
 * NMapDrawings (map_drawing.tscn's SubViewport at half the map's size), B
 * stops. The lines stay for the act: every map screen shows them. Not yet:
 * the boss node's Spine silhouette. */
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
    bool busy() const {
        return opening_ < 0.45 || travel_t_ >= 0 || (act_anim_ >= 0 && act_anim_ < act_delay_ + act_duration_ * 0.25);
    }
    void begin_travel(int col, int row);
    /* NMapScreen.Open: the act's map rises in unless Fast mode is on and
     * the map's FTUE was seen (set before build) */
    void set_map_ftue_seen(bool seen) { map_ftue_seen_ = seen; }
    /* The act's opening scroll over (NMapScreen.InitMapPrompt's moment) */
    bool settled() const { return !busy() && act_anim_ < 0 && fabsf(scroll_ - scroll_to_) <= 0.1f; }
    /* NMapScreen: a press during the rise ends it (DisableInputVeryBriefly) */
    void finish_opening() {
        if (act_anim_ >= 0) act_anim_ = act_delay_ + act_duration_;
    }
    /* NMapScreen._startingPointNode (the act's start: the Ancient) */
    Control* start_point() const { return start_point_; }
    /* The drawing tools or a drawing have the pad (B is theirs). */
    bool drawing_active() const { return draw_mode_ != 0 || tool_focus_ >= 0; }
    /* A new run: no lines (NMapScreen.SetMap with clearDrawings). */
    static void clear_drawings();

private:
    struct Point {
        int col, row;
        Control* node;
        Control* icon_container; /* normal points: %IconContainer (pulses) */
        Control* icon;
        Control* reticle;
        bool choice;
        float elapsed; /* the pulse's phase */
        int kind;      /* 0 normal, 1 start / ancient, 2 boss, 3 second boss */
        bool normal;   /* an NNormalMapPoint (its circle once traveled) */
        float circle_rot, circle_scale; /* NMapCircleVfx's (the logic's) */
    };
    /* NNormalMapPoint.ShowCircleVfx; NMapScreen.TravelToMapCoord's
     * NMapNodeSelectVfx */
    void add_circle(Point& p, bool animate);
    void add_select_vfx(Point& p);
    struct Placed {
        const Json* data;
        Vec2 pos, size, endpoint;
        Control* node;
    };
    void focus(int i);
    /* the drawing tools and NControllerMapDrawingInput */
    void build_drawings(Node* root, const Json& d);
    void focus_tool(int i);
    void tools_input(const PadState& pad);
    void drawing_input(const PadState& pad);
    void start_drawing(int mode);
    void stop_drawing();
    void pen(bool down);
    class Line2D* add_line(bool erase, const std::vector<Vec2>& points);
    Control* tools_;          /* %DrawingTools */
    Control* tool_icons_[3];  /* draw, erase, clear: their Icon */
    Control* tool_tip_;
    int tool_focus_;          /* -1: the map's points have the focus */
    int draw_mode_;           /* 0 none, 1 drawing, 2 erasing */
    Control* drawings_;       /* TheMap/Drawings */
    Node* draw_vp_;           /* map_drawing.tscn's DrawViewport */
    Node* session_;           /* controller_map_drawing_input.tscn */
    Control* cursor_;
    Control* cursor_icon_;
    Vec2 cursor_dir_;
    bool pen_;
    bool pen_wait_;           /* the A that started it not let go yet */
    class Line2D* line_;
    Color draw_color_;        /* Character.MapDrawingColor */
    SceneLibrary* lib_;
    std::vector<Point> points_;
    Control* start_point_;
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
    /* NMapScreen's scroll (up / down): the map's offset, eased to its
     * target, within the act's first and last rows. */
    Control* map_ctl_;
    float scroll_, scroll_to_, scroll_lo_, scroll_hi_, scroll_step_;
    /* StartOfActAnim: the map rising from 1800 to -600 (1 s delay, 3 s
     * Expo InOut); input before a quarter of it is ignored, after it ends
     * it. -1: not playing. NActBanner over it, freed at banner_end_. */
    double act_anim_, act_delay_, act_duration_; /* Fast mode: 0.5 s, 1.5 s */
    bool map_ftue_seen_;
    double travel_time_; /* TravelToMapCoord's ticks: 0.8 s, Fast mode 0.3 s */
    Node* banner_;
    double banner_end_;
    Vec2 marker_to_;
};

} /* namespace spire */

#endif

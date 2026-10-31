/* SceneTree: the viewport's nodes, laid out like Godot's (anchors,
 * containers, minimum sizes) and drawn in Godot's canvas order (tree order,
 * show_behind_parent, z_index, clip_contents, clip_children, modulate
 * inheritance). */
#ifndef SPIRE_TREE_H
#define SPIRE_TREE_H

#include "node.h"

namespace spire {

class Canvas;

class SceneTree {
public:
    SceneTree();
    ~SceneTree(); /* deletes the nodes */

    /* The design resolution (layout units) and its mapping to screen pixels
     * (Godot's canvas_items stretch: 1920x1080 -> 1280x720 is a 2/3 scale). */
    void set_viewport(const Vec2& design_size, const Xform2D& to_screen);
    Node* root() { return &root_; }
    /* A diagnostic: with a clock (ms) every node's own process and draw
     * time is added up by its type (a script's behaviour by its script);
     * log_profile logs the costliest per frame over those frames and
     * starts again. NULL: off. */
    static void set_profile(LoadClock clock);
    static void log_profile(int frames);
    /* With the profile on: the frame's costliest node types or scripts
     * (ms of process and draw since the last call, those over half a
     * millisecond) when want, for a slow frame's log; called each frame. */
    static std::string take_frame_profile(bool want);
    /* A diagnostic: render() leaving out a kind of draw (what each costs the
     * GPU; the console's profile=1), and without the warm-up's step. -1:
     * off, 0: everything drawn. */
    enum { SKIP_TEXT = 1, SKIP_PARTICLES = 2, SKIP_SPINE = 4, SKIP_BACKGROUND = 8, SKIP_SHADERS = 16, SKIP_IMAGES = 32 };
    static void set_draw_skip(int mask);

    void layout(int passes = 3);
    /* One frame of time for every node (particles, animations), dt in s
     * (real time; the hit stop slows what the nodes get). */
    void process(double dt);
    /* NHitStop (Engine.SetTimeScale): the nodes' time runs at 0.1, easing
     * back to 1 (Ease.Interpolate with trans, In) over seconds of real
     * time; a new one replaces it. */
    static void hit_stop(float seconds, int trans);
    static float time_scale() { return time_scale_; }
    void render(Canvas& c);

    /* Items not drawn, by reason ("shader res://...", ...), from the last render. */
    std::map<std::string, int> skipped;
    int items_drawn;

    struct DrawItem;

private:
    void layout_node(Node* n);
    void process_node(Node* n, double dt);
    void collect(Node* n, const Xform2D& xf, const Color& mod, int z, const Rect2* clip, MaterialRes* mat,
                 bool in_group, std::vector<DrawItem>& out);
    void draw_items(Canvas& canvas, std::vector<DrawItem>& items);
    void render_viewport(Canvas& canvas, class SubViewport* vp);
    std::vector<class SubViewport*> viewports_; /* found by collect */

    Node root_;
    Vec2 design_size_;
    static float time_scale_, hit_stop_t_, hit_stop_len_;
    static int hit_stop_trans_;
    Xform2D to_screen_;
    Canvas* canvas_; /* during render */
    int layer_;      /* during collect: the CanvasLayer the items are in (0: the default canvas) */
};

} /* namespace spire */

#endif

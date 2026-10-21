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
};

} /* namespace spire */

#endif

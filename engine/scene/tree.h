/* SceneTree: the viewport's nodes, laid out like Godot's (anchors,
 * containers, minimum sizes) and drawn in Godot's canvas order (tree order,
 * show_behind_parent, z_index, clip_contents, modulate inheritance). */
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
    void render(Canvas& c);

    /* Items not drawn, by reason ("shader res://...", ...), from the last render. */
    std::map<std::string, int> skipped;
    int items_drawn;

    struct DrawItem;

private:
    void layout_node(Node* n);
    void collect(Node* n, const Xform2D& xf, const Color& mod, int z, const Rect2* clip, MaterialRes* mat,
                 std::vector<DrawItem>& out);

    Node root_;
    Vec2 design_size_;
    Xform2D to_screen_;
};

} /* namespace spire */

#endif

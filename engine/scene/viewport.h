/* Godot's SubViewport, SubViewportContainer and ViewportTexture: a
 * SubViewport's children are not drawn with the tree; the tree renders
 * them into a texture of the viewport's own (SceneTree::render, through
 * Renderer::begin_capture) when its update mode asks, and the nodes that
 * show it (a ViewportTexture on a Sprite2D, TextureRect or particles, or a
 * SubViewportContainer) draw that texture. */
#ifndef SPIRE_VIEWPORT_H
#define SPIRE_VIEWPORT_H

#include "node.h"
#include "resource.h"

namespace spire {

class Texture;

class SubViewport : public Node {
public:
    enum { UPDATE_DISABLED, UPDATE_ONCE, UPDATE_WHEN_VISIBLE, UPDATE_WHEN_PARENT_VISIBLE, UPDATE_ALWAYS };
    SubViewport();
    virtual ~SubViewport(); /* its texture is released with the next frame */
    int width, height;     /* size: the texture's pixels (layout units where shown) */
    Vec2 size_2d_override; /* the canvas size its children use (0: the size) */
    bool transparent_bg;
    int update_mode;
    Texture* texture;      /* the last render (NULL: none yet) */
    ImageTextureRes image; /* the texture as a resource, width x height */

    /* The canvas its children draw on, in their units. */
    Vec2 canvas_size() const;
    /* Its update mode and visibility ask for a render this frame. */
    bool wants_render() const;
    /* A render finished (t: the capture, NULL: failed); UPDATE_ONCE then
     * becomes UPDATE_DISABLED, as in Godot. */
    void rendered(Texture* t);
    void set_size(int w, int h);
    virtual bool set(const std::string& key, const Json& v, const PropCtx& ctx);
};

/* Godot's SubViewportContainer: its SubViewport child's texture over its
 * rect (stretch: the viewport sized to the rect / stretch_shrink). */
class SubViewportContainer : public Control {
public:
    SubViewportContainer() : stretch(false), stretch_shrink(1) {}
    bool stretch;
    int stretch_shrink;
    SubViewport* viewport() const;
    virtual bool set(const std::string& key, const Json& v, const PropCtx& ctx);
    virtual void resized();
    virtual void draw(Canvas& c);
};

/* A ViewportTexture: the SubViewport at viewport_path from the scene's
 * root. The scene's own copy is shared by its instances; each node that
 * shows it binds a copy of its own (bind_texture). */
class ViewportTextureRes : public Texture2DRes {
public:
    ViewportTextureRes() : Texture2DRes(RES_VIEWPORT_TEXTURE, "ViewportTexture"), user(NULL), direct(NULL) {}
    std::string viewport_path;
    Node* user; /* the node it is bound to (NULL: not bound) */
    /* Viewport.GetTexture from code: that viewport (in the same tree as
     * the user, gone with it) */
    SubViewport* direct;
    SubViewport* viewport() const;
    virtual Vec2 size() const;
    virtual bool get_rect_region(const Rect2& dst, const Rect2& src, Rect2& out_dst, Rect2& out_src,
                                 ImageTextureRes** image);
};

/* A texture property's resource for node: a texture as it is, a
 * ViewportTexture bound to node (a copy the node owns); NULL otherwise. */
Texture2DRes* bind_texture(Node* node, Resource* r);

} /* namespace spire */

#endif

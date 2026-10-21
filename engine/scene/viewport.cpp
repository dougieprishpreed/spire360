#include "viewport.h"

#include "canvas.h"
#include "value.h"

namespace spire {

/* ---- SubViewport ---- */

SubViewport::SubViewport()
    : width(512), height(512), transparent_bg(false), update_mode(UPDATE_WHEN_VISIBLE), texture(NULL) {
    image.size_ = Vec2(512, 512);
    image.render_target = true;
}

SubViewport::~SubViewport() {
    if (texture) Canvas::release_later(texture);
}

Vec2 SubViewport::canvas_size() const {
    if (size_2d_override.x > 0 && size_2d_override.y > 0) return size_2d_override;
    return Vec2((float)width, (float)height);
}

bool SubViewport::wants_render() const {
    if (width <= 0 || height <= 0) return false;
    switch (update_mode) {
    case UPDATE_ONCE:
    case UPDATE_ALWAYS:
        return true;
    case UPDATE_WHEN_VISIBLE:
    case UPDATE_WHEN_PARENT_VISIBLE: {
        /* Shown: its parent is visible (or not a canvas item). */
        const Node* p = parent;
        return !p || !p->is_canvas_item() || ((const CanvasItem*)p)->is_visible_in_tree();
    }
    default:
        return false;
    }
}

void SubViewport::rendered(Texture* t) {
    texture = t;
    image.rendered = t;
    image.size_ = Vec2((float)width, (float)height);
    if (update_mode == UPDATE_ONCE) update_mode = UPDATE_DISABLED;
}

void SubViewport::set_size(int w, int h) {
    width = w < 0 ? 0 : w;
    height = h < 0 ? 0 : h;
    image.size_ = Vec2((float)width, (float)height);
}

bool SubViewport::set(const std::string& key, const Json& v, const PropCtx& ctx) {
    if (key == "size") {
        Vec2 s((float)width, (float)height);
        jv_vec2(v, s);
        set_size((int)s.x, (int)s.y);
        return true;
    }
    if (key == "size_2d_override") {
        jv_vec2(v, size_2d_override);
        return true;
    }
    if (key == "transparent_bg") {
        transparent_bg = v.b();
        return true;
    }
    if (key == "render_target_update_mode") {
        update_mode = v.i(UPDATE_WHEN_VISIBLE);
        return true;
    }
    /* Its 2D canvas only: stretch is how size_2d_override applies (always
     * stretched here); input, 3D, msaa and sdf settings change nothing
     * drawn. */
    if (key == "size_2d_override_stretch" || key == "handle_input_locally" || key == "gui_disable_input" ||
        key == "disable_3d" || key == "own_world_3d" || key == "world_3d" || key == "msaa_2d" || key == "sdf_scale" ||
        key == "gui_snap_controls_to_pixels" || key == "render_target_clear_mode")
        return true;
    return Node::set(key, v, ctx);
}

/* ---- SubViewportContainer ---- */

SubViewport* SubViewportContainer::viewport() const {
    for (size_t i = 0; i < children.size(); ++i)
        if (children[i]->type == "SubViewport") return (SubViewport*)children[i];
    return NULL;
}

bool SubViewportContainer::set(const std::string& key, const Json& v, const PropCtx& ctx) {
    if (key == "stretch") {
        stretch = v.b();
        return true;
    }
    if (key == "stretch_shrink") {
        stretch_shrink = v.i(1) < 1 ? 1 : v.i(1);
        return true;
    }
    return Control::set(key, v, ctx);
}

void SubViewportContainer::resized() {
    SubViewport* vp = viewport();
    if (vp && stretch) vp->set_size((int)(size.x / stretch_shrink + 0.5f), (int)(size.y / stretch_shrink + 0.5f));
}

void SubViewportContainer::draw(Canvas& c) {
    SubViewport* vp = viewport();
    if (!vp || !vp->texture) return;
    Vec2 s = stretch ? size : Vec2((float)vp->width, (float)vp->height);
    c.draw_texture_rect_region(&vp->image, Rect2(Vec2(0, 0), s), Rect2(Vec2(0, 0), vp->image.size_));
}

/* ---- ViewportTexture ---- */

SubViewport* ViewportTextureRes::viewport() const {
    if (direct) return direct;
    if (!user || viewport_path.empty()) return NULL;
    /* local to the scene: the path is from the root of the user's scene */
    Node* root = user->owner ? user->owner : user;
    Node* n = root->get_node(viewport_path);
    return n && n->type == "SubViewport" ? (SubViewport*)n : NULL;
}

Vec2 ViewportTextureRes::size() const {
    SubViewport* vp = viewport();
    return vp ? Vec2((float)vp->width, (float)vp->height) : Vec2(0, 0);
}

bool ViewportTextureRes::get_rect_region(const Rect2& dst, const Rect2& src, Rect2& out_dst, Rect2& out_src,
                                         ImageTextureRes** image) {
    SubViewport* vp = viewport();
    if (!vp || !vp->texture) return false;
    out_dst = dst;
    out_src = src;
    *image = &vp->image;
    return true;
}

Texture2DRes* bind_texture(Node* node, Resource* r) {
    if (!r) return NULL;
    if (r->kind == Resource::RES_VIEWPORT_TEXTURE) {
        const ViewportTextureRes* proto = (const ViewportTextureRes*)r;
        ViewportTextureRes* own = new ViewportTextureRes();
        own->path = proto->path;
        own->viewport_path = proto->viewport_path;
        own->user = node;
        node->owned_resources.push_back(own);
        return own;
    }
    return r->is_texture() ? (Texture2DRes*)r : NULL;
}

} /* namespace spire */

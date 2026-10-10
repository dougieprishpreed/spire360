/* Canvas: the draw commands of Godot's CanvasItem (draw_rect,
 * draw_texture_rect, nine patches, polygons) in an item's local coordinates,
 * turned into renderer triangles. One Canvas per frame; the scene tree sets
 * the item state (transform, modulate, blend) before each item draws. */
#ifndef SPIRE_CANVAS_H
#define SPIRE_CANVAS_H

#include "../gfx/renderer.h"
#include "resource.h"

namespace spire {

class Pack;

enum NinePatchAxis { AXIS_STRETCH = 0, AXIS_TILE = 1, AXIS_TILE_FIT = 2 };

class Canvas {
public:
    Canvas(Renderer* r, const Pack* pack);

    /* Item state. xform maps local coordinates to screen pixels. */
    void set_item(const Xform2D& xform, const Color& modulate, BlendMode blend, int tex_flags);
    void set_clip(const Rect2* screen_rect);
    /* Ported material shaders that reduce to a colour matrix (NULL: none). */
    void set_color_matrix(const float* m) { r_->set_color_matrix(m); }
    const Xform2D& xform() const { return xform_; }
    /* Per-draw blend inside an item (Spine slots). */
    void set_blend(BlendMode b) { blend_ = b; }
    BlendMode blend() const { return blend_; }

    void draw_rect(const Rect2& r, const Color& c);
    void draw_texture_rect(Texture2DRes* t, const Rect2& r, bool tile, const Color& mod = Color());
    void draw_texture_rect_region(Texture2DRes* t, const Rect2& r, const Rect2& src, const Color& mod = Color());
    /* Godot's canvas_item_add_nine_patch on the texture behind t; margins
     * are left, top, right, bottom. */
    void draw_nine_patch(Texture2DRes* t, const Rect2& rect, const Rect2& src, const float margin[4], int axis_h,
                         int axis_v, bool draw_center, const Color& mod);
    /* A glyph from an MSDF atlas page (Godot's
     * canvas_item_add_msdf_texture_rect_region); uv in page pixels. */
    void draw_msdf(ImageTextureRes* page, const Rect2& dst, const Rect2& uv, const Color& color, float px_range,
                   float outline);
    /* Untextured indexed triangles with per-vertex colours. */
    void draw_colored_triangles(const std::vector<Vec2>& v, const std::vector<int>& idx,
                                const std::vector<Color>& colors);
    /* Textured, indexed, per-vertex colours; uvs in 0..1 of the image. */
    void draw_triangles(ImageTextureRes* img, const std::vector<Vec2>& v, const std::vector<Vec2>& uv,
                        const std::vector<Color>& colors, const std::vector<int>& idx);

    Texture* gpu_texture(ImageTextureRes* img);
    Renderer* renderer() const { return r_; }
    long draw_calls() const { return calls_; }

private:
    void image_quad(ImageTextureRes* img, Rect2 dst, Rect2 src, const Color& mod, int extra_flags);
    void emit_quad(Texture* tex, const Vec2& p0, const Vec2& p1, const Vec2& uv0, const Vec2& uv1, const Color& c,
                   int flags);

    Renderer* r_;
    const Pack* pack_;
    Xform2D xform_;
    Color modulate_;
    BlendMode blend_;
    int tex_flags_;
    long calls_;
    std::vector<Vertex> verts_;
};

} /* namespace spire */

#endif

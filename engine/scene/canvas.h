/* Canvas: the draw commands of Godot's CanvasItem (draw_rect,
 * draw_texture_rect, nine patches, polygons) in an item's local coordinates,
 * turned into renderer triangles. A Canvas owns the renderer's texture cache;
 * the scene tree sets
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
    ~Canvas();
    void begin_frame();
    void end_frame();
    void set_texture_budget(size_t bytes) { texture_budget_ = bytes; }
    size_t texture_bytes() const { return texture_bytes_; }
    size_t texture_count() const;
    /* Generated sampler textures share the same ownership and budget. */
    Texture* cached_texture(const std::string& key);
    Texture* cache_texture(const std::string& key, const TextureData& data);

    /* Item state. xform maps local coordinates to screen pixels. */
    void set_item(const Xform2D& xform, const Color& modulate, BlendMode blend, int tex_flags);
    void set_clip(const Rect2* screen_rect);
    /* Ported material shaders that reduce to a colour matrix (NULL: none). */
    void set_color_matrix(const float* m) { r_->set_color_matrix(m); }
    void set_ripple(const float* p) { r_->set_ripple(p); }
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
                        const std::vector<Color>& colors, const std::vector<int>& idx, int flags = 0);

    Texture* gpu_texture(ImageTextureRes* img);
    Renderer* renderer() const { return r_; }
    long draw_calls() const { return calls_; }
    /* Count the draws without sending them (does an item draw anything?). */
    void set_count_only(bool on) { count_only_ = on; }

private:
    Canvas(const Canvas&);
    Canvas& operator=(const Canvas&);
    struct CachedTexture {
        Texture* texture;
        size_t bytes;
        u64 touched, frame;
        CachedTexture() : texture(NULL), bytes(0), touched(0), frame(0) {}
    };
    std::map<std::string, CachedTexture> textures_;
    size_t texture_bytes_, texture_budget_;
    u64 texture_clock_, frame_;
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
    bool count_only_;
    std::vector<Vertex> verts_;
};

} /* namespace spire */

#endif

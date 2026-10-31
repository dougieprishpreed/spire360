/* Canvas: the draw commands of Godot's CanvasItem (draw_rect,
 * draw_texture_rect, nine patches, polygons) in an item's local coordinates,
 * turned into renderer triangles. A Canvas owns the renderer's texture cache;
 * the scene tree sets
 * the item state (transform, modulate, blend) before each item draws. */
#ifndef SPIRE_CANVAS_H
#define SPIRE_CANVAS_H

#include "../gfx/image.h"
#include "../gfx/renderer.h"
#include "resource.h"

namespace spire {

class PackReader;

class Pack;

enum NinePatchAxis { AXIS_STRETCH = 0, AXIS_TILE = 1, AXIS_TILE_FIT = 2 };

class Canvas {
public:
    Canvas(Renderer* r, const Pack* pack);
    ~Canvas();
    void begin_frame();
    void end_frame();
    void set_texture_budget(size_t bytes) { texture_budget_ = bytes; }
    size_t texture_budget() const { return texture_budget_; }
    size_t texture_bytes() const { return texture_bytes_; }
    size_t texture_count() const;
    /* The cache's traffic since the last call (created, evicted, the
     * frames over the budget, the most bytes held), then reset. */
    struct TextureStats {
        /* starved: frames whose own textures alone were over the budget
         * (kept rather than evicted as they were drawn) */
        unsigned created, evicted, over_frames, starved;
        size_t peak_bytes;
    };
    TextureStats take_texture_stats();
    /* A test aid (spire_play --console-textures): each texture counted at
     * its Xbox 360 size (DXT blocks or texels tiled in 32 x 32, in 4 KB
     * pages) instead of this renderer's, so a PC run fills the budget as
     * the console does. */
    void set_console_sizes(bool on) { console_sizes_ = on; }
    /* A diagnostic: the next frame's draws logged one by one (the image,
     * its texture's size and format or that it has none, the screen rect,
     * the colour), text glyphs counted. */
    void dump_next_frame() { dump_pending_ = true; }
    /* Generated sampler textures share the same ownership and budget. */
    Texture* cached_texture(const std::string& key);
    Texture* cache_texture(const std::string& key, const TextureData& data);
    /* A pack texture loaded before its first draw (SceneLibrary's warm-up):
     * kept until the budget needs its room (an idle one is not released).
     * False: it was there already, or the pack has no such texture. */
    bool prefetch(const std::string& path);
    /* The same through the reader (set_reader): 0 the texture is held (made
     * now from what was read, or before), 1 its read is under way (ask
     * again), -1 it cannot be made. Without a reader: prefetch's read. */
    int prefetch_async(const std::string& path);
    /* Textures read ahead on its thread (PackReader); NULL: read here. */
    void set_reader(PackReader* reader) { reader_ = reader; }
    /* A renderer texture its owner no longer uses (a SubViewport's capture),
     * destroyed at the next begin_frame. */
    static void release_later(Texture* t);

    /* For a caller that writes its own quads' vertices (Particles2D's
     * plain particles): the texture, the item's transform and modulate,
     * its blend, the render target's size (0 0: unknown). False when the
     * vertices must go through draw_triangles (a dump, counting, the
     * wind's sway, no texture yet). direct_reserve / direct_commit then
     * frame each batch (at most Renderer::RESERVE_MAX vertices); one draw
     * counted for the lot (direct_done). */
    struct Direct {
        Texture* tex;
        Xform2D xform;
        Color modulate;
        BlendMode blend;
        int target_w, target_h;
    };
    bool direct(ImageTextureRes* img, Direct* out);
    Vertex* direct_reserve(const Direct& d, int count) { return r_->reserve(d.tex, count, blend_, tex_flags_); }
    void direct_commit(int unused) {
        if (unused > 0) r_->give_back(unused);
        r_->commit();
    }
    void direct_done() { ++calls_; }

    /* Item state. xform maps local coordinates to screen pixels. */
    void set_item(const Xform2D& xform, const Color& modulate, BlendMode blend, int tex_flags);
    void set_clip(const Rect2* screen_rect);
    /* Ported material shaders that reduce to a colour matrix (NULL: none). */
    void set_color_matrix(const float* m) { r_->set_color_matrix(m); }
    void set_ripple(const float* p) { r_->set_ripple(p); }
    /* A vertex shader's sway (shaders/vfx/wind_sway.gdshader): each local
     * vertex moves amount * max(0, (1 - UV.y) - height_offset) along x
     * before the item's transform. on false: none. */
    void set_wind(bool on, float amount, float height_offset) {
        wind_on_ = on;
        wind_ = amount;
        wind_height_ = height_offset;
    }
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
    /* The glyphs drawn between these go out at end_glyphs a page of their
     * font at a time, in their order on each page: one draw a page instead
     * of one at each change of page (a font's glyphs lie on several 512 x
     * 512 pages, and neighbours in a word often on different ones). For
     * glyphs of one pass (shadow, outline or fill), which may overlap only
     * in their own colour, where the order makes no difference. */
    void begin_glyphs();
    void end_glyphs();
    /* Untextured indexed triangles with per-vertex colours. */
    void draw_colored_triangles(const std::vector<Vec2>& v, const std::vector<int>& idx,
                                const std::vector<Color>& colors);
    /* Textured, indexed, per-vertex colours; uvs in 0..1 of the image. */
    void draw_triangles(ImageTextureRes* img, const std::vector<Vec2>& v, const std::vector<Vec2>& uv,
                        const std::vector<Color>& colors, const std::vector<int>& idx, int flags = 0);

    Texture* gpu_texture(ImageTextureRes* img);
    /* The colour matrix of shaders/hsv.gdshader at h, s, v (rows of r, g,
     * b, offset). */
    static void hsv_matrix(float h, float s, float v, float out[12]);
    Renderer* renderer() const { return r_; }
    /* Textures a screen draws that wait for room in the GPU's memory (a new
     * room holds its fade-in while one does). */
    static bool textures_waiting() { return waiting_textures_ > 0; }
    long draw_calls() const { return calls_; }
    /* Count the draws without sending them (does an item draw anything?). */
    void set_count_only(bool on) { count_only_ = on; }
    bool count_only() const { return count_only_; }

private:
    Canvas(const Canvas&);
    Canvas& operator=(const Canvas&);
    struct CachedTexture {
        Texture* texture;
        size_t bytes;
        u64 touched, frame;
        bool kept; /* prefetch: no idle release */
        /* a texture the GPU's memory had no room for: tried again from
         * this frame (0: missing from the pack, never) */
        u64 retry_at;
        CachedTexture() : texture(NULL), bytes(0), touched(0), frame(0), kept(false), retry_at(0) {}
    };
    std::map<std::string, CachedTexture> textures_;
    unsigned serial_; /* this canvas among all (ImageTextureRes::gpu_canvas) */
    unsigned epoch_;  /* textures_' erasures: an image's remembered entry is stale */
    size_t texture_bytes_, texture_budget_;
    /* bytes end_frame releases beyond the budget's (a texture not made for
     * want of memory) */
    size_t want_free_;
    u64 texture_clock_, frame_;
    TextureStats stats_;
    bool console_sizes_;
    bool dump_pending_, dumping_;
    unsigned dump_draws_, dump_glyphs_;
    const char* dump_what_; /* the image emit_quad draws (the dump) */
    void image_quad(ImageTextureRes* img, Rect2 dst, Rect2 src, const Color& mod, int extra_flags);
    bool derive(const DerivedImage& d, TextureData& out);
    /* The last pack image derive decoded (until the frame ends). */
    std::string decoded_path_;
    Image decoded_;
    int decoded_w_, decoded_h_;
    void emit_quad(Texture* tex, const Vec2& p0, const Vec2& p1, const Vec2& uv0, const Vec2& uv1, const Color& c,
                   int flags);
    struct HeldGlyph {
        Texture* page;
        Rect2 dst;
        Vec2 uv0, uv1;
        Color color;
        float px_range, outline;
    };
    bool glyphs_held_;
    std::vector<HeldGlyph> glyphs_;

    Renderer* r_;
    const Pack* pack_;
    PackReader* reader_;
    Xform2D xform_;
    Color modulate_;
    BlendMode blend_;
    int tex_flags_;
    long calls_;
    bool count_only_;
    bool wind_on_;
    float wind_, wind_height_;
    Texture* load_gpu_texture(ImageTextureRes* img, const std::string& key);
    void no_room(const std::string& key, const std::string& path, TextureData& data);
    /* textures read that the GPU's memory had no room for, kept for the
     * retry (read again, the merchant's room waited 0.5 s more on its
     * floor's) */
    std::map<std::string, TextureData> no_room_data_;
    static int waiting_textures_;
    void put_triangles(Texture* tex, const std::vector<Vec2>& v, const std::vector<Vec2>* uv,
                       const std::vector<Color>* colors, const std::vector<int>& idx, int flags);
    Vec2 sway(const Vec2& p, const Vec2& uv) const {
        if (!wind_on_) return p;
        float k = 1.0f - uv.y - wind_height_;
        return k > 0 ? Vec2(p.x + wind_ * k, p.y) : p;
    }
};

} /* namespace spire */

#endif

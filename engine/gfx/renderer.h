/* Renderer interface: everything is textured, vertex-coloured triangles in
 * screen pixels. Backends: software (PC tools, tests), D3D9 (Xbox 360). */
#ifndef SPIRE_RENDERER_H
#define SPIRE_RENDERER_H

#include "../core/base.h"
#include "../core/pack.h"

namespace spire {

enum BlendMode { BLEND_MIX, BLEND_ADD, BLEND_SUB, BLEND_MUL, BLEND_PREMULT_ALPHA };

/* draw_triangles flags */
enum { DRAW_REPEAT = 1, DRAW_NEAREST = 2, DRAW_MSDF = 4 };

class Texture {
public:
    Texture() : width(0), height(0), orig_width(0), orig_height(0) {}
    virtual ~Texture() {}
    int width, height;           /* stored pixels */
    int orig_width, orig_height; /* layout units (the game's size) */
};

struct Vertex {
    Vec2 pos;     /* screen pixels */
    Vec2 uv;      /* 0..1 */
    Color color;  /* straight alpha, multiplies the texel */
    Vertex() {}
    Vertex(const Vec2& p, const Vec2& t, const Color& c) : pos(p), uv(t), color(c) {}
};

class Renderer {
public:
    virtual ~Renderer() {}
    virtual Texture* create_texture(const TextureData& data) = 0;
    virtual void destroy_texture(Texture* t) = 0;
    virtual void begin(int width, int height, const Color& clear) = 0;
    /* Screen-space scissor; NULL disables. */
    virtual void set_clip(const Rect2* clip) = 0;
    /* count is a multiple of 3. tex NULL = white. flags: DRAW_*. */
    virtual void draw_triangles(Texture* tex, const Vertex* v, int count, BlendMode blend, int flags) = 0;
    /* Parameters of the next DRAW_MSDF draws (Godot's canvas shader: pixel
     * range of the glyph atlas, outline thickness; 0 = fill). */
    virtual void set_msdf(float px_range, float outline) = 0;
    /* A colour matrix applied to the texel before modulation (ported canvas
     * shaders such as the game's hsv.gdshader): rgb' = M * rgb + offset * a,
     * m = 3 rows of (r, g, b, offset). NULL: identity. */
    virtual void set_color_matrix(const float* m) = 0;
    virtual void end() = 0;
};

} /* namespace spire */

#endif

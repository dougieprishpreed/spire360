/* Renderer interface: everything is textured, vertex-coloured triangles in
 * screen pixels. Backends: software (PC tools, tests), D3D9 (Xbox 360). */
#ifndef SPIRE_RENDERER_H
#define SPIRE_RENDERER_H

#include "../core/base.h"
#include "../core/pack.h"

namespace spire {

enum BlendMode { BLEND_MIX, BLEND_ADD, BLEND_SUB, BLEND_MUL, BLEND_PREMULT_ALPHA };

/* draw_triangles flags. DRAW_GROUP: composite the canvas group layer
 * through this draw's alpha (see begin_group). */
enum { DRAW_REPEAT = 1, DRAW_NEAREST = 2, DRAW_MSDF = 4, DRAW_GROUP = 8 };

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
    /* Canvas groups (Godot's clip_children). Draws between begin_group and
     * end_group go to a transparent offscreen layer instead of the screen.
     * Later draws flagged DRAW_GROUP composite that layer through their own
     * alpha: out = layer(pixel) * texel.a * colour.a, premultiplied over the
     * screen. bounds: the screen area the group and its mask stay inside.
     * One group at a time; false if groups are unavailable (the caller then
     * draws the children unmasked). */
    /* Godot's shaders/card_ripple.gdshader on the next draws (the card
     * glow): a = texel.a * colour.a is read as a distance field; with
     * sdf = mod(phase + a, modulo), b = a - sdf + smoothstep(0, ease, sdf) *
     * modulo, the output alpha is smoothstep(1 - width, 1, b). p = phase
     * (TIME * ripple_speed), modulo, ease, width; NULL: off. */
    /* Gradient map (the game's grayscale and flipbook VFX shaders): the
     * texel's red picks the colour from lut (a 256x1 texture, straight
     * alpha); the alpha is the texel's red (alpha_from_red) or its alpha.
     * The vertex colour multiplies the result. NULL: off. */
    virtual void set_gradient_map(Texture* lut, bool alpha_from_red) = 0;
    virtual void set_ripple(const float* p) = 0;
    virtual bool begin_group(const Rect2& bounds) = 0;
    virtual void end_group() = 0;
    virtual void end() = 0;
};

} /* namespace spire */

#endif

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
    Texture() : width(0), height(0), orig_width(0), orig_height(0), memory_bytes(0) {}
    virtual ~Texture() {}
    int width, height;           /* stored pixels */
    int orig_width, orig_height; /* layout units (the game's size) */
    size_t memory_bytes;        /* renderer allocation estimate */
};

struct Vertex {
    Vec2 pos;     /* screen pixels */
    Vec2 uv;      /* 0..1 */
    Color color;  /* straight alpha, multiplies the texel */
    Vertex() {}
    Vertex(const Vec2& p, const Vec2& t, const Color& c) : pos(p), uv(t), color(c) {}
};

/* The game's stepped fire shaders (shaders/vfx/vfx_stepped_shader_fire_*):
 * two scrolling noise textures times a mask, the texture read through two
 * scrolling distortions, thresholded into an inner and an outer colour.
 * Textures NULL: white (Godot's default for an unset sampler). Each *_uv is
 * scale.xy, offset.xy (the panning's offset already at this time). */
struct FireParams {
    Texture* noise1;
    Texture* noise2;
    Texture* noise_mask;  /* clamped */
    Texture* dist1;
    Texture* dist2;
    Texture* dist_mask;   /* clamped */
    float noise1_uv[4], noise2_uv[4], mask_uv[4], dist1_uv[4], dist2_uv[4], dist_mask_uv[4];
    float strength[4];    /* noise1, noise2, invert mask (0/1), dark variant (0/1) */
    float dist_strength[4]; /* dist1, dist2 */
    float steps[4];       /* inner step x, y; outer step x, y */
    float inner[4], outer[4];
};

/* The game's erosion particle shaders (vfx_ring_polar, vfx_poof,
 * vfx_common_particle...): the texture (read in polar coordinates about its
 * centre, or through a scrolling distortion of itself), its colour from a
 * gradient (lut: the red picks it; NULL: white), its alpha eroded:
 * smoothstep(threshold, threshold + width, red or alpha); times the vertex
 * colour. */
struct VfxParams {
    Texture* lut;
    float polar;          /* 1: polar coordinates */
    float erosion[4];     /* threshold, width, on (0/1), from the alpha (0/1: red) */
    float distortion[4];  /* offset.xy (the panning now), strength, on (0/1) */
};

/* Shaders that read the screen (hint_screen_texture). Godot copies the
 * screen as drawn so far once per canvas, at the first item reading it, and
 * makes its mipmaps by downsampling each level by two and blurring it (a
 * 7-tap Gaussian, CopyEffects::gaussian_blur). The draws replace texel and
 * colour by the copy read at the pixel's screen uv (SCREEN_UV):
 *  SCREEN_DARK (dark_blur): the copy at lod (trilinear), mixed toward
 *      opaque black by mix.
 *  SCREEN_NOISE (vfx_water_reflection_post, the_insatiable_sand_fall_2):
 *      at SCREEN_UV - strength / 2 + noise(UV + noise_offset).r * strength
 *      (noise repeats; NULL: white), its alpha times mask(UV).a (NULL:
 *      white).
 *  SCREEN_RADIAL (radial_blur): the mean of count reads at SCREEN_UV -
 *      power * (SCREEN_UV - center) * i, i = 0..count-1 (at most 16),
 *      opaque.
 *  SCREEN_GLASS (debug_ancient_card_border_overlay, the Ancient cards'
 *      glass border): the copy at lod, its rgb + strength, its alpha
 *      mask(UV).a, mixed toward color by mask(UV).g * mix; times the
 *      vertex colour and the texel's alpha times mask(UV).a.
 *  SCREEN_DOOM (doom_overlay): the copy in YIQ, its chroma times
 *      mix(1, mix, a) and all of it times mix(1, strength, a) (a: the
 *      vertex colour's alpha), back to RGB, plus color * SCREEN_UV.y^6 *
 *      power * a (power: 0.7 * the shader's pulse at this time). */
enum ScreenMode { SCREEN_DARK = 1, SCREEN_NOISE = 2, SCREEN_RADIAL = 4, SCREEN_GLASS = 5, SCREEN_DOOM = 6 };

struct ScreenParams {
    int mode;
    float lod, mix;
    Texture* noise;
    Texture* mask;
    float noise_offset[2];
    float strength;
    float center[2];
    float power;
    int count;
    float color[4];
};

/* An effect shader (effects.h): its id, parameters and textures (NULL:
 * white; bit i of `repeat`: texture i repeats, else clamps). */
struct EffectParams {
    int id;
    float p[16];
    Texture* tex[4];
    unsigned repeat;
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
    /* The stepped fire (FireParams); NULL: off. The draw's texture is the
     * shader's TEXTURE; the vertex colour's alpha fades it. */
    virtual void set_fire(const FireParams* p) = 0;
    /* The erosion particle shaders (VfxParams); NULL: off. */
    virtual void set_vfx(const VfxParams* p) = 0;
    virtual void set_ripple(const float* p) = 0;
    /* The screen-reading shaders (ScreenParams) on the next draws; NULL:
     * off. false: not available (the caller does not draw the item). */
    virtual bool set_screen(const ScreenParams* p) {
        (void)p;
        return false;
    }
    /* A new canvas: the next screen-reading draw copies the screen again. */
    virtual void screen_barrier() {}
    /* An effect shader on the next draws (EffectParams); NULL: off. false:
     * not available (the caller does not draw the item). */
    virtual bool set_effect(const EffectParams* e) {
        (void)e;
        return false;
    }
    virtual bool begin_group(const Rect2& bounds) = 0;
    virtual void end_group() = 0;
    virtual void end() = 0;
};

} /* namespace spire */

#endif

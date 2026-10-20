/* Software renderer: the reference backend for PC tools and tests. Samples
 * like Godot's canvas renderer (bilinear, texel centres, clamp to edge) and
 * blends premultiplied. */
#ifndef SPIRE_SW_RENDERER_H
#define SPIRE_SW_RENDERER_H

#include "image.h"
#include "renderer.h"

namespace spire {

class SwTexture : public Texture {
public:
    Image img; /* level 0, premultiplied */
};

class SwRenderer : public Renderer {
public:
    SwRenderer();
    virtual Texture* create_texture(const TextureData& data);
    virtual void destroy_texture(Texture* t);
    virtual void begin(int width, int height, const Color& clear);
    virtual void set_clip(const Rect2* clip);
    virtual void draw_triangles(Texture* tex, const Vertex* v, int count, BlendMode blend, int flags);
    virtual void end() {}
    virtual void set_color_matrix(const float* m) {
        cm_on_ = m != NULL;
        if (m)
            for (int i = 0; i < 12; ++i) cm_[i] = m[i];
    }
    virtual void set_ripple(const float* p) {
        ripple_on_ = p != NULL;
        if (p)
            for (int i = 0; i < 4; ++i) ripple_[i] = p[i];
    }
    virtual void set_gradient_map(Texture* lut, bool alpha_from_red) {
        gm_ = (SwTexture*)lut;
        gm_red_ = alpha_from_red;
    }
    virtual void set_vfx(const VfxParams* p) {
        vfx_on_ = p != NULL;
        if (p) vfx_ = *p;
    }
    virtual void set_fire(const FireParams* p) {
        fire_on_ = p != NULL;
        if (p) fire_ = *p;
    }
    virtual void set_msdf(float px_range, float outline) {
        msdf_range_ = px_range;
        msdf_outline_ = outline;
    }

    virtual bool set_screen(const ScreenParams* p);
    virtual bool set_effect(const EffectParams* e) {
        effect_on_ = e != NULL;
        if (e) effect_ = *e;
        return true;
    }
    virtual void screen_barrier() { screen_levels_ = 0; }

    virtual bool begin_group(const Rect2& bounds);
    virtual void end_group();

    const Image& framebuffer() const { return fb_; }
    long triangles_drawn() const { return tris_; }

private:
    void triangle(SwTexture* tex, const Vertex& a, const Vertex& b, const Vertex& c, BlendMode blend, int flags);
    Image fb_;
    Image layer_;   /* the canvas group layer */
    Image* target_; /* fb_, or layer_ inside a group */
    int gx0_, gy0_, gx1_, gy1_; /* the group's pixel bounds */
    bool clip_on_;
    int cx0_, cy0_, cx1_, cy1_;
    long tris_;
    float msdf_range_, msdf_outline_;
    bool cm_on_;
    float cm_[12];
    bool ripple_on_;
    float ripple_[4];
    SwTexture* gm_; /* gradient map lut (straight alpha), NULL: off */
    bool gm_red_;
    bool fire_on_;
    FireParams fire_;
    bool vfx_on_;
    VfxParams vfx_;
    bool screen_on_;
    ScreenParams screen_;
    bool effect_on_;
    EffectParams effect_;
    void effect_pixel(const SwTexture* tex, float u, float v, int flags, float su, float sv, const float col[4],
                      float out[4]) const;
    std::vector<Image> screen_mips_; /* the screen copy and its levels */
    int screen_levels_;              /* levels made this canvas (0: no copy yet) */
    void screen_level(int level);
    void sample_screen(float u, float v, float lod, float out[4]) const;
};

} /* namespace spire */

#endif

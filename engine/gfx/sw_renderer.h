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
    virtual void set_msdf(float px_range, float outline) {
        msdf_range_ = px_range;
        msdf_outline_ = outline;
    }

    const Image& framebuffer() const { return fb_; }
    long triangles_drawn() const { return tris_; }

private:
    void triangle(SwTexture* tex, const Vertex& a, const Vertex& b, const Vertex& c, BlendMode blend, int flags);
    Image fb_;
    bool clip_on_;
    int cx0_, cy0_, cx1_, cy1_;
    long tris_;
    float msdf_range_, msdf_outline_;
    bool cm_on_;
    float cm_[12];
};

} /* namespace spire */

#endif

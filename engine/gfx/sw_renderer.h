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
    virtual void draw_triangles(Texture* tex, const Vertex* v, int count, BlendMode blend);
    virtual void end() {}

    const Image& framebuffer() const { return fb_; }
    long triangles_drawn() const { return tris_; }

private:
    void triangle(SwTexture* tex, const Vertex& a, const Vertex& b, const Vertex& c, BlendMode blend);
    Image fb_;
    bool clip_on_;
    int cx0_, cy0_, cx1_, cy1_;
    long tris_;
};

} /* namespace spire */

#endif

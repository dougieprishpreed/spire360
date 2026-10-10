/* D3D9 renderer: PC Direct3D 9 and the Xbox 360's Direct3D (XDK), one
 * implementation. Batches triangles while the state (texture, blend, flags,
 * MSDF and colour-matrix parameters, clip) is unchanged.
 *
 * Xbox 360 notes (_XBOX): textures from the pack hold PC block order; they
 * are endian-swapped and tiled for Xenos when created (XGEndianSwapSurface,
 * XGTileTextureLevel). Shaders come precompiled from the XDK's fxc
 * (canvas_vs.h / canvas_ps.h from shaders/canvas.hlsl); on PC they are
 * compiled at startup with D3DCompile. */
#ifndef SPIRE_D3D9_RENDERER_H
#define SPIRE_D3D9_RENDERER_H

#include "renderer.h"

#if defined(_XBOX)
#include <xtl.h>
#else
#include <d3d9.h>
#endif

namespace spire {

class D3D9Texture : public Texture {
public:
    D3D9Texture() : tex(NULL) {}
    IDirect3DTexture9* tex;
};

class D3D9Renderer : public Renderer {
public:
    D3D9Renderer();
    virtual ~D3D9Renderer();
    /* device: created by the platform layer. width/height: back buffer. */
    bool init(IDirect3DDevice9* device, std::string* error);
    void shutdown();

    virtual Texture* create_texture(const TextureData& data);
    virtual void destroy_texture(Texture* t);
    virtual void begin(int width, int height, const Color& clear);
    virtual void set_clip(const Rect2* clip);
    virtual void draw_triangles(Texture* tex, const Vertex* v, int count, BlendMode blend, int flags);
    virtual void set_msdf(float px_range, float outline);
    virtual void set_color_matrix(const float* m);
    virtual void end();

    long draw_calls() const { return draws_; }

private:
    struct GpuVertex {
        float x, y, u, v;
        float r, g, b, a;
    };
    void flush();
    void apply_state();
    IDirect3DDevice9* dev_;
    IDirect3DVertexShader9* vs_;
    IDirect3DPixelShader9* ps_;
    IDirect3DVertexDeclaration9* decl_;
    IDirect3DTexture9* white_;
    std::vector<GpuVertex> batch_;
    /* pending state (what the batch was recorded with) */
    D3D9Texture* tex_;
    BlendMode blend_;
    int flags_;
    float msdf_[2];
    bool cm_on_;
    float cm_[12];
    bool clip_on_;
    RECT clip_;
    bool dirty_;
    int width_, height_;
    long draws_;
};

} /* namespace spire */

#endif

/* D3D9 renderer: PC Direct3D 9 and the Xbox 360's Direct3D (XDK), one
 * implementation. Batches triangles while the state (texture, blend, flags,
 * MSDF and colour-matrix parameters, clip) is unchanged.
 *
 * Xbox 360 notes (_XBOX): textures from the pack hold PC block order; they
 * are endian-swapped and tiled for Xenos when created (XGEndianSwapSurface,
 * XGTileTextureLevel). Shaders come precompiled from the XDK's fxc
 * (canvas_vs.h / canvas_ps.h from shaders/canvas.hlsl); on PC they are
 * compiled at startup with D3DCompile.
 *
 * Canvas groups (clip_children) draw into an offscreen target: on PC a
 * render-target texture; on Xbox 360 a second EDRAM surface after the back
 * buffer, resolved into a texture for the mask pass (only the group's
 * bounds, aligned to the resolve granularity).
 *
 * Screen-reading shaders (set_screen): the screen is copied into a texture
 * (PC: StretchRect; Xbox 360: Resolve), and each blurred level of the copy
 * is a texture of its own, drawn by a pass of the canvas shader from the
 * level above (PC: into the texture; Xbox 360: into an EDRAM surface after
 * the group surface, resolved). */
#ifndef SPIRE_D3D9_RENDERER_H
#define SPIRE_D3D9_RENDERER_H

#include "effects.h"
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
    virtual void set_ripple(const float* p);
    virtual void set_gradient_map(Texture* lut, bool alpha_from_red);
    virtual void set_fire(const FireParams* p);
    virtual void set_vfx(const VfxParams* p);
    virtual bool set_screen(const ScreenParams* p);
    virtual bool set_effect(const EffectParams* e);
    virtual void screen_barrier() { screen_levels_ = 0; }
    virtual bool begin_group(const Rect2& bounds);
    virtual void end_group();
    virtual bool begin_capture(int w, int h);
    virtual Texture* end_capture(Texture* reuse);
    virtual void end();

    long draw_calls() const { return draws_; }

    /* Debugging the canvas groups on the console: how they are drawn.
     * 0 normal; 1 groups off (the children unmasked); 2 the layer through
     * the mask's quads (the mask's alpha ignored); 3 normal, and the layer
     * texture over the whole frame; 4 the whole surface resolved; 5 as 4,
     * and the layer texture over the frame. The first groups after a
     * change are logged. */
    enum { GROUP_DEBUG_MODES = 6 };
    void set_group_debug(int mode);
    int group_debug() const { return group_debug_; }

private:
    struct GpuVertex {
        float x, y, u, v;
        float r, g, b, a;
    };
    void flush();
    void apply_state();
    /* The state apply_state last gave the device, so a batch only sends
     * what changed (a texture, mostly); forgotten whenever other code sets
     * device state directly (a frame's start, targets, the screen passes). */
    enum { CACHED_STAGES = 13, CACHED_CONSTANTS = 64 };
    void forget_state();
    void set_tex(DWORD stage, IDirect3DBaseTexture9* t);
    void set_samp(DWORD stage, D3DSAMPLERSTATETYPE type, DWORD v);
    void set_rs(D3DRENDERSTATETYPE type, DWORD v);
    void set_psc(UINT reg, const float* v, UINT count);
    IDirect3DBaseTexture9* c_tex_[CACHED_STAGES];
    DWORD c_samp_[CACHED_STAGES][5]; /* ADDRESSU, ADDRESSV, MAGFILTER, MINFILTER, MIPFILTER */
    IDirect3DPixelShader9* c_ps_;
    DWORD c_rs_[4];                  /* BLENDOP, SRCBLEND, DESTBLEND, SCISSORTESTENABLE */
    bool c_clip_known_;
    RECT c_clip_;
    float c_psc_[CACHED_CONSTANTS][4];
    bool c_psc_known_[CACHED_CONSTANTS];
    bool make_group_target();
    void release_group_target();
    enum { SCREEN_LEVELS = 6 };
    bool copy_screen();
    bool screen_pass(int level);
    void release_screen();
    void level_size(int level, int* w, int* h) const;
    IDirect3DDevice9* dev_;
    IDirect3DVertexShader9* vs_;
    IDirect3DPixelShader9* ps_;
    IDirect3DVertexDeclaration9* decl_;
    IDirect3DTexture9* white_;
    /* Xbox 360: D3D frees a released texture's memory at once while the
     * GPU may still read it for the frames it has queued; a destroyed
     * texture is released once the GPU has passed the fence set then. */
    struct Retired {
        IDirect3DTexture9* tex;
        DWORD fence;
    };
    std::vector<Retired> retired_;
    void release_retired(bool all);
    std::vector<GpuVertex> batch_;
    /* pending state (what the batch was recorded with) */
    D3D9Texture* tex_;
    BlendMode blend_;
    int flags_;
    float msdf_[2];
    bool cm_on_;
    float cm_[12];
    bool ripple_on_;
    float ripple_[4];
    D3D9Texture* gm_; /* gradient map lut, NULL: off */
    bool gm_red_;
    bool fire_on_;
    FireParams fire_;
    bool vfx_on_;
    VfxParams vfx_;
    bool clip_on_;
    RECT clip_;
    bool dirty_;
    /* Canvas groups: an offscreen target and the texture the mask samples
     * (on Xbox 360 a second EDRAM surface resolved into group_tex_; on PC a
     * render-target texture, both the same object). */
    IDirect3DSurface9* group_rt_;
    IDirect3DTexture9* group_tex_;
    IDirect3DSurface9* main_rt_;
    bool group_failed_;
    RECT group_rect_;
    D3D9Texture group_view_; /* group_tex_ as a Texture (debug view) */
    /* A capture (begin_capture) on the group target: its size (0: none);
     * on PC the system-memory copy it is read back through. */
    int cap_w_, cap_h_;
#if !defined(_XBOX)
    IDirect3DSurface9* capture_sys_;
#endif
    int group_debug_, group_log_;
    /* The screen copy (level 0) and its blurred levels. */
    bool screen_on_;
    ScreenParams screen_;
    IDirect3DTexture9* screen_lv_[SCREEN_LEVELS];
#if defined(_XBOX)
    IDirect3DSurface9* screen_rt_[SCREEN_LEVELS]; /* EDRAM surfaces of the passes */
#endif
    int screen_levels_; /* levels made this canvas (0: no copy yet) */
    bool screen_failed_;
    /* Effect shaders (effects.h): one pixel shader each. */
    IDirect3DPixelShader9* effect_ps_[EFFECT_COUNT];
    bool effect_on_;
    EffectParams effect_;
    int width_, height_;
    long draws_;
};

} /* namespace spire */

#endif

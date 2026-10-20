#include "d3d9_renderer.h"

#include "../core/pack.h"

#include <math.h>
#include <string.h>

#if defined(_XBOX)
#include <xgraphics.h>
#if !defined(XGTILE_NONPACKED)
#define XGTILE_NONPACKED 0x1
#endif
#include "canvas_ps.h" /* g_ps_main, from fxc /Fh /Vn g_ps_main */
#include "canvas_vs.h" /* g_vs_main */
#else
#include <d3dcompiler.h>
static const char* const CANVAS_HLSL[] = {
#include "shaders/canvas_hlsl.inc"
    NULL
};
#endif

namespace spire {

D3D9Renderer::D3D9Renderer()
    : dev_(NULL), vs_(NULL), ps_(NULL), decl_(NULL), white_(NULL), tex_(NULL), blend_(BLEND_MIX), flags_(0),
      cm_on_(false), ripple_on_(false), gm_(NULL), gm_red_(false), fire_on_(false), vfx_on_(false), clip_on_(false), dirty_(true), group_rt_(NULL), group_tex_(NULL), main_rt_(NULL),
      group_failed_(false), group_debug_(0), group_log_(0), screen_on_(false), screen_levels_(0),
      screen_failed_(false), width_(0), height_(0), draws_(0) {
    memset(&screen_, 0, sizeof(screen_));
    for (int i = 0; i < SCREEN_LEVELS; ++i) {
        screen_lv_[i] = NULL;
#if defined(_XBOX)
        screen_rt_[i] = NULL;
#endif
    }
    msdf_[0] = 16;
    msdf_[1] = 0;
    memset(cm_, 0, sizeof(cm_));
    memset(ripple_, 0, sizeof(ripple_));
    memset(&clip_, 0, sizeof(clip_));
    memset(&group_rect_, 0, sizeof(group_rect_));
}

D3D9Renderer::~D3D9Renderer() { shutdown(); }

#define RELEASE(p)          \
    do {                    \
        if (p) p->Release(); \
        p = NULL;           \
    } while (0)

void D3D9Renderer::shutdown() {
    release_group_target();
    release_screen();
    RELEASE(vs_);
    RELEASE(ps_);
    RELEASE(decl_);
    RELEASE(white_);
    dev_ = NULL;
}

#if !defined(_XBOX)
typedef HRESULT(WINAPI* D3DCompileFn)(LPCVOID, SIZE_T, LPCSTR, const D3D_SHADER_MACRO*, ID3DInclude*, LPCSTR, LPCSTR,
                                      UINT, UINT, ID3DBlob**, ID3DBlob**);

static bool compile(const std::string& src, const char* entry, const char* target, ID3DBlob** out,
                    std::string* error) {
    static D3DCompileFn fn = NULL;
    if (!fn) {
        HMODULE m = LoadLibraryA("d3dcompiler_47.dll");
        if (!m) m = LoadLibraryA("d3dcompiler_43.dll");
        if (m) fn = (D3DCompileFn)(void*)GetProcAddress(m, "D3DCompile");
    }
    if (!fn) {
        if (error) *error = "d3dcompiler_47.dll not found";
        return false;
    }
    ID3DBlob* errs = NULL;
    HRESULT hr = fn(src.c_str(), src.size(), "canvas.hlsl", NULL, NULL, entry, target, 0, 0, out, &errs);
    if (FAILED(hr)) {
        if (error) *error = errs ? std::string((const char*)errs->GetBufferPointer(), errs->GetBufferSize()) : "D3DCompile failed";
        if (errs) errs->Release();
        return false;
    }
    if (errs) errs->Release();
    return true;
}
#endif

bool D3D9Renderer::init(IDirect3DDevice9* device, std::string* error) {
    dev_ = device;
    D3DVERTEXELEMENT9 el[] = { { 0, 0, D3DDECLTYPE_FLOAT2, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_POSITION, 0 },
                               { 0, 8, D3DDECLTYPE_FLOAT2, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_TEXCOORD, 0 },
                               { 0, 16, D3DDECLTYPE_FLOAT4, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_COLOR, 0 },
                               D3DDECL_END() };
    if (FAILED(dev_->CreateVertexDeclaration(el, &decl_))) {
        if (error) *error = "CreateVertexDeclaration failed";
        return false;
    }
#if defined(_XBOX)
    if (FAILED(dev_->CreateVertexShader((const DWORD*)g_vs_main, &vs_)) ||
        FAILED(dev_->CreatePixelShader((const DWORD*)g_ps_main, &ps_))) {
        if (error) *error = "CreateVertexShader/CreatePixelShader failed";
        return false;
    }
#else
    std::string src;
    for (int i = 0; CANVAS_HLSL[i]; ++i) src += CANVAS_HLSL[i];
    ID3DBlob *vsb = NULL, *psb = NULL;
    if (!compile(src, "vs_main", "vs_3_0", &vsb, error) || !compile(src, "ps_main", "ps_3_0", &psb, error)) return false;
    HRESULT h1 = dev_->CreateVertexShader((const DWORD*)vsb->GetBufferPointer(), &vs_);
    HRESULT h2 = dev_->CreatePixelShader((const DWORD*)psb->GetBufferPointer(), &ps_);
    vsb->Release();
    psb->Release();
    if (FAILED(h1) || FAILED(h2)) {
        if (error) *error = "CreateVertexShader/CreatePixelShader failed";
        return false;
    }
#endif
    /* 1x1 white for untextured draws */
    TextureData td;
    td.format = TEX_ARGB8;
    td.flags = 0;
    td.width = td.height = td.orig_width = td.orig_height = 1;
    td.levels.resize(1);
    td.levels[0].assign(4, 0xFF);
    D3D9Texture* w = (D3D9Texture*)create_texture(td);
    if (!w || !w->tex) {
        if (error) *error = "cannot create textures";
        delete w;
        return false;
    }
    white_ = w->tex;
    w->tex = NULL;
    delete w;
    return true;
}

/* ---- textures ---- */

static D3DFORMAT d3d_format(int f) {
    switch (f) {
    case TEX_DXT1: return D3DFMT_DXT1;
    case TEX_DXT5: return D3DFMT_DXT5;
    default: return D3DFMT_A8R8G8B8;
    }
}

Texture* D3D9Renderer::create_texture(const TextureData& d) {
    D3D9Texture* t = new D3D9Texture();
    t->width = d.width;
    t->height = d.height;
    t->orig_width = d.orig_width;
    t->orig_height = d.orig_height;
    if (!dev_ || d.levels.empty()) { delete t; return NULL; }
    D3DFORMAT fmt = d3d_format(d.format);
    bool block = d.format != TEX_ARGB8;
#if defined(_XBOX)
    /* One level for now (no packed mip tail: D3D creates a one-level
     * texture unpacked, its base level at the base address). */
    UINT levels = 1;
    if (FAILED(dev_->CreateTexture(d.width, d.height, levels, 0, fmt, 0, &t->tex, NULL))) { delete t; return NULL; }
#else
    UINT levels = (UINT)d.levels.size();
    if (FAILED(dev_->CreateTexture(d.width, d.height, levels, 0, fmt, D3DPOOL_MANAGED, &t->tex, NULL))) { delete t; return NULL; }
#endif
    for (UINT lv = 0; lv < levels; ++lv) {
        int w = d.width >> lv, h = d.height >> lv;
        if (w < 1) w = 1;
        if (h < 1) h = 1;
        int rows = block ? (h + 3) / 4 : h;
        int pitch = block ? ((w + 3) / 4) * (d.format == TEX_DXT1 ? 8 : 16) : w * 4;
#if defined(_XBOX)
        /* Xenos tiles in 32x32 blocks (texels for uncompressed data).
         * Include the alignment in the cache budget. */
        int blocks_x = block ? (w + 3) / 4 : w;
        int blocks_y = rows;
        size_t bytes = (size_t)((blocks_x + 31) & ~31) * (size_t)((blocks_y + 31) & ~31) *
                       (size_t)(block ? (d.format == TEX_DXT1 ? 8 : 16) : 4);
        t->memory_bytes += (bytes + 4095) & ~(size_t)4095;
#else
        t->memory_bytes += (size_t)rows * (size_t)pitch;
#endif
        const std::vector<u8>& src = d.levels[lv];
        if ((int)src.size() < rows * pitch) break;
#if defined(_XBOX)
        /* Xenos reads DXT as 16-bit words and ARGB8 as 32-bit words, big
         * endian: swap the PC-order data, then tile it. */
        std::vector<u8> sw(src.begin(), src.begin() + rows * pitch);
        if (block) {
            for (size_t i = 0; i + 1 < sw.size(); i += 2) {
                u8 a = sw[i];
                sw[i] = sw[i + 1];
                sw[i + 1] = a;
            }
        } else {
            for (size_t i = 0; i + 3 < sw.size(); i += 4) {
                u8 a = sw[i], b = sw[i + 1];
                sw[i] = sw[i + 3];
                sw[i + 1] = sw[i + 2];
                sw[i + 2] = b;
                sw[i + 3] = a;
            }
        }
        D3DLOCKED_RECT lr;
        if (FAILED(t->tex->LockRect(lv, &lr, NULL, 0))) break;
        /* XGTILE_NONPACKED: without it a level of 16 texels or less is
         * written where the packed mip tail would hold it, and the GPU
         * (reading the unpacked base) sees nothing: every small texture
         * (the health bars' pieces, the 1x1 white of untextured draws) was
         * invisible. */
        XGTileTextureLevel(w, h, lv, XGGetGpuFormat(fmt), XGTILE_NONPACKED, lr.pBits, NULL, &sw[0], pitch, NULL);
        t->tex->UnlockRect(lv);
#else
        D3DLOCKED_RECT lr;
        if (FAILED(t->tex->LockRect(lv, &lr, NULL, 0))) break;
        for (int r = 0; r < rows; ++r) memcpy((u8*)lr.pBits + r * lr.Pitch, &src[r * pitch], pitch);
        t->tex->UnlockRect(lv);
#endif
    }
    return t;
}

void D3D9Renderer::destroy_texture(Texture* t) {
    if (!t) return;
    flush();
    D3D9Texture* d = (D3D9Texture*)t;
    if (tex_ == d) tex_ = NULL;
    if (gm_ == d) gm_ = NULL;
    if (vfx_on_ && vfx_.lut == t) { vfx_.lut = NULL; vfx_on_ = false; }
    if (screen_on_ && (screen_.noise == t || screen_.mask == t)) screen_on_ = false;
    if (fire_on_ && (fire_.noise1 == t || fire_.noise2 == t || fire_.noise_mask == t ||
        fire_.dist1 == t || fire_.dist2 == t || fire_.dist_mask == t)) fire_on_ = false;
    if (dev_) for (int stage = 0; stage < 9; ++stage) dev_->SetTexture(stage, NULL);
    dirty_ = true;
    RELEASE(d->tex);
    delete d;
}

/* ---- frame ---- */

void D3D9Renderer::begin(int width, int height, const Color& c) {
    if (width != width_ || height != height_) {
        release_group_target();
        release_screen();
    }
    screen_levels_ = 0;
    width_ = width;
    height_ = height;
    draws_ = 0;
    dev_->Clear(0, NULL, D3DCLEAR_TARGET,
                D3DCOLOR_ARGB((int)(c.a * 255 + 0.5f), (int)(c.r * 255 + 0.5f), (int)(c.g * 255 + 0.5f),
                              (int)(c.b * 255 + 0.5f)),
                1.0f, 0);
    dev_->BeginScene();
    dev_->SetVertexDeclaration(decl_);
    dev_->SetVertexShader(vs_);
    dev_->SetPixelShader(ps_);
    dev_->SetRenderState(D3DRS_CULLMODE, D3DCULL_NONE);
    dev_->SetRenderState(D3DRS_ZENABLE, FALSE);
    dev_->SetRenderState(D3DRS_ALPHABLENDENABLE, TRUE);
#if defined(_XBOX)
    /* pixel centres at .5 like the software renderer (D3D10 rules) */
    dev_->SetRenderState(D3DRS_HALFPIXELOFFSET, TRUE);
    float vp[4] = { 2.0f / width, -2.0f / height, -1.0f, 1.0f };
#else
    float vp[4] = { 2.0f / width, -2.0f / height, -1.0f - 1.0f / width, 1.0f + 1.0f / height };
#endif
    dev_->SetVertexShaderConstantF(0, vp, 1);
    float inv[4] = { 1.0f / width, 1.0f / height, 0, 0 };
    dev_->SetVertexShaderConstantF(1, inv, 1);
    clip_on_ = false;
    dirty_ = true;
    batch_.clear();
}

void D3D9Renderer::end() {
    if ((group_debug_ == 3 || group_debug_ == 5) && group_tex_) {
        /* The layer texture over a black frame. */
        set_clip(NULL);
        set_color_matrix(NULL);
        set_ripple(NULL);
        set_gradient_map(NULL, false);
        set_fire(NULL);
        set_vfx(NULL);
        group_view_.tex = group_tex_;
        group_view_.width = group_view_.orig_width = width_;
        group_view_.height = group_view_.orig_height = height_;
        float w = (float)width_, h = (float)height_;
        Vertex q[6];
        Vec2 p[4] = { Vec2(0, 0), Vec2(w, 0), Vec2(w, h), Vec2(0, h) };
        Vec2 uv[4] = { Vec2(0, 0), Vec2(1, 0), Vec2(1, 1), Vec2(0, 1) };
        static const int idx[6] = { 0, 1, 2, 0, 2, 3 };
        for (int i = 0; i < 6; ++i) q[i] = Vertex(p[idx[i]], uv[idx[i]], Color(0, 0, 0, 1));
        draw_triangles(NULL, q, 6, BLEND_MIX, 0);
        for (int i = 0; i < 6; ++i) q[i].color = Color(1, 1, 1, 1);
        draw_triangles(&group_view_, q, 6, BLEND_MIX, DRAW_NEAREST);
    }
    flush();
    tex_ = NULL;
    dev_->EndScene();
}

void D3D9Renderer::set_group_debug(int mode) {
    group_debug_ = mode;
    group_log_ = 4;
    log_info("d3d9: canvas group debug mode %d", mode);
}

/* ---- canvas groups ---- */

void D3D9Renderer::release_group_target() {
    group_view_.tex = NULL;
    RELEASE(group_rt_);
    RELEASE(group_tex_);
    RELEASE(main_rt_);
    group_failed_ = false;
}

bool D3D9Renderer::make_group_target() {
    if (group_tex_) return true;
    if (group_failed_ || !dev_) return false;
#if defined(_XBOX)
    /* EDRAM: the back buffer sits at tile 0 (no depth buffer); the group
     * surface goes after it. */
    D3DSURFACE_PARAMETERS sp;
    memset(&sp, 0, sizeof(sp));
    sp.Base = XGSurfaceSize(width_, height_, D3DFMT_A8R8G8B8, D3DMULTISAMPLE_NONE);
    if (FAILED(dev_->CreateRenderTarget(width_, height_, D3DFMT_A8R8G8B8, D3DMULTISAMPLE_NONE, 0, FALSE, &group_rt_,
                                        &sp)) ||
        FAILED(dev_->CreateTexture(width_, height_, 1, 0, D3DFMT_A8R8G8B8, 0, &group_tex_, NULL))) {
#else
    if (FAILED(dev_->CreateTexture(width_, height_, 1, D3DUSAGE_RENDERTARGET, D3DFMT_A8R8G8B8, D3DPOOL_DEFAULT,
                                   &group_tex_, NULL)) ||
        FAILED(group_tex_->GetSurfaceLevel(0, &group_rt_))) {
#endif
        log_warn("d3d9: no canvas group target; clip_children draws unmasked");
        RELEASE(group_rt_);
        RELEASE(group_tex_);
        group_failed_ = true;
        return false;
    }
    return true;
}

bool D3D9Renderer::begin_group(const Rect2& b) {
    if (group_debug_ == 1 || main_rt_ || !make_group_target()) return false;
    flush();
    if (FAILED(dev_->GetRenderTarget(0, &main_rt_))) {
        main_rt_ = NULL;
        return false;
    }
    /* The bounds in whole pixels, grown to the resolve granularity (Xenos
     * resolves 8x8-aligned rectangles; 32 keeps clear and resolve simple). */
    long x0 = (long)floorf(b.pos.x), y0 = (long)floorf(b.pos.y);
    long x1 = (long)ceilf(b.pos.x + b.size.x), y1 = (long)ceilf(b.pos.y + b.size.y);
    x0 &= ~31L;
    y0 &= ~31L;
    x1 = (x1 + 31) & ~31L;
    y1 = (y1 + 31) & ~31L;
    if (x0 < 0) x0 = 0;
    if (y0 < 0) y0 = 0;
    if (x1 > width_) x1 = width_;
    if (y1 > height_) y1 = height_;
    if (x1 < x0) x1 = x0;
    if (y1 < y0) y1 = y0;
    group_rect_.left = x0;
    group_rect_.top = y0;
    group_rect_.right = x1;
    group_rect_.bottom = y1;
    if (group_log_ > 0) {
        --group_log_;
        log_info("d3d9: group (%.1f, %.1f, %.1f x %.1f) -> rect %ld,%ld - %ld,%ld of %dx%d", b.pos.x, b.pos.y, b.size.x,
                 b.size.y, x0, y0, x1, y1, width_, height_);
    }
    dev_->SetRenderTarget(0, group_rt_);
    /* SetRenderTarget resets the viewport and scissor; apply_state sets the
     * scissor again. Clear the whole target (fast on both). */
    dev_->SetRenderState(D3DRS_SCISSORTESTENABLE, FALSE);
    dev_->Clear(0, NULL, D3DCLEAR_TARGET, 0, 1.0f, 0);
    dirty_ = true;
    return true;
}

void D3D9Renderer::end_group() {
    if (!main_rt_) return;
    flush();
#if defined(_XBOX)
    if (group_debug_ >= 4) {
        dev_->Resolve(D3DRESOLVE_RENDERTARGET0, NULL, group_tex_, NULL, 0, 0, NULL, 1.0f, 0, NULL);
    } else if (group_rect_.right > group_rect_.left && group_rect_.bottom > group_rect_.top) {
        D3DRECT src = { group_rect_.left, group_rect_.top, group_rect_.right, group_rect_.bottom };
        D3DPOINT dst = { group_rect_.left, group_rect_.top };
        dev_->Resolve(D3DRESOLVE_RENDERTARGET0, &src, group_tex_, &dst, 0, 0, NULL, 1.0f, 0, NULL);
    }
#endif
    dev_->SetRenderTarget(0, main_rt_);
    RELEASE(main_rt_);
    dirty_ = true;
}

/* ---- the screen copy ---- */

void D3D9Renderer::release_screen() {
    for (int i = 0; i < SCREEN_LEVELS; ++i) {
        RELEASE(screen_lv_[i]);
#if defined(_XBOX)
        RELEASE(screen_rt_[i]);
#endif
    }
    screen_levels_ = 0;
    screen_on_ = false;
    screen_failed_ = false;
}

/* Godot's back buffer mipmaps: each level half the one above, at least 1. */
void D3D9Renderer::level_size(int level, int* w, int* h) const {
    int x = width_, y = height_;
    for (int i = 0; i < level; ++i) {
        x = x > 1 ? x >> 1 : 1;
        y = y > 1 ? y >> 1 : 1;
    }
    *w = x;
    *h = y;
}

/* Level 0: the render target as drawn so far. */
bool D3D9Renderer::copy_screen() {
#if defined(_XBOX)
    if (!screen_lv_[0] && FAILED(dev_->CreateTexture(width_, height_, 1, 0, D3DFMT_A8R8G8B8, 0, &screen_lv_[0], NULL)))
        return false;
    /* The current target (EDRAM) into the texture; EDRAM is kept. */
    if (FAILED(dev_->Resolve(D3DRESOLVE_RENDERTARGET0, NULL, screen_lv_[0], NULL, 0, 0, NULL, 1.0f, 0, NULL)))
        return false;
#else
    if (!screen_lv_[0] && FAILED(dev_->CreateTexture(width_, height_, 1, D3DUSAGE_RENDERTARGET, D3DFMT_A8R8G8B8,
                                                     D3DPOOL_DEFAULT, &screen_lv_[0], NULL)))
        return false;
    IDirect3DSurface9 *src = NULL, *dst = NULL;
    bool ok = SUCCEEDED(dev_->GetRenderTarget(0, &src)) && SUCCEEDED(screen_lv_[0]->GetSurfaceLevel(0, &dst)) &&
              SUCCEEDED(dev_->StretchRect(src, NULL, dst, NULL, D3DTEXF_NONE));
    RELEASE(src);
    RELEASE(dst);
    if (!ok) return false;
#endif
    screen_levels_ = 1;
    return true;
}

/* Level `level` from the one above: a full-target quad through the canvas
 * shader's level pass (the 7x7 Gaussian of the bilinear reads, as
 * image.cpp's blur_mip_level), blending off. */
bool D3D9Renderer::screen_pass(int level) {
    int w, h;
    level_size(level, &w, &h);
    IDirect3DSurface9* prev = NULL;
    if (FAILED(dev_->GetRenderTarget(0, &prev))) return false;
#if defined(_XBOX)
    if (!screen_lv_[level] && FAILED(dev_->CreateTexture(w, h, 1, 0, D3DFMT_A8R8G8B8, 0, &screen_lv_[level], NULL))) {
        RELEASE(prev);
        return false;
    }
    if (!screen_rt_[level]) {
        /* EDRAM after the back buffer and the group surface (2048 tiles of
         * 5120 bytes in all). */
        DWORD full = XGSurfaceSize(width_, height_, D3DFMT_A8R8G8B8, D3DMULTISAMPLE_NONE);
        DWORD mine = XGSurfaceSize(w, h, D3DFMT_A8R8G8B8, D3DMULTISAMPLE_NONE);
        D3DSURFACE_PARAMETERS sp;
        memset(&sp, 0, sizeof(sp));
        sp.Base = full * 2;
        if (sp.Base + mine > 2048 ||
            FAILED(dev_->CreateRenderTarget(w, h, D3DFMT_A8R8G8B8, D3DMULTISAMPLE_NONE, 0, FALSE, &screen_rt_[level],
                                            &sp))) {
            screen_rt_[level] = NULL;
            RELEASE(prev);
            return false;
        }
    }
    dev_->SetRenderTarget(0, screen_rt_[level]);
#else
    if (!screen_lv_[level] && FAILED(dev_->CreateTexture(w, h, 1, D3DUSAGE_RENDERTARGET, D3DFMT_A8R8G8B8,
                                                         D3DPOOL_DEFAULT, &screen_lv_[level], NULL))) {
        RELEASE(prev);
        return false;
    }
    IDirect3DSurface9* dst = NULL;
    if (FAILED(screen_lv_[level]->GetSurfaceLevel(0, &dst))) {
        RELEASE(prev);
        return false;
    }
    dev_->SetTexture(10, NULL); /* never sample the target being drawn */
    dev_->SetRenderTarget(0, dst);
    RELEASE(dst);
#endif
    dev_->SetRenderState(D3DRS_SCISSORTESTENABLE, FALSE);
    dev_->SetRenderState(D3DRS_ALPHABLENDENABLE, FALSE);
#if defined(_XBOX)
    float vp[4] = { 2.0f / w, -2.0f / h, -1.0f, 1.0f };
#else
    float vp[4] = { 2.0f / w, -2.0f / h, -1.0f - 1.0f / w, 1.0f + 1.0f / h };
#endif
    dev_->SetVertexShaderConstantF(0, vp, 1);
    float inv[4] = { 1.0f / w, 1.0f / h, 0, 0 };
    dev_->SetVertexShaderConstantF(1, inv, 1);
    float pc[8] = { 3.0f, 0, 0, 0, 0, 0, 1.0f / w, 1.0f / h };
    dev_->SetPixelShaderConstantF(22, pc, 2);
    dev_->SetTexture(9, screen_lv_[level - 1]);
    dev_->SetSamplerState(9, D3DSAMP_ADDRESSU, D3DTADDRESS_CLAMP);
    dev_->SetSamplerState(9, D3DSAMP_ADDRESSV, D3DTADDRESS_CLAMP);
    dev_->SetSamplerState(9, D3DSAMP_MINFILTER, D3DTEXF_LINEAR);
    dev_->SetSamplerState(9, D3DSAMP_MAGFILTER, D3DTEXF_LINEAR);
    dev_->SetSamplerState(9, D3DSAMP_MIPFILTER, D3DTEXF_NONE);
    GpuVertex q[6];
    float px[6] = { 0, (float)w, (float)w, 0, (float)w, 0 }, py[6] = { 0, 0, (float)h, 0, (float)h, (float)h };
    for (int i = 0; i < 6; ++i) {
        q[i].x = px[i];
        q[i].y = py[i];
        q[i].u = px[i] / w;
        q[i].v = py[i] / h;
        q[i].r = q[i].g = q[i].b = q[i].a = 1.0f;
    }
    dev_->DrawPrimitiveUP(D3DPT_TRIANGLELIST, 2, q, sizeof(GpuVertex));
    ++draws_;
#if defined(_XBOX)
    dev_->Resolve(D3DRESOLVE_RENDERTARGET0, NULL, screen_lv_[level], NULL, 0, 0, NULL, 1.0f, 0, NULL);
#endif
    dev_->SetRenderTarget(0, prev);
    RELEASE(prev);
    dev_->SetTexture(9, NULL);
    dev_->SetRenderState(D3DRS_ALPHABLENDENABLE, TRUE);
#if defined(_XBOX)
    float fvp[4] = { 2.0f / width_, -2.0f / height_, -1.0f, 1.0f };
#else
    float fvp[4] = { 2.0f / width_, -2.0f / height_, -1.0f - 1.0f / width_, 1.0f + 1.0f / height_ };
#endif
    dev_->SetVertexShaderConstantF(0, fvp, 1);
    float finv[4] = { 1.0f / width_, 1.0f / height_, 0, 0 };
    dev_->SetVertexShaderConstantF(1, finv, 1);
    dirty_ = true;
    screen_levels_ = level + 1;
    return true;
}

bool D3D9Renderer::set_screen(const ScreenParams* p) {
    if (!p) {
        if (screen_on_) {
            flush();
            screen_on_ = false;
            dirty_ = true;
        }
        return true;
    }
    if (screen_failed_ || !dev_) return false;
    int need = p->mode == SCREEN_DARK ? (int)ceilf(p->lod) : 0;
    if (need > SCREEN_LEVELS - 1) need = SCREEN_LEVELS - 1;
    if (screen_levels_ <= need) {
        flush();
        if (screen_levels_ == 0 && !copy_screen()) {
            log_warn("d3d9: no screen copy; screen-reading shaders are not drawn");
            screen_failed_ = true;
            return false;
        }
        while (screen_levels_ <= need)
            if (!screen_pass(screen_levels_)) {
                log_warn("d3d9: no screen copy level %d; screen-reading shaders are not drawn", screen_levels_);
                screen_failed_ = true;
                return false;
            }
    }
    if (!screen_on_ || memcmp(p, &screen_, sizeof(screen_)) != 0) {
        flush();
        screen_on_ = true;
        screen_ = *p;
        dirty_ = true;
    }
    return true;
}

void D3D9Renderer::set_clip(const Rect2* r) {
    bool on = r != NULL;
    RECT rc;
    memset(&rc, 0, sizeof(rc));
    if (r) {
        rc.left = (LONG)(r->pos.x + 0.5f);
        rc.top = (LONG)(r->pos.y + 0.5f);
        rc.right = (LONG)(r->pos.x + r->size.x + 0.5f);
        rc.bottom = (LONG)(r->pos.y + r->size.y + 0.5f);
    }
    if (on == clip_on_ && (!on || memcmp(&rc, &clip_, sizeof(rc)) == 0)) return;
    flush();
    clip_on_ = on;
    clip_ = rc;
    dirty_ = true;
}

void D3D9Renderer::set_msdf(float px_range, float outline) {
    if (msdf_[0] == px_range && msdf_[1] == outline) return;
    flush();
    msdf_[0] = px_range;
    msdf_[1] = outline;
    dirty_ = true;
}

void D3D9Renderer::set_color_matrix(const float* m) {
    bool on = m != NULL;
    if (on == cm_on_ && (!on || memcmp(m, cm_, sizeof(cm_)) == 0)) return;
    flush();
    cm_on_ = on;
    if (m) memcpy(cm_, m, sizeof(cm_));
    dirty_ = true;
}

void D3D9Renderer::set_ripple(const float* p) {
    bool on = p != NULL;
    if (on == ripple_on_ && (!on || memcmp(p, ripple_, sizeof(ripple_)) == 0)) return;
    flush();
    ripple_on_ = on;
    if (p) memcpy(ripple_, p, sizeof(ripple_));
    dirty_ = true;
}

void D3D9Renderer::set_gradient_map(Texture* lut, bool alpha_from_red) {
    D3D9Texture* t = (D3D9Texture*)lut;
    if (t == gm_ && (!t || alpha_from_red == gm_red_)) return;
    flush();
    gm_ = t;
    gm_red_ = alpha_from_red;
    dirty_ = true;
}

void D3D9Renderer::set_fire(const FireParams* p) {
    if (!p && !fire_on_) return;
    if (p && fire_on_ && memcmp(p, &fire_, sizeof(fire_)) == 0) return;
    flush();
    fire_on_ = p != NULL;
    if (p) fire_ = *p;
    dirty_ = true;
}

void D3D9Renderer::set_vfx(const VfxParams* p) {
    if (!p && !vfx_on_) return;
    if (p && vfx_on_ && memcmp(p, &vfx_, sizeof(vfx_)) == 0) return;
    flush();
    vfx_on_ = p != NULL;
    if (p) vfx_ = *p;
    dirty_ = true;
}

void D3D9Renderer::draw_triangles(Texture* tex, const Vertex* v, int count, BlendMode blend, int flags) {
    D3D9Texture* t = (D3D9Texture*)tex;
    if (t != tex_ || blend != blend_ || flags != flags_) {
        flush();
        tex_ = t;
        blend_ = blend;
        flags_ = flags;
        dirty_ = true;
    }
    for (int i = 0; i < count; ++i) {
        GpuVertex g;
        g.x = v[i].pos.x;
        g.y = v[i].pos.y;
        g.u = v[i].uv.x;
        g.v = v[i].uv.y;
        g.r = v[i].color.r;
        g.g = v[i].color.g;
        g.b = v[i].color.b;
        g.a = v[i].color.a;
        batch_.push_back(g);
    }
}

void D3D9Renderer::apply_state() {
    bool group = (flags_ & DRAW_GROUP) && group_tex_ && !main_rt_;
    IDirect3DTexture9* t = tex_ && tex_->tex ? tex_->tex : white_;
    if (group && group_debug_ == 2) t = white_;
    dev_->SetTexture(0, t);
    DWORD addr = (flags_ & DRAW_REPEAT) ? D3DTADDRESS_WRAP : D3DTADDRESS_CLAMP;
    DWORD filt = (flags_ & DRAW_NEAREST) ? D3DTEXF_POINT : D3DTEXF_LINEAR;
    dev_->SetSamplerState(0, D3DSAMP_ADDRESSU, addr);
    dev_->SetSamplerState(0, D3DSAMP_ADDRESSV, addr);
    dev_->SetSamplerState(0, D3DSAMP_MINFILTER, filt);
    dev_->SetSamplerState(0, D3DSAMP_MAGFILTER, filt);
    dev_->SetSamplerState(0, D3DSAMP_MIPFILTER, D3DTEXF_NONE);
    if (group) {
        dev_->SetTexture(1, group_tex_);
        dev_->SetSamplerState(1, D3DSAMP_ADDRESSU, D3DTADDRESS_CLAMP);
        dev_->SetSamplerState(1, D3DSAMP_ADDRESSV, D3DTADDRESS_CLAMP);
        dev_->SetSamplerState(1, D3DSAMP_MINFILTER, D3DTEXF_POINT);
        dev_->SetSamplerState(1, D3DSAMP_MAGFILTER, D3DTEXF_POINT);
        dev_->SetSamplerState(1, D3DSAMP_MIPFILTER, D3DTEXF_NONE);
    } else {
        /* never sample the target being drawn */
        dev_->SetTexture(1, NULL);
    }
    /* The stepped fire's textures (s3..s8): noises and distortions
     * repeat, the masks clamp; unset ones are white. */
    if (fire_on_) {
        Texture* ft[6] = { fire_.noise1, fire_.noise2, fire_.noise_mask, fire_.dist1, fire_.dist2, fire_.dist_mask };
        for (int k = 0; k < 6; ++k) {
            D3D9Texture* t = (D3D9Texture*)ft[k];
            DWORD stage = (DWORD)(3 + k);
            dev_->SetTexture(stage, t && t->tex ? t->tex : white_);
            DWORD a = (k == 2 || k == 5) ? D3DTADDRESS_CLAMP : D3DTADDRESS_WRAP;
            dev_->SetSamplerState(stage, D3DSAMP_ADDRESSU, a);
            dev_->SetSamplerState(stage, D3DSAMP_ADDRESSV, a);
            dev_->SetSamplerState(stage, D3DSAMP_MINFILTER, D3DTEXF_LINEAR);
            dev_->SetSamplerState(stage, D3DSAMP_MAGFILTER, D3DTEXF_LINEAR);
            dev_->SetSamplerState(stage, D3DSAMP_MIPFILTER, D3DTEXF_NONE);
        }
    } else {
        for (DWORD stage = 3; stage < 9; ++stage) dev_->SetTexture(stage, NULL);
    }
    /* The gradient map's lut (s2): 256x1, linear, clamped (the erosion
     * shaders' lut uses it too). */
    D3D9Texture* lut = gm_ && gm_->tex ? gm_ : (vfx_on_ && vfx_.lut ? (D3D9Texture*)vfx_.lut : NULL);
    dev_->SetTexture(2, lut && lut->tex ? lut->tex : NULL);
    if (lut && lut->tex) {
        dev_->SetSamplerState(2, D3DSAMP_ADDRESSU, D3DTADDRESS_CLAMP);
        dev_->SetSamplerState(2, D3DSAMP_ADDRESSV, D3DTADDRESS_CLAMP);
        dev_->SetSamplerState(2, D3DSAMP_MINFILTER, D3DTEXF_LINEAR);
        dev_->SetSamplerState(2, D3DSAMP_MAGFILTER, D3DTEXF_LINEAR);
        dev_->SetSamplerState(2, D3DSAMP_MIPFILTER, D3DTEXF_NONE);
    }
    switch (blend_) {
    case BLEND_ADD:
        dev_->SetRenderState(D3DRS_BLENDOP, D3DBLENDOP_ADD);
        dev_->SetRenderState(D3DRS_SRCBLEND, D3DBLEND_ONE);
        dev_->SetRenderState(D3DRS_DESTBLEND, D3DBLEND_ONE);
        break;
    case BLEND_SUB:
        dev_->SetRenderState(D3DRS_BLENDOP, D3DBLENDOP_REVSUBTRACT);
        dev_->SetRenderState(D3DRS_SRCBLEND, D3DBLEND_ONE);
        dev_->SetRenderState(D3DRS_DESTBLEND, D3DBLEND_ONE);
        break;
    case BLEND_MUL:
        dev_->SetRenderState(D3DRS_BLENDOP, D3DBLENDOP_ADD);
        dev_->SetRenderState(D3DRS_SRCBLEND, D3DBLEND_DESTCOLOR);
        dev_->SetRenderState(D3DRS_DESTBLEND, D3DBLEND_ZERO);
        break;
    default:
        dev_->SetRenderState(D3DRS_BLENDOP, D3DBLENDOP_ADD);
        dev_->SetRenderState(D3DRS_SRCBLEND, D3DBLEND_ONE);
        dev_->SetRenderState(D3DRS_DESTBLEND, D3DBLEND_INVSRCALPHA);
        break;
    }
    dev_->SetRenderState(D3DRS_SCISSORTESTENABLE, clip_on_ ? TRUE : FALSE);
    if (clip_on_) dev_->SetScissorRect(&clip_);
    float tw = 1, th = 1;
    if (tex_) {
        tw = (float)tex_->width;
        th = (float)tex_->height;
    }
    float c[96] = { cm_on_ ? 1.0f : 0.0f, (flags_ & DRAW_MSDF) ? 1.0f : 0.0f, group ? 1.0f : 0.0f,
                    ripple_on_ ? 1.0f : 0.0f,
                    msdf_[0], msdf_[1], tw, th,
                    cm_[0], cm_[1], cm_[2], cm_[3],
                    cm_[4], cm_[5], cm_[6], cm_[7],
                    cm_[8], cm_[9], cm_[10], cm_[11],
                    ripple_[0], ripple_[1], ripple_[2], ripple_[3],
                    gm_ && gm_->tex ? 1.0f : 0.0f, gm_red_ ? 1.0f : 0.0f, 0.0f, 0.0f };
    /* c7..c18: the stepped fire. */
    c[28] = fire_on_ ? 1.0f : 0.0f;
    c[29] = c[30] = c[31] = 0.0f;
    const float* fv[11] = { fire_.noise1_uv, fire_.noise2_uv, fire_.mask_uv, fire_.dist1_uv, fire_.dist2_uv,
                            fire_.dist_mask_uv, fire_.strength, fire_.dist_strength, fire_.steps, fire_.inner,
                            fire_.outer };
    for (int k = 0; k < 11; ++k)
        for (int j = 0; j < 4; ++j) c[32 + k * 4 + j] = fire_on_ ? fv[k][j] : 0.0f;
    /* c19..c21: the erosion particle shaders. */
    c[76] = vfx_on_ ? 1.0f : 0.0f;
    c[77] = vfx_on_ ? vfx_.polar : 0.0f;
    c[78] = vfx_on_ && vfx_.lut ? 1.0f : 0.0f;
    c[79] = 0.0f;
    for (int j = 0; j < 4; ++j) {
        c[80 + j] = vfx_on_ ? vfx_.erosion[j] : 0.0f;
        c[84 + j] = vfx_on_ ? vfx_.distortion[j] : 0.0f;
    }
    /* c22..c23 and s9..s12: the screen copy (ScreenParams). */
    if (screen_on_ && screen_levels_ > 0) {
        int l0 = 0;
        float f = 0;
        if (screen_.mode == SCREEN_DARK) {
            float lod = screen_.lod < 0 ? 0 : screen_.lod;
            if (lod > (float)(screen_levels_ - 1)) lod = (float)(screen_levels_ - 1);
            l0 = (int)floorf(lod);
            f = lod - (float)l0;
        }
        int l1 = l0 + 1 < screen_levels_ ? l0 + 1 : l0;
        IDirect3DTexture9* st[4] = { screen_lv_[l0], screen_lv_[l1], white_, white_ };
        if (screen_.noise && ((D3D9Texture*)screen_.noise)->tex) st[2] = ((D3D9Texture*)screen_.noise)->tex;
        if (screen_.mask && ((D3D9Texture*)screen_.mask)->tex) st[3] = ((D3D9Texture*)screen_.mask)->tex;
        for (int k = 0; k < 4; ++k) {
            DWORD stage = (DWORD)(9 + k);
            DWORD a = k == 2 ? D3DTADDRESS_WRAP : D3DTADDRESS_CLAMP;
            dev_->SetTexture(stage, st[k]);
            dev_->SetSamplerState(stage, D3DSAMP_ADDRESSU, a);
            dev_->SetSamplerState(stage, D3DSAMP_ADDRESSV, a);
            dev_->SetSamplerState(stage, D3DSAMP_MINFILTER, D3DTEXF_LINEAR);
            dev_->SetSamplerState(stage, D3DSAMP_MAGFILTER, D3DTEXF_LINEAR);
            dev_->SetSamplerState(stage, D3DSAMP_MIPFILTER, D3DTEXF_NONE);
        }
        c[88] = (float)screen_.mode;
        c[89] = f;
        c[90] = screen_.mix;
        c[91] = screen_.strength;
        c[92] = screen_.noise_offset[0];
        c[93] = screen_.noise_offset[1];
    } else {
        for (DWORD stage = 9; stage < 13; ++stage) dev_->SetTexture(stage, NULL);
        for (int k = 88; k < 94; ++k) c[k] = 0.0f;
    }
    c[94] = c[95] = 0.0f;
    dev_->SetPixelShaderConstantF(0, c, 24);
    dirty_ = false;
}

void D3D9Renderer::flush() {
    if (batch_.empty() || !dev_) return;
    if (dirty_) apply_state();
    dev_->DrawPrimitiveUP(D3DPT_TRIANGLELIST, (UINT)(batch_.size() / 3), &batch_[0], sizeof(GpuVertex));
    ++draws_;
    batch_.clear();
}

} /* namespace spire */

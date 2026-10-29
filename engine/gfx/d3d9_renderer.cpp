#include "d3d9_renderer.h"

#include "../core/pack.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

#if defined(_XBOX)
#include <xgraphics.h>
#if !defined(XGTILE_NONPACKED)
#define XGTILE_NONPACKED 0x1
#endif
#include "canvas_ps.h" /* g_ps_main, from fxc /Fh /Vn g_ps_main */
#include "canvas_plain_ps.h" /* g_ps_plain */
#include "canvas_text_ps.h" /* g_ps_text */
#include "canvas_vs.h" /* g_vs_main */
#include "effects_ps.h" /* g_effect_ps[id], from effects_build.cmd */
#else
#include <d3dcompiler.h>
static const char* const CANVAS_HLSL[] = {
#include "shaders/canvas_hlsl.inc"
    NULL
};
static const char* const EFFECTS_HLSL[] = {
#include "shaders/effects_hlsl.inc"
    NULL
};
#endif

namespace spire {

D3D9Renderer::D3D9Renderer()
    : dev_(NULL), vs_(NULL), ps_(NULL), ps_plain_(NULL), ps_text_(NULL), decl_(NULL), white_(NULL), batch_(new Vertex[BATCH_MAX]), batch_count_(0), tex_(NULL), blend_(BLEND_MIX), flags_(0),
      cm_on_(false), ripple_on_(false), gm_(NULL), gm_red_(false), fire_on_(false), vfx_on_(false), clip_on_(false), dirty_(true), group_rt_(NULL), group_tex_(NULL), main_rt_(NULL),
      group_failed_(false), group_debug_(0), group_log_(0), screen_on_(false), screen_levels_(0),
      screen_failed_(false), width_(0), height_(0), draws_(0), state_ticks_(0), submit_ticks_(0) {
    /* the batch is drawn with GpuVertex's declaration */
    typedef char vertex_layout_matches[sizeof(Vertex) == sizeof(GpuVertex) ? 1 : -1];
    (void)sizeof(vertex_layout_matches);
    memset(&screen_, 0, sizeof(screen_));
    memset(&effect_, 0, sizeof(effect_));
    effect_on_ = false;
    for (int i = 0; i < EFFECT_COUNT; ++i) effect_ps_[i] = NULL;
    cap_w_ = cap_h_ = 0;
#if !defined(_XBOX)
    capture_sys_ = NULL;
#endif
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

D3D9Renderer::~D3D9Renderer() {
    shutdown();
    delete[] batch_;
}

#define RELEASE(p)          \
    do {                    \
        if (p) p->Release(); \
        p = NULL;           \
    } while (0)

void D3D9Renderer::release_retired(bool all) {
    size_t kept = 0;
    for (size_t i = 0; i < retired_.size(); ++i) {
#if defined(_XBOX)
        if (!all && dev_ && dev_->IsFencePending(retired_[i].fence)) {
            retired_[kept++] = retired_[i];
            continue;
        }
#endif
        RELEASE(retired_[i].tex);
    }
    (void)all;
    retired_.resize(kept);
}

void D3D9Renderer::shutdown() {
#if defined(_XBOX)
    if (dev_ && !retired_.empty()) dev_->BlockUntilIdle();
#endif
    release_retired(true);
    release_group_target();
    release_screen();
    for (int i = 0; i < EFFECT_COUNT; ++i) RELEASE(effect_ps_[i]);
    RELEASE(vs_);
    RELEASE(ps_);
    RELEASE(ps_plain_);
    RELEASE(ps_text_);
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
    forget_state();
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
    if (FAILED(dev_->CreatePixelShader((const DWORD*)g_ps_plain, &ps_plain_))) ps_plain_ = NULL;
    if (FAILED(dev_->CreatePixelShader((const DWORD*)g_ps_text, &ps_text_))) ps_text_ = NULL;
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
    const char* lean[2] = { "ps_plain", "ps_text" };
    IDirect3DPixelShader9** lean_ps[2] = { &ps_plain_, &ps_text_ };
    for (int i = 0; i < 2; ++i) {
        ID3DBlob* b = NULL;
        std::string err;
        if (!compile(src, lean[i], "ps_3_0", &b, &err)) {
            log_warn("d3d9: %s: %s", lean[i], err.c_str());
            continue;
        }
        if (FAILED(dev_->CreatePixelShader((const DWORD*)b->GetBufferPointer(), lean_ps[i]))) *lean_ps[i] = NULL;
        b->Release();
    }
#endif
    /* The effect shaders; one that does not load leaves its effect off
     * (set_effect: false, the item is not drawn). */
#if defined(_XBOX)
    for (int i = 1; i < EFFECT_COUNT && i < g_effect_ps_count; ++i)
        if (g_effect_ps[i] && FAILED(dev_->CreatePixelShader((const DWORD*)g_effect_ps[i], &effect_ps_[i])))
            effect_ps_[i] = NULL;
#else
    std::string esrc;
    for (int i = 0; EFFECTS_HLSL[i]; ++i) esrc += EFFECTS_HLSL[i];
    for (int i = 1; i < EFFECT_COUNT; ++i) {
        ID3DBlob* b = NULL;
        std::string err, entry = format("ps_e%d", i);
        if (!compile(esrc, entry.c_str(), "ps_3_0", &b, &err)) {
            log_warn("d3d9: effect %d: %s", i, err.c_str());
            continue;
        }
        if (FAILED(dev_->CreatePixelShader((const DWORD*)b->GetBufferPointer(), &effect_ps_[i]))) effect_ps_[i] = NULL;
        b->Release();
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
    t->format = d.format;
    if (!dev_ || d.levels.empty()) { delete t; return NULL; }
    D3DFORMAT fmt = d3d_format(d.format);
    bool block = d.format != TEX_ARGB8;
#if defined(_XBOX)
    /* One level for now (no packed mip tail: D3D creates a one-level
     * texture unpacked, its base level at the base address). */
    UINT levels = 1;
    HRESULT hr = dev_->CreateTexture(d.width, d.height, levels, 0, fmt, 0, &t->tex, NULL);
    if (FAILED(hr)) {
        /* drawn as nothing: logged (the first few) with the memory left */
        static int logged = 0;
        if (logged < 20) {
            ++logged;
            MEMORYSTATUS ms;
            GlobalMemoryStatus(&ms);
            log_warn("d3d9: CreateTexture %dx%d format %d failed (%08lx), %u MB free", d.width, d.height, d.format,
                     (unsigned long)hr, (unsigned)(ms.dwAvailPhys >> 20));
        }
        delete t;
        return NULL;
    }
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
    for (int k = 0; effect_on_ && k < 5; ++k)
        if (effect_.tex[k] == t) effect_on_ = false;
    if (fire_on_ && (fire_.noise1 == t || fire_.noise2 == t || fire_.noise_mask == t ||
        fire_.dist1 == t || fire_.dist2 == t || fire_.dist_mask == t)) fire_on_ = false;
    if (dev_) for (int stage = 0; stage < 9; ++stage) dev_->SetTexture(stage, NULL);
    dirty_ = true;
    forget_state();
#if defined(_XBOX)
    if (dev_ && d->tex) {
        Retired r;
        r.tex = d->tex;
        r.fence = dev_->InsertFence();
        retired_.push_back(r);
        d->tex = NULL;
    }
#endif
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
    release_retired(false);
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
    forget_state();
    batch_count_ = 0;
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
#if !defined(_XBOX)
    RELEASE(capture_sys_);
#endif
    cap_w_ = cap_h_ = 0;
    group_failed_ = false;
}

/* ---- captures (SubViewport): the group target, copied out ---- */

bool D3D9Renderer::begin_capture(int w, int h) {
    if (main_rt_ || cap_w_ || w <= 0 || h <= 0 || w > width_ || h > height_ || !make_group_target()) return false;
    flush();
    if (FAILED(dev_->GetRenderTarget(0, &main_rt_))) {
        main_rt_ = NULL;
        return false;
    }
    dev_->SetRenderTarget(0, group_rt_);
    dev_->SetRenderState(D3DRS_SCISSORTESTENABLE, FALSE);
    dev_->Clear(0, NULL, D3DCLEAR_TARGET, 0, 1.0f, 0);
    cap_w_ = w;
    cap_h_ = h;
    dirty_ = true;
    forget_state();
    return true;
}

Texture* D3D9Renderer::end_capture(Texture* reuse) {
    if (!cap_w_ || !main_rt_) return NULL;
    flush();
    int w = cap_w_, h = cap_h_;
    cap_w_ = cap_h_ = 0;
    D3D9Texture* t = (D3D9Texture*)reuse;
    if (t && (t->width != w || t->height != h)) {
        destroy_texture(t);
        t = NULL;
    }
    bool fresh = !t;
    if (fresh) {
        t = new D3D9Texture();
        t->width = t->orig_width = w;
        t->height = t->orig_height = h;
        t->premultiplied = true;
        t->memory_bytes = (size_t)w * h * 4;
#if defined(_XBOX)
        if (FAILED(dev_->CreateTexture(w, h, 1, 0, D3DFMT_A8R8G8B8, 0, &t->tex, NULL))) t->tex = NULL;
#else
        if (FAILED(dev_->CreateTexture(w, h, 1, 0, D3DFMT_A8R8G8B8, D3DPOOL_MANAGED, &t->tex, NULL))) t->tex = NULL;
#endif
    }
    bool ok = t->tex != NULL;
#if defined(_XBOX)
    if (ok) {
        D3DRECT src = { 0, 0, w, h };
        D3DPOINT dst = { 0, 0 };
        ok = SUCCEEDED(dev_->Resolve(D3DRESOLVE_RENDERTARGET0, &src, t->tex, &dst, 0, 0, NULL, 1.0f, 0, NULL));
    }
#else
    /* Read back through system memory into a managed texture (survives a
     * device reset). */
    if (ok && !capture_sys_ &&
        FAILED(dev_->CreateOffscreenPlainSurface(width_, height_, D3DFMT_A8R8G8B8, D3DPOOL_SYSTEMMEM, &capture_sys_, NULL)))
        capture_sys_ = NULL;
    ok = ok && capture_sys_ && SUCCEEDED(dev_->GetRenderTargetData(group_rt_, capture_sys_));
    if (ok) {
        D3DLOCKED_RECT src, dst;
        RECT r = { 0, 0, w, h };
        ok = SUCCEEDED(capture_sys_->LockRect(&src, &r, D3DLOCK_READONLY));
        if (ok) {
            if (SUCCEEDED(t->tex->LockRect(0, &dst, NULL, 0))) {
                for (int y = 0; y < h; ++y)
                    memcpy((u8*)dst.pBits + (size_t)y * dst.Pitch, (const u8*)src.pBits + (size_t)y * src.Pitch, (size_t)w * 4);
                t->tex->UnlockRect(0);
            } else {
                ok = false;
            }
            capture_sys_->UnlockRect();
        }
    }
#endif
    dev_->SetRenderTarget(0, main_rt_);
    RELEASE(main_rt_);
    dirty_ = true;
    forget_state();
    if (!ok) {
        if (fresh) destroy_texture(t);
        return fresh ? NULL : t;
    }
    return t;
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
    forget_state();
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
    forget_state();
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
    dev_->SetPixelShader(ps_); /* the canvas shader's level pass, whatever the draws use */
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
    forget_state();
    screen_levels_ = level + 1;
    return true;
}

bool D3D9Renderer::set_effect(const EffectParams* e) {
    if (!e) {
        if (effect_on_) {
            flush();
            effect_on_ = false;
            dirty_ = true;
        }
        return true;
    }
    if (e->id <= 0 || e->id >= EFFECT_COUNT || !effect_ps_[e->id]) return false;
    if (!effect_on_ || memcmp(e, &effect_, sizeof(effect_)) != 0) {
        flush();
        effect_on_ = true;
        effect_ = *e;
        dirty_ = true;
    }
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
    int need = p->mode == SCREEN_DARK || p->mode == SCREEN_GLASS ? (int)ceilf(p->lod) : 0;
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
    while (count > 0) {
        int n = count < BATCH_MAX ? count : BATCH_MAX;
        memcpy(reserve(tex, n, blend, flags), v, (size_t)n * sizeof(Vertex));
        v += n;
        count -= n;
    }
}

Vertex* D3D9Renderer::reserve(Texture* tex, int count, BlendMode blend, int flags) {
    D3D9Texture* t = (D3D9Texture*)tex;
    if (t != tex_ || blend != blend_ || flags != flags_) {
        flush();
        tex_ = t;
        blend_ = blend;
        flags_ = flags;
        dirty_ = true;
    }
    if (batch_count_ + count > BATCH_MAX) flush();
    Vertex* v = batch_ + batch_count_;
    batch_count_ += count;
    return v;
}

/* ---- the device state cache ---- */

static IDirect3DBaseTexture9* const UNKNOWN_TEX = (IDirect3DBaseTexture9*)1;
static IDirect3DPixelShader9* const UNKNOWN_PS = (IDirect3DPixelShader9*)1;
static const DWORD UNKNOWN_STATE = 0xFFFFFFFF;

void D3D9Renderer::forget_state() {
    for (int s = 0; s < CACHED_STAGES; ++s) {
        c_tex_[s] = UNKNOWN_TEX;
        for (int k = 0; k < 5; ++k) c_samp_[s][k] = UNKNOWN_STATE;
    }
    c_ps_ = UNKNOWN_PS;
    for (int k = 0; k < 4; ++k) c_rs_[k] = UNKNOWN_STATE;
    c_clip_known_ = false;
    for (int r = 0; r < CACHED_CONSTANTS; ++r) c_psc_known_[r] = false;
}

void D3D9Renderer::set_tex(DWORD stage, IDirect3DBaseTexture9* t) {
    if (stage < (DWORD)CACHED_STAGES) {
        if (c_tex_[stage] == t) return;
        c_tex_[stage] = t;
    }
    dev_->SetTexture(stage, t);
}

void D3D9Renderer::set_samp(DWORD stage, D3DSAMPLERSTATETYPE type, DWORD v) {
    int k = type == D3DSAMP_ADDRESSU ? 0 : type == D3DSAMP_ADDRESSV ? 1 : type == D3DSAMP_MAGFILTER ? 2
          : type == D3DSAMP_MINFILTER ? 3 : type == D3DSAMP_MIPFILTER ? 4 : -1;
    if (k >= 0 && stage < (DWORD)CACHED_STAGES) {
        if (c_samp_[stage][k] == v) return;
        c_samp_[stage][k] = v;
    }
    dev_->SetSamplerState(stage, type, v);
}

void D3D9Renderer::set_rs(D3DRENDERSTATETYPE type, DWORD v) {
    int k = type == D3DRS_BLENDOP ? 0 : type == D3DRS_SRCBLEND ? 1 : type == D3DRS_DESTBLEND ? 2
          : type == D3DRS_SCISSORTESTENABLE ? 3 : -1;
    if (k >= 0) {
        if (c_rs_[k] == v) return;
        c_rs_[k] = v;
    }
    dev_->SetRenderState(type, v);
}

/* Only the registers that differ, in runs. */
void D3D9Renderer::set_psc(UINT reg, const float* v, UINT count) {
    UINT i = 0;
    while (i < count) {
        UINT r = reg + i;
        bool same = r < (UINT)CACHED_CONSTANTS && c_psc_known_[r] && memcmp(c_psc_[r], v + i * 4, sizeof(float) * 4) == 0;
        if (same) {
            ++i;
            continue;
        }
        UINT j = i;
        while (j < count) {
            UINT rj = reg + j;
            if (rj < (UINT)CACHED_CONSTANTS && c_psc_known_[rj] && memcmp(c_psc_[rj], v + j * 4, sizeof(float) * 4) == 0) break;
            if (rj < (UINT)CACHED_CONSTANTS) {
                memcpy(c_psc_[rj], v + j * 4, sizeof(float) * 4);
                c_psc_known_[rj] = true;
            }
            ++j;
        }
        dev_->SetPixelShaderConstantF(reg + i, v + i * 4, j - i);
        i = j;
    }
}

void D3D9Renderer::apply_state() {
    bool group = (flags_ & DRAW_GROUP) && group_tex_ && !main_rt_;
    IDirect3DTexture9* t = tex_ && tex_->tex ? tex_->tex : white_;
    if (group && group_debug_ == 2) t = white_;
    set_tex(0, t);
    DWORD addr = (flags_ & DRAW_REPEAT) ? D3DTADDRESS_WRAP : D3DTADDRESS_CLAMP;
    DWORD filt = (flags_ & DRAW_NEAREST) ? D3DTEXF_POINT : D3DTEXF_LINEAR;
    set_samp(0, D3DSAMP_ADDRESSU, addr);
    set_samp(0, D3DSAMP_ADDRESSV, addr);
    set_samp(0, D3DSAMP_MINFILTER, filt);
    set_samp(0, D3DSAMP_MAGFILTER, filt);
    set_samp(0, D3DSAMP_MIPFILTER, D3DTEXF_NONE);
    if (group) {
        set_tex(1, group_tex_);
        set_samp(1, D3DSAMP_ADDRESSU, D3DTADDRESS_CLAMP);
        set_samp(1, D3DSAMP_ADDRESSV, D3DTADDRESS_CLAMP);
        set_samp(1, D3DSAMP_MINFILTER, D3DTEXF_POINT);
        set_samp(1, D3DSAMP_MAGFILTER, D3DTEXF_POINT);
        set_samp(1, D3DSAMP_MIPFILTER, D3DTEXF_NONE);
    } else {
        /* never sample the target being drawn */
        set_tex(1, NULL);
    }
    IDirect3DPixelShader9* shader = effect_on_ && effect_ps_[effect_.id] ? effect_ps_[effect_.id] : ps_;
    if (shader == ps_ && !fire_on_ && !vfx_on_ && !(screen_on_ && screen_levels_ > 0) && !cm_on_ && !ripple_on_ &&
        !(gm_ && gm_->tex) && !group) {
        IDirect3DPixelShader9* lean = (flags_ & DRAW_MSDF) ? ps_text_ : ps_plain_;
        if (lean) shader = lean;
    }
    if (shader != c_ps_) {
        dev_->SetPixelShader(shader);
        c_ps_ = shader;
    }
    /* The stepped fire's textures (s3..s8): noises and distortions
     * repeat, the masks clamp; unset ones are white. */
    if (fire_on_) {
        Texture* ft[6] = { fire_.noise1, fire_.noise2, fire_.noise_mask, fire_.dist1, fire_.dist2, fire_.dist_mask };
        for (int k = 0; k < 6; ++k) {
            D3D9Texture* t = (D3D9Texture*)ft[k];
            DWORD stage = (DWORD)(3 + k);
            set_tex(stage, t && t->tex ? t->tex : white_);
            DWORD a = (k == 2 || k == 5) ? D3DTADDRESS_CLAMP : D3DTADDRESS_WRAP;
            set_samp(stage, D3DSAMP_ADDRESSU, a);
            set_samp(stage, D3DSAMP_ADDRESSV, a);
            set_samp(stage, D3DSAMP_MINFILTER, D3DTEXF_LINEAR);
            set_samp(stage, D3DSAMP_MAGFILTER, D3DTEXF_LINEAR);
            set_samp(stage, D3DSAMP_MIPFILTER, D3DTEXF_NONE);
        }
    } else if (effect_on_) {
        /* the effect's textures (s3..s7), white where unset */
        for (int k = 0; k < 5; ++k) {
            D3D9Texture* t = (D3D9Texture*)effect_.tex[k];
            DWORD stage = (DWORD)(3 + k);
            set_tex(stage, t && t->tex ? t->tex : white_);
            DWORD a = (effect_.repeat >> k) & 1 ? D3DTADDRESS_WRAP : D3DTADDRESS_CLAMP;
            set_samp(stage, D3DSAMP_ADDRESSU, a);
            set_samp(stage, D3DSAMP_ADDRESSV, a);
            set_samp(stage, D3DSAMP_MINFILTER, D3DTEXF_LINEAR);
            set_samp(stage, D3DSAMP_MAGFILTER, D3DTEXF_LINEAR);
            set_samp(stage, D3DSAMP_MIPFILTER, D3DTEXF_NONE);
        }
        set_tex(8, NULL);
    } else {
        for (DWORD stage = 3; stage < 9; ++stage) set_tex(stage, NULL);
    }
    /* The gradient map's lut (s2): 256x1, linear, clamped (the erosion
     * shaders' lut uses it too). */
    D3D9Texture* lut = gm_ && gm_->tex ? gm_ : (vfx_on_ && vfx_.lut ? (D3D9Texture*)vfx_.lut : NULL);
    set_tex(2, lut && lut->tex ? lut->tex : NULL);
    if (lut && lut->tex) {
        set_samp(2, D3DSAMP_ADDRESSU, D3DTADDRESS_CLAMP);
        set_samp(2, D3DSAMP_ADDRESSV, D3DTADDRESS_CLAMP);
        set_samp(2, D3DSAMP_MINFILTER, D3DTEXF_LINEAR);
        set_samp(2, D3DSAMP_MAGFILTER, D3DTEXF_LINEAR);
        set_samp(2, D3DSAMP_MIPFILTER, D3DTEXF_NONE);
    }
    switch (blend_) {
    case BLEND_ADD:
        set_rs(D3DRS_BLENDOP, D3DBLENDOP_ADD);
        set_rs(D3DRS_SRCBLEND, D3DBLEND_ONE);
        set_rs(D3DRS_DESTBLEND, D3DBLEND_ONE);
        break;
    case BLEND_SUB:
        set_rs(D3DRS_BLENDOP, D3DBLENDOP_REVSUBTRACT);
        set_rs(D3DRS_SRCBLEND, D3DBLEND_ONE);
        set_rs(D3DRS_DESTBLEND, D3DBLEND_ONE);
        break;
    case BLEND_MUL:
        set_rs(D3DRS_BLENDOP, D3DBLENDOP_ADD);
        set_rs(D3DRS_SRCBLEND, D3DBLEND_DESTCOLOR);
        set_rs(D3DRS_DESTBLEND, D3DBLEND_ZERO);
        break;
    default:
        set_rs(D3DRS_BLENDOP, D3DBLENDOP_ADD);
        set_rs(D3DRS_SRCBLEND, D3DBLEND_ONE);
        set_rs(D3DRS_DESTBLEND, D3DBLEND_INVSRCALPHA);
        break;
    }
    set_rs(D3DRS_SCISSORTESTENABLE, clip_on_ ? TRUE : FALSE);
    if (clip_on_ && !(c_clip_known_ && memcmp(&c_clip_, &clip_, sizeof(RECT)) == 0)) {
        dev_->SetScissorRect(&clip_);
        c_clip_ = clip_;
        c_clip_known_ = true;
    }
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
        if (screen_.mode == SCREEN_DARK || screen_.mode == SCREEN_GLASS) {
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
            set_tex(stage, st[k]);
            set_samp(stage, D3DSAMP_ADDRESSU, a);
            set_samp(stage, D3DSAMP_ADDRESSV, a);
            set_samp(stage, D3DSAMP_MINFILTER, D3DTEXF_LINEAR);
            set_samp(stage, D3DSAMP_MAGFILTER, D3DTEXF_LINEAR);
            set_samp(stage, D3DSAMP_MIPFILTER, D3DTEXF_NONE);
        }
        c[88] = (float)screen_.mode;
        c[89] = f;
        c[90] = screen_.mix;
        c[91] = screen_.strength;
        c[92] = screen_.noise_offset[0];
        c[93] = screen_.noise_offset[1];
        if (screen_.mode == SCREEN_DOOM) {
            /* c22.yzw: saturation, value, pulse; c23: the gradient colour */
            c[89] = screen_.mix;
            c[90] = screen_.strength;
            c[91] = screen_.power;
            c[92] = screen_.color[0];
            c[93] = screen_.color[1];
            c[94] = screen_.color[2];
            c[95] = screen_.color[3];
        }
        if (screen_.mode == SCREEN_GLASS) {
            /* c22.yzw: lod fraction, corner weight, add; c23: the corner colour */
            c[90] = screen_.mix;
            c[91] = screen_.strength;
            c[92] = screen_.color[0];
            c[93] = screen_.color[1];
            c[94] = screen_.color[2];
            c[95] = screen_.color[3];
        }
        if (screen_.mode == SCREEN_CHROMA) {
            /* c22.w: strength; s11 the distance curve */
            c[89] = c[90] = 0;
            c[91] = screen_.strength;
        }
        if (screen_.mode == SCREEN_DISTORT) {
            /* c22.yw: polar, strength; c23: polar offset, tiling */
            c[89] = screen_.count == 1 ? 1.0f : 0.0f;
            c[90] = 0;
            c[91] = screen_.strength;
            c[92] = screen_.noise_offset[0];
            c[93] = screen_.noise_offset[1];
            c[94] = screen_.center[0];
            c[95] = screen_.center[1];
        }
        if (screen_.mode == SCREEN_RADIAL) {
            /* c22.yzw: count, power; c23.xy: centre */
            c[89] = (float)(screen_.count < 1 ? 1 : (screen_.count > 16 ? 16 : screen_.count));
            c[90] = screen_.power;
            c[91] = 0;
            c[92] = screen_.center[0];
            c[93] = screen_.center[1];
        }
    } else {
        for (DWORD stage = 9; stage < 13; ++stage) set_tex(stage, NULL);
        for (int k = 88; k < 94; ++k) c[k] = 0.0f;
    }
    if (!(screen_on_ && screen_levels_ > 0 &&
          (screen_.mode == SCREEN_GLASS || screen_.mode == SCREEN_DOOM || screen_.mode == SCREEN_DISTORT)))
        c[94] = c[95] = 0.0f;
    set_psc(0, c, 24);
    if (effect_on_) set_psc(24, effect_.p, 4);
    if (effect_on_ && effect_.extra_count > 0) set_psc(29, effect_.extra, (UINT)effect_.extra_count);
    /* c28.x: the texture holds premultiplied colours (a capture) */
    float premul[4] = { tex_ && tex_->premultiplied ? 1.0f : 0.0f, 0, 0, 0 };
    set_psc(28, premul, 1);
    dirty_ = false;
}

void D3D9Renderer::flush() {
    if (batch_count_ == 0 || !dev_) return;
    LARGE_INTEGER t0, t1, t2;
    QueryPerformanceCounter(&t0);
    if (dirty_) apply_state();
    QueryPerformanceCounter(&t1);
    dev_->DrawPrimitiveUP(D3DPT_TRIANGLELIST, (UINT)(batch_count_ / 3), batch_, sizeof(GpuVertex));
    QueryPerformanceCounter(&t2);
    state_ticks_ += t1.QuadPart - t0.QuadPart;
    submit_ticks_ += t2.QuadPart - t1.QuadPart;
    ++draws_;
    batch_count_ = 0;
}

#if defined(_XBOX)
static void put_le(FILE* f, unsigned v, int bytes) {
    for (int i = 0; i < bytes; ++i) fputc((int)((v >> (8 * i)) & 0xFF), f);
}

/* A texel's place in Xenos' tiled layout (32-bit texels; XGAddress2DTiled-
 * Offset): Resolve writes it even into a LIN_ texture. */
static u32 tiled_offset(u32 x, u32 y, u32 width) {
    const u32 log_bpp = 2;
    u32 aligned = (width + 31) & ~31u;
    u32 macro = ((x >> 5) + (y >> 5) * (aligned >> 5)) << (log_bpp + 7);
    u32 micro = ((x & 7) + ((y & 6) << 2)) << log_bpp;
    u32 offset = macro + ((micro & ~15u) << 1) + (micro & 15) + ((y & 8) << (3 + log_bpp)) + ((y & 1) << 4);
    return (((offset & ~511u) << 3) + ((offset & 448) << 2) + (offset & 63) + ((y & 16) << 7) +
            (((((y & 8) >> 2) + (x >> 3)) & 3) << 6)) >> log_bpp;
}

/* tiled 32-bit ARGB words (native order) to a bottom-up 24-bit BMP */
static bool write_bmp(const char* path, const u8* pixels, int w, int h) {
    FILE* f = fopen(path, "wb");
    if (!f) return false;
    unsigned row = (unsigned)w * 3, size = 54 + row * (unsigned)h;
    fputc('B', f);
    fputc('M', f);
    put_le(f, size, 4);
    put_le(f, 0, 4);
    put_le(f, 54, 4);
    put_le(f, 40, 4);
    put_le(f, (unsigned)w, 4);
    put_le(f, (unsigned)h, 4);
    put_le(f, 1, 2);
    put_le(f, 24, 2);
    put_le(f, 0, 4);
    put_le(f, row * (unsigned)h, 4);
    put_le(f, 2835, 4);
    put_le(f, 2835, 4);
    put_le(f, 0, 4);
    put_le(f, 0, 4);
    std::vector<u8> line(row);
    const u32* src = (const u32*)pixels;
    for (int y = h - 1; y >= 0; --y) {
        for (int x = 0; x < w; ++x) {
            u32 v = src[tiled_offset((u32)x, (u32)y, (u32)w)];
            line[x * 3] = (u8)(v & 0xFF);
            line[x * 3 + 1] = (u8)((v >> 8) & 0xFF);
            line[x * 3 + 2] = (u8)((v >> 16) & 0xFF);
        }
        fwrite(&line[0], 1, row, f);
    }
    bool ok = !ferror(f);
    fclose(f);
    return ok;
}
#endif

bool D3D9Renderer::save_screenshot(const char* path) {
#if defined(_XBOX)
    if (!dev_ || width_ <= 0 || height_ <= 0) return false;
    flush();
    /* (a LIN_ format, though Resolve writes the tiled layout: write_bmp
     * untiles; the console's shots of 2026-10-27 show it) */
    IDirect3DTexture9* shot = NULL;
    if (FAILED(dev_->CreateTexture(width_, height_, 1, 0, D3DFMT_LIN_A8R8G8B8, 0, &shot, NULL))) return false;
    bool ok = SUCCEEDED(dev_->Resolve(D3DRESOLVE_RENDERTARGET0, NULL, shot, NULL, 0, 0, NULL, 1.0f, 0, NULL));
    dev_->BlockUntilIdle();
    D3DLOCKED_RECT lr;
    if (ok && SUCCEEDED(shot->LockRect(0, &lr, NULL, D3DLOCK_READONLY))) {
        ok = write_bmp(path, (const u8*)lr.pBits, width_, height_);
        shot->UnlockRect(0);
    } else {
        ok = false;
    }
    shot->Release(); /* the GPU is idle: nothing uses it */
    return ok;
#else
    (void)path;
    return false;
#endif
}

void D3D9Renderer::take_draw_times(double* state_ms, double* submit_ms) {
    LARGE_INTEGER f;
    QueryPerformanceFrequency(&f);
    *state_ms = (double)state_ticks_ * 1000.0 / (double)f.QuadPart;
    *submit_ms = (double)submit_ticks_ * 1000.0 / (double)f.QuadPart;
    state_ticks_ = submit_ticks_ = 0;
}

} /* namespace spire */

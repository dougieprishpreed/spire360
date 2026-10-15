#include "d3d9_renderer.h"

#include "../core/pack.h"

#include <math.h>
#include <string.h>

#if defined(_XBOX)
#include <xgraphics.h>
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
      cm_on_(false), clip_on_(false), dirty_(true), group_rt_(NULL), group_tex_(NULL), main_rt_(NULL),
      group_failed_(false), width_(0), height_(0), draws_(0) {
    msdf_[0] = 16;
    msdf_[1] = 0;
    memset(cm_, 0, sizeof(cm_));
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
    if (!dev_ || d.levels.empty()) return t;
    D3DFORMAT fmt = d3d_format(d.format);
    bool block = d.format != TEX_ARGB8;
#if defined(_XBOX)
    /* One level for now: the packed mip tail needs XGTILE flags per level. */
    UINT levels = 1;
    if (FAILED(dev_->CreateTexture(d.width, d.height, levels, 0, fmt, 0, &t->tex, NULL))) return t;
#else
    UINT levels = (UINT)d.levels.size();
    if (FAILED(dev_->CreateTexture(d.width, d.height, levels, 0, fmt, D3DPOOL_MANAGED, &t->tex, NULL))) return t;
#endif
    for (UINT lv = 0; lv < levels; ++lv) {
        int w = d.width >> lv, h = d.height >> lv;
        if (w < 1) w = 1;
        if (h < 1) h = 1;
        int rows = block ? (h + 3) / 4 : h;
        int pitch = block ? ((w + 3) / 4) * (d.format == TEX_DXT1 ? 8 : 16) : w * 4;
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
        XGTileTextureLevel(w, h, lv, XGGetGpuFormat(fmt), 0, lr.pBits, NULL, &sw[0], pitch, NULL);
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
    RELEASE(d->tex);
    delete d;
}

/* ---- frame ---- */

void D3D9Renderer::begin(int width, int height, const Color& c) {
    if (width != width_ || height != height_) release_group_target();
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
    flush();
    dev_->EndScene();
}

/* ---- canvas groups ---- */

void D3D9Renderer::release_group_target() {
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
    if (main_rt_ || !make_group_target()) return false;
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
    if (group_rect_.right > group_rect_.left && group_rect_.bottom > group_rect_.top) {
        D3DRECT src = { group_rect_.left, group_rect_.top, group_rect_.right, group_rect_.bottom };
        D3DPOINT dst = { group_rect_.left, group_rect_.top };
        dev_->Resolve(D3DRESOLVE_RENDERTARGET0, &src, group_tex_, &dst, 0, 0, NULL, 1.0f, 0, NULL);
    }
#endif
    dev_->SetRenderTarget(0, main_rt_);
    RELEASE(main_rt_);
    dirty_ = true;
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
    IDirect3DTexture9* t = tex_ && tex_->tex ? tex_->tex : white_;
    dev_->SetTexture(0, t);
    DWORD addr = (flags_ & DRAW_REPEAT) ? D3DTADDRESS_WRAP : D3DTADDRESS_CLAMP;
    DWORD filt = (flags_ & DRAW_NEAREST) ? D3DTEXF_POINT : D3DTEXF_LINEAR;
    dev_->SetSamplerState(0, D3DSAMP_ADDRESSU, addr);
    dev_->SetSamplerState(0, D3DSAMP_ADDRESSV, addr);
    dev_->SetSamplerState(0, D3DSAMP_MINFILTER, filt);
    dev_->SetSamplerState(0, D3DSAMP_MAGFILTER, filt);
    dev_->SetSamplerState(0, D3DSAMP_MIPFILTER, D3DTEXF_NONE);
    bool group = (flags_ & DRAW_GROUP) && group_tex_ && !main_rt_;
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
    float c[20] = { cm_on_ ? 1.0f : 0.0f, (flags_ & DRAW_MSDF) ? 1.0f : 0.0f, group ? 1.0f : 0.0f, 0,
                    msdf_[0], msdf_[1], tw, th,
                    cm_[0], cm_[1], cm_[2], cm_[3],
                    cm_[4], cm_[5], cm_[6], cm_[7],
                    cm_[8], cm_[9], cm_[10], cm_[11] };
    dev_->SetPixelShaderConstantF(0, c, 5);
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

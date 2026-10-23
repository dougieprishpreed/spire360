#include "sw_renderer.h"
#include "effects.h"

#include <math.h>
#include <string.h>

namespace spire {

SwRenderer::SwRenderer()
    : target_(&fb_), gx0_(0), gy0_(0), gx1_(0), gy1_(0), clip_on_(false), cx0_(0), cy0_(0), cx1_(0), cy1_(0), tris_(0), msdf_range_(16), msdf_outline_(0), cm_on_(false), ripple_on_(false), gm_(NULL), gm_red_(false), fire_on_(false), vfx_on_(false), screen_on_(false), effect_on_(false),
      screen_levels_(0) {
    for (int i = 0; i < 4; ++i) ripple_[i] = 0;
    cap_w_ = cap_h_ = 0;
}

Texture* SwRenderer::create_texture(const TextureData& d) {
    SwTexture* t = new SwTexture();
    t->width = d.width;
    t->height = d.height;
    t->orig_width = d.orig_width;
    t->orig_height = d.orig_height;
    if (d.levels.empty() || d.levels[0].empty() ||
        !decode_texture_level(d.format, d.width, d.height, &d.levels[0][0], d.levels[0].size(), t->img,
                              (d.flags & TEXF_DATA) == 0)) {
        log_warn("texture decode failed (%dx%d fmt %d)", d.width, d.height, d.format);
        t->img.resize(1, 1);
    }
    t->memory_bytes = (size_t)t->img.w * (size_t)t->img.h * 4;
    return t;
}

void SwRenderer::destroy_texture(Texture* t) { delete t; }

void SwRenderer::begin(int w, int h, const Color& c) {
    fb_.resize(w, h);
    u8 px[4] = { (u8)(c.r * c.a * 255 + 0.5f), (u8)(c.g * c.a * 255 + 0.5f), (u8)(c.b * c.a * 255 + 0.5f),
                 (u8)(c.a * 255 + 0.5f) };
    for (size_t i = 0; i < fb_.px.size(); i += 4) {
        fb_.px[i] = px[0];
        fb_.px[i + 1] = px[1];
        fb_.px[i + 2] = px[2];
        fb_.px[i + 3] = px[3];
    }
    clip_on_ = false;
    tris_ = 0;
    target_ = &fb_;
    screen_levels_ = 0;
}

bool SwRenderer::begin_capture(int w, int h) {
    if (cap_w_ || target_ != &fb_ || w <= 0 || h <= 0 || w > fb_.w || h > fb_.h) return false;
    if (capture_.w != fb_.w || capture_.h != fb_.h) capture_.resize(fb_.w, fb_.h);
    for (int y = 0; y < h; ++y) memset(capture_.at(0, y), 0, (size_t)w * 4);
    cap_w_ = w;
    cap_h_ = h;
    target_ = &capture_;
    return true;
}

Texture* SwRenderer::end_capture(Texture* reuse) {
    if (!cap_w_) return NULL;
    target_ = &fb_;
    SwTexture* t = (SwTexture*)reuse;
    if (t && (t->width != cap_w_ || t->height != cap_h_)) {
        destroy_texture(t);
        t = NULL;
    }
    if (!t) {
        t = new SwTexture();
        t->width = t->orig_width = cap_w_;
        t->height = t->orig_height = cap_h_;
        t->img.resize(cap_w_, cap_h_);
        t->memory_bytes = (size_t)cap_w_ * cap_h_ * 4;
        t->premultiplied = true;
    }
    for (int y = 0; y < cap_h_; ++y) memcpy(t->img.at(0, y), capture_.at(0, y), (size_t)cap_w_ * 4);
    cap_w_ = cap_h_ = 0;
    return t;
}

bool SwRenderer::begin_group(const Rect2& b) {
    if (cap_w_) return false; /* not inside a capture */
    if (layer_.w != fb_.w || layer_.h != fb_.h) {
        layer_.resize(fb_.w, fb_.h);
        for (size_t i = 0; i < layer_.px.size(); ++i) layer_.px[i] = 0;
    } else {
        /* What the previous group could have drawn. */
        for (int y = gy0_; y < gy1_ && gx1_ > gx0_; ++y) memset(layer_.at(gx0_, y), 0, (size_t)(gx1_ - gx0_) * 4);
    }
    /* Pixels whose centres the bounds contain, like the triangle coverage. */
    gx0_ = (int)floorf(b.pos.x + 0.5f);
    gy0_ = (int)floorf(b.pos.y + 0.5f);
    gx1_ = (int)floorf(b.pos.x + b.size.x + 0.5f);
    gy1_ = (int)floorf(b.pos.y + b.size.y + 0.5f);
    if (gx0_ < 0) gx0_ = 0;
    if (gy0_ < 0) gy0_ = 0;
    if (gx1_ > fb_.w) gx1_ = fb_.w;
    if (gy1_ > fb_.h) gy1_ = fb_.h;
    if (gx1_ < gx0_) gx1_ = gx0_;
    if (gy1_ < gy0_) gy1_ = gy0_;
    target_ = &layer_;
    return true;
}

void SwRenderer::end_group() { target_ = &fb_; }

void SwRenderer::set_clip(const Rect2* r) {
    clip_on_ = r != NULL;
    if (r) {
        cx0_ = (int)floorf(r->pos.x + 0.5f);
        cy0_ = (int)floorf(r->pos.y + 0.5f);
        cx1_ = (int)floorf(r->pos.x + r->size.x + 0.5f);
        cy1_ = (int)floorf(r->pos.y + r->size.y + 0.5f);
    }
}

void SwRenderer::draw_triangles(Texture* tex, const Vertex* v, int count, BlendMode blend, int flags) {
    for (int i = 0; i + 2 < count; i += 3) triangle((SwTexture*)tex, v[i], v[i + 1], v[i + 2], blend, flags);
}

static int wrap(int i, int n) {
    i %= n;
    return i < 0 ? i + n : i;
}

/* Bilinear (or nearest), premultiplied, clamp to edge or repeat; returns 0..1 floats. */
static void sample(const Image& img, float u, float v, int flags, float out[4]) {
    float x = u * img.w - 0.5f, y = v * img.h - 0.5f;
    if (flags & DRAW_NEAREST) {
        x = floorf(x + 0.5f);
        y = floorf(y + 0.5f);
    }
    int x0 = (int)floorf(x), y0 = (int)floorf(y);
    float fx = x - x0, fy = y - y0;
    int x1 = x0 + 1, y1 = y0 + 1;
    if (flags & DRAW_REPEAT) {
        x0 = wrap(x0, img.w);
        x1 = wrap(x1, img.w);
        y0 = wrap(y0, img.h);
        y1 = wrap(y1, img.h);
    }
    if (x0 < 0) x0 = 0;
    if (y0 < 0) y0 = 0;
    if (x1 < 0) x1 = 0;
    if (y1 < 0) y1 = 0;
    if (x0 >= img.w) x0 = img.w - 1;
    if (x1 >= img.w) x1 = img.w - 1;
    if (y0 >= img.h) y0 = img.h - 1;
    if (y1 >= img.h) y1 = img.h - 1;
    const u8* a = img.at(x0, y0);
    const u8* b = img.at(x1, y0);
    const u8* c = img.at(x0, y1);
    const u8* d = img.at(x1, y1);
    for (int k = 0; k < 4; ++k) {
        float top = a[k] + (b[k] - a[k]) * fx;
        float bot = c[k] + (d[k] - c[k]) * fx;
        out[k] = (top + (bot - top) * fy) * (1.0f / 255.0f);
    }
}

/* A fire texture's texel (NULL: white), repeating or clamped. */
static void fire_texel(const Texture* t, float u, float v, bool repeat, float out[4]) {
    if (!t) {
        out[0] = out[1] = out[2] = out[3] = 1.0f;
        return;
    }
    sample(((const SwTexture*)t)->img, u, v, repeat ? DRAW_REPEAT : 0, out);
}

void SwRenderer::screen_level(int level) {
    if ((int)screen_mips_.size() <= level) screen_mips_.resize((size_t)level + 1);
    blur_mip_level(screen_mips_[(size_t)level - 1], screen_mips_[(size_t)level]);
    screen_levels_ = level + 1;
}

bool SwRenderer::set_screen(const ScreenParams* p) {
    screen_on_ = p != NULL;
    if (!p) return true;
    screen_ = *p;
    if (screen_levels_ == 0) {
        if (screen_mips_.empty()) screen_mips_.resize(1);
        screen_mips_[0] = fb_;
        screen_levels_ = 1;
    }
    int need = p->mode == SCREEN_DARK || p->mode == SCREEN_GLASS ? (int)ceilf(p->lod) : 0;
    while (screen_levels_ <= need) {
        const Image& top = screen_mips_[(size_t)screen_levels_ - 1];
        if (top.w <= 1 && top.h <= 1) break;
        screen_level(screen_levels_);
    }
    return true;
}

void SwRenderer::sample_screen(float u, float v, float lod, float out[4]) const {
    image_lod(&screen_mips_[0], screen_levels_, u, v, lod, out);
}

static float smoothstepf(float e0, float e1, float x) {
    float t = e1 != e0 ? (x - e0) / (e1 - e0) : (x >= e1 ? 1.0f : 0.0f);
    t = t < 0 ? 0 : (t > 1 ? 1 : t);
    return t * t * (3.0f - 2.0f * t);
}

/* shift_hue (shaders/vfx/_util/hue_shift.gdshaderinc): rgb rotated about
 * the grey axis by shift turns. */
static void shift_hue(float rgb[3], float shift) {
    if (shift == 0) return;
    const float g = 0.57735f;
    float d = (rgb[0] + rgb[1] + rgb[2]) * g;
    float p[3] = { g * d, g * d, g * d };
    float uu[3] = { rgb[0] - p[0], rgb[1] - p[1], rgb[2] - p[2] };
    float vv[3] = { g * uu[2] - g * uu[1], g * uu[0] - g * uu[2], g * uu[1] - g * uu[0] };
    float c = cosf(shift * 6.2831853f), s = sinf(shift * 6.2831853f);
    for (int k = 0; k < 3; ++k) rgb[k] = uu[k] * c + vv[k] * s + p[k];
}

/* A premultiplied texel as straight colour. */
static void unpremul(float t[4]) {
    float ia = t[3] > 0 ? 1.0f / t[3] : 0.0f;
    t[0] *= ia;
    t[1] *= ia;
    t[2] *= ia;
}

/* polar_coordinates (shaders/vfx/_util): fract(distance from the middle *
 * 2 * zoom, angle / 2 pi). */
static void polar(float u, float v, float zoom, float* pu, float* pv) {
    float dx = u - 0.5f, dy = v - 0.5f;
    float r = sqrtf(dx * dx + dy * dy) * 2.0f * zoom, a = atan2f(dy, dx) / 6.2832f;
    *pu = r - floorf(r);
    *pv = a - floorf(a);
}

/* An effect's texture i at uv (NULL: white). */
static void effect_texel(const EffectParams& e, int i, float u, float v, float out[4]) {
    fire_texel(e.tex[i], u, v, (e.repeat >> i) & 1, out);
}

/* The effect shaders (effects.h), in the software renderer: s is the
 * premultiplied result. */
void SwRenderer::effect_pixel(const SwTexture* tex, float u, float v, int flags, float su, float sv,
                              const float col[4], float out[4]) const {
    (void)su;
    (void)sv;
    (void)col;
    const EffectParams& e = effect_;
    switch (e.id) {
    case EFFECT_WATER_REFLECTION: {
        float n[4], m[4];
        effect_texel(e, 0, u + e.p[0], v + e.p[0], n);
        effect_texel(e, 1, u, v, m);
        if (m[3] <= 0.1f || !tex) {
            if (tex) sample(tex->img, u, v, flags, out);
            else out[0] = out[1] = out[2] = out[3] = 1;
        } else {
            float d = -e.p[1] * 0.5f + n[0] * e.p[1] * 0.5f;
            sample(tex->img, u + d, v + d, flags, out);
        }
        return;
    }
    case EFFECT_SCROLLING_SMOKE: {
        float sm[4], g[4];
        effect_texel(e, 0, u, v + e.p[0], sm);
        effect_texel(e, 1, u, v, g);
        float ia = sm[3] > 0 ? 1.0f / sm[3] : 0.0f;
        float a = smoothstepf(e.p[1], e.p[2], sm[3] * (g[3] > 0 ? g[0] / g[3] : 0.0f)) * e.p[3];
        a = clampf(a, 0, 1);
        for (int k = 0; k < 3; ++k) out[k] = e.p[4 + k] * sm[k] * ia * a;
        out[3] = a;
        return;
    }
    case EFFECT_LUT_COLOR:
    case EFFECT_RAY:
    case EFFECT_LUT_ALPHA: {
        float t[4] = { 1, 1, 1, 1 }, l[4];
        if (tex) sample(tex->img, u, v, flags, t);
        float ta = t[3], tr = ta > 0 ? t[0] / ta : 0.0f;
        effect_texel(e, 0, tr, 0.5f, l);
        float la = l[3], ia = la > 0 ? 1.0f / la : 0.0f;
        float a;
        if (e.id == EFFECT_LUT_COLOR) a = col[3] * (ta + (tr - ta) * e.p[0]);
        else if (e.id == EFFECT_RAY) {
            float va[4];
            effect_texel(e, 1, v, u, va);
            a = col[3] * ta * (va[3] > 0 ? va[0] / va[3] : 0.0f);
        } else a = la * ta * col[3];
        a = clampf(a, 0, 1);
        for (int k = 0; k < 3; ++k) out[k] = clampf(l[k] * ia * col[k], 0, 1) * a;
        out[3] = a;
        return;
    }
    case EFFECT_SIMPLE_NOISE: {
        float t[4] = { 1, 1, 1, 1 }, n1[4], n2[4];
        if (tex) sample(tex->img, u, v, flags, t);
        effect_texel(e, 0, (u + 0.275f) * e.p[2] - 0.275f + e.p[4], v * e.p[3] + e.p[5], n1);
        effect_texel(e, 1, (u + 0.275f) * e.p[6] - 0.275f + e.p[8], v * e.p[7] + e.p[9], n2);
        float ia = t[3] > 0 ? 1.0f / t[3] : 0.0f, tr = t[0] * ia;
        float a = clampf(smoothstepf(e.p[0], e.p[1], col[3] * t[3] * tr * n1[0] * n2[0]), 0, 1);
        for (int k = 0; k < 3; ++k) out[k] = clampf(col[k] * t[k] * ia, 0, 1) * a;
        out[3] = a;
        return;
    }
    case EFFECT_DECIMILLIPEDE_SUCK: {
        float p1[4], p2[4], g[4], sh[4];
        effect_texel(e, 0, u * 3 + e.p[0], v * 3 + e.p[1], p1);
        effect_texel(e, 0, u * 2 + e.p[2], v * 2 + e.p[3], p2);
        float pr = p1[0] * p2[0], pg = p1[1] * p2[1];
        effect_texel(e, 1, u + 0.2f + pr, v - 0.05f + pg, g);
        effect_texel(e, 2, u + pr * 0.03f, v, sh);
        float a = smoothstepf(0.4f, 0.6f, g[0]) * smoothstepf(0.4f, 0.7f, sh[0]) * e.p[4];
        a = clampf(a, 0, 1);
        out[0] = 0.0153f * a;
        out[1] = 0.090072f * a;
        out[2] = 0.17f * a;
        out[3] = a;
        return;
    }
    case EFFECT_MOLTEN_FIST: {
        float t[4] = { 1, 1, 1, 1 }, n1[4], n2[4], m[4], fa[4];
        if (tex) sample(tex->img, u, v, flags, t);
        effect_texel(e, 0, u * e.p[4] + e.p[6], v * e.p[5] + e.p[7], n1);
        effect_texel(e, 1, u * e.p[8] + e.p[10], v * e.p[9] + e.p[11], n2);
        effect_texel(e, 2, u * e.p[12] + e.p[14], (v - 0.04f) * e.p[13] + 0.04f + e.p[15], m);
        effect_texel(e, 3, u, v, fa);
        float mb = m[2];
        if (e.p[3] < 0) mb = 1 - mb;
        float s2 = e.extra[0] != 0 ? e.p[2] * t[3] * col[3] : e.p[2];
        float x = (s2 * n2[2] * fabsf(e.p[3]) * n1[2] + mb) * fa[3];
        float a = clampf(smoothstepf(e.p[0], e.p[1], x), 0, 1);
        float ia = t[3] > 0 ? 1.0f / t[3] : 0.0f;
        for (int k = 0; k < 3; ++k) out[k] = clampf(col[k] * t[k] * ia, 0, 1) * a;
        out[3] = a;
        return;
    }
    case EFFECT_SLASH_FLAT: {
        float sh[4], n1[4], n2[4];
        effect_texel(e, 0, u, v, sh);
        effect_texel(e, 1, (u + 0.275f) * e.p[5] - 0.275f + e.p[7], v * e.p[6] + e.p[8], n1);
        effect_texel(e, 2, (u + 0.275f) * e.p[9] - 0.275f + e.p[15], v * e.p[10] + e.p[11], n2);
        float x = sh[2] * e.p[2] * n1[0] * n2[0] * e.p[3];
        float a = clampf(smoothstepf(e.p[0], e.p[1], x) * e.p[4], 0, 1);
        for (int k = 0; k < 3; ++k) out[k] = e.p[12 + k] * a;
        out[3] = a;
        return;
    }
    case EFFECT_EXHAUST_DISSOLVE:
    case EFFECT_EXHAUST_QUICK: {
        float t[4] = { 1, 1, 1, 1 }, l[4];
        if (tex) sample(tex->img, u, v, flags, t);
        bool quick = e.id == EFFECT_EXHAUST_QUICK;
        float r, erosion, bright;
        if (quick) {
            r = t[3] > 0 ? t[0] / t[3] : 0.0f;
            erosion = smoothstepf(e.p[0], e.p[0] + e.p[1], r);
        } else {
            float er[4];
            effect_texel(e, 0, u + e.p[1], v, er);
            r = er[3] > 0 ? er[0] / er[3] : 0.0f;
            erosion = r >= e.p[0] ? 1.0f : 0.0f;
        }
        bright = smoothstepf(e.p[0], e.p[0] + e.p[2], r);
        effect_texel(e, quick ? 0 : 1, bright, 0.5f, l);
        float il = l[3] > 0 ? 1.0f / l[3] : 0.0f;
        float bc[3] = { l[0] * il, l[1] * il, l[2] * il };
        shift_hue(bc, e.p[3]);
        float a, rgb[3];
        if (quick) {
            a = t[3] * col[3] * erosion;
            for (int k = 0; k < 3; ++k) rgb[k] = bc[k];
        } else {
            /* the viewport's texel as stored (premultiplied), as the game */
            a = t[3] * col[3] * erosion;
            for (int k = 0; k < 3; ++k) rgb[k] = t[k] + (bc[k] - t[k]) * (1 - bright);
        }
        a = clampf(a, 0, 1);
        for (int k = 0; k < 3; ++k) out[k] = clampf(rgb[k] * col[k], 0, 1) * a;
        out[3] = a;
        return;
    }
    case EFFECT_TINT_GRAY: {
        float t[4] = { 1, 1, 1, 1 };
        if (tex) sample(tex->img, u, v, flags, t);
        float ia = t[3] > 0 ? 1.0f / t[3] : 0.0f;
        float g = (t[0] * 0.299f + t[1] * 0.587f + t[2] * 0.114f) * ia;
        float a = clampf(t[3] * 0.75f, 0, 1);
        for (int k = 0; k < 3; ++k) out[k] = clampf(e.p[k] * (0.7f + 0.3f * g), 0, 1) * a;
        out[3] = a;
        return;
    }
    case EFFECT_TAB_APPEAR: {
        float p = e.p[0];
        float s1 = smoothstepf(p, p + 0.2f, u), s2 = smoothstepf(p + 0.2f, p + 0.25f, u);
        float vv = v + ((v - 0.5f) * 0.75f + 0.5f - v) * s1;
        float t[4] = { 1, 1, 1, 1 };
        if (tex) sample(tex->img, u, vv, flags, t);
        float ia = t[3] > 0 ? 1.0f / t[3] : 0.0f;
        static const float add[3] = { 1.0f, 0.75f, 0.9f };
        float c[4];
        for (int k = 0; k < 3; ++k) {
            float tk = t[k] * ia;
            c[k] = tk + (tk + add[k] - tk) * s1;
            c[k] = c[k] + (add[k] - c[k]) * s2;
        }
        c[3] = t[3] + (1 - t[3]) * s1;
        c[3] = c[3] * (1 - s2);
        float a = clampf(c[3] * t[3], 0, 1);
        for (int k = 0; k < 3; ++k) out[k] = clampf(c[k], 0, 1) * a;
        out[3] = a;
        return;
    }
    case EFFECT_LOW_HP_BORDER: {
        float dx = u - 0.5f, dy = v - 0.5f;
        float pr = sqrtf(dx * dx + dy * dy) * 2.0f, pa = atan2f(dy, dx) / 6.2832f;
        pr -= floorf(pr);
        pa -= floorf(pa);
        float n[4];
        effect_texel(e, 0, pr * e.p[4] + e.p[6], pa * e.p[5] + e.p[7], n);
        float x = fabsf(u - 0.5f) * 2, y = fabsf(v - 0.5f) * 2;
        float q = 1 - (1 - sqrtf(x * x + y * y) / e.p[3]) / (1 - e.p[2]);
        float fa = q * e.p[0] + n[0] * 0.25f;
        fa = smoothstepf(e.p[8], e.p[9], fa) * e.p[1];
        float a = clampf(fa * col[3], 0, 1);
        for (int k = 0; k < 3; ++k) out[k] = clampf(e.p[10 + k] * col[k], 0, 1) * a;
        out[3] = a;
        return;
    }
    case EFFECT_SLEEPING_Z: {
        float d[4], er[4], t[4] = { 1, 1, 1, 1 };
        effect_texel(e, 0, u + e.p[3], v + e.p[4], d);
        float db = d[3] > 0 ? d[2] / d[3] : 0.0f;
        float off = (db - 0.5f) * 2.0f * e.p[2];
        if (tex) sample(tex->img, u + off, v + off, flags, t);
        effect_texel(e, 0, u + e.p[5], v + e.p[5], er);
        float eg = er[3] > 0 ? er[1] / er[3] : 0.0f;
        float erosion = smoothstepf(e.p[0], e.p[0] + e.p[1], eg);
        float ia = t[3] > 0 ? 1.0f / t[3] : 0.0f, r = t[0] * ia;
        float c[4];
        for (int k = 0; k < 3; ++k) c[k] = e.p[6 + k] + (col[k] - e.p[6 + k]) * r;
        c[3] = e.p[9] + (col[3] - e.p[9]) * r;
        float a = clampf(c[3] * erosion * t[3], 0, 1);
        for (int k = 0; k < 3; ++k) out[k] = clampf(c[k], 0, 1) * a;
        out[3] = a;
        return;
    }
    case EFFECT_LUT_EROSION:
    case EFFECT_POTION_SPLASH: {
        float t[4] = { 1, 1, 1, 1 }, l[4];
        float erosion_in;
        if (e.id == EFFECT_POTION_SPLASH) {
            float d[4] = { 1, 1, 1, 1 };
            if (tex) sample(tex->img, u + e.p[2], v + e.p[3], flags, d);
            float db = d[3] > 0 ? d[2] / d[3] : 0.0f;
            if (tex) sample(tex->img, u + db * e.p[4], v + db * e.p[4], flags, t);
            erosion_in = t[3];
        } else {
            if (tex) sample(tex->img, u, v, flags, t);
            erosion_in = t[3] > 0 ? t[0] / t[3] : 0.0f;
        }
        float r = t[3] > 0 ? t[0] / t[3] : 0.0f;
        effect_texel(e, 0, r, 0.5f, l);
        float il = l[3] > 0 ? 1.0f / l[3] : 0.0f;
        float a = clampf(col[3] * smoothstepf(e.p[0], e.p[0] + e.p[1], erosion_in), 0, 1);
        for (int k = 0; k < 3; ++k) out[k] = clampf(l[k] * il * col[k], 0, 1) * a;
        out[3] = a;
        return;
    }
    case EFFECT_GOOPY_BITS: {
        float d[4] = { 0, 0, 0, 0 }, t[4] = { 0, 0, 0, 0 }, l[4];
        if (tex) sample(tex->img, u + e.p[2], v + e.p[3], flags, d);
        float off = ((d[3] > 0 ? d[2] / d[3] : 0.0f) - 0.5f) * 2.0f * e.p[4];
        float f = floorf(e.p[5]);
        float fu = (u + fmodf(f, e.p[6])) / e.p[6], fv = (v + floorf(f / e.p[6])) / e.p[7];
        if (tex) sample(tex->img, fu + off, fv + off, flags, t);
        float ia = t[3] > 0 ? 1.0f / t[3] : 0.0f, r = t[0] * ia;
        effect_texel(e, 0, r, r, l);
        float il = l[3] > 0 ? 1.0f / l[3] : 0.0f;
        float a = clampf(smoothstepf(e.p[0], e.p[0] + e.p[1], t[1] * ia) * col[3], 0, 1);
        for (int k = 0; k < 3; ++k) out[k] = clampf(l[k] * il * col[k], 0, 1) * a;
        out[3] = a;
        return;
    }
    case EFFECT_BOUND_MAIN: {
        float t[4] = { 1, 1, 1, 1 }, b[4] = { 0, 0, 0, 0 };
        if (tex) {
            sample(tex->img, u, v, flags, t);
            sample(tex->img, u * e.p[0] + e.p[2], v * e.p[1] + e.p[3], flags, b);
        }
        float bg = b[3] > 0 ? b[1] / b[3] : 0.0f, k = 1 + (e.p[4] - 1) * bg;
        float r = t[3] > 0 ? t[0] / t[3] : 0.0f;
        float a = clampf(t[3] * col[3], 0, 1);
        for (int q = 0; q < 3; ++q) out[q] = clampf(r * col[q] * k, 0, 1) * a;
        out[3] = a;
        return;
    }
    case EFFECT_POLAR_BORDER: {
        float dx = u - 0.5f, dy = v - 0.5f;
        float pr = sqrtf(dx * dx + dy * dy) * 2.0f, pa = atan2f(dy, dx) / 6.2831853f;
        pr -= floorf(pr);
        pa -= floorf(pa);
        float t[4] = { 1, 1, 1, 1 }, m[4];
        if (tex) sample(tex->img, pr * e.p[0] + e.p[2], pa * e.p[1] + e.p[3], flags, t);
        effect_texel(e, 0, u, v, m);
        float ia = t[3] > 0 ? 1.0f / t[3] : 0.0f;
        float a = clampf(smoothstepf(e.p[4], e.p[4] + e.p[5], col[3] * m[3]), 0, 1);
        for (int k = 0; k < 3; ++k) out[k] = clampf(t[k] * ia * col[k], 0, 1) * a;
        out[3] = a;
        return;
    }
    case EFFECT_SQUIGGLY_RING: {
        float dx = u - 0.5f, dy = v - 0.5f;
        float pr = sqrtf(dx * dx + dy * dy) * 2.0f, pa = atan2f(dy, dx) / 6.2831853f;
        pr -= floorf(pr);
        pa -= floorf(pa);
        float dm[4] = { 0, 0, 0, 0 }, t[4] = { 0, 0, 0, 0 }, ring[4] = { 0, 0, 0, 0 };
        if (tex) sample(tex->img, pr * e.p[2] + e.p[4] + e.p[6], pa * e.p[3] + e.p[5] + e.p[6], flags, dm);
        float id = dm[3] > 0 ? 1.0f / dm[3] : 0.0f;
        float len = sqrtf(dx * dx + dy * dy);
        float k = len > 1e-6f ? dm[1] * id * dm[2] * id * e.p[7] / len : 0.0f;
        float qx = u - dx * k - 0.5f, qy = v - dy * k - 0.5f;
        float qr = sqrtf(qx * qx + qy * qy) * 2.0f, qa = atan2f(qy, qx) / 6.2831853f;
        qr -= floorf(qr);
        qa -= floorf(qa);
        if (tex) {
            sample(tex->img, u, v, flags, t);
            sample(tex->img, qr * e.p[8] + e.p[10] + e.p[11], qa * e.p[9], flags, ring);
        }
        float rr = ring[3] > 0 ? ring[0] / ring[3] : 0.0f;
        float a = clampf(col[3] * t[3] * smoothstepf(e.p[0], e.p[0] + e.p[1], rr), 0, 1);
        for (int q = 0; q < 3; ++q) out[q] = clampf(col[q], 0, 1) * a;
        out[3] = a;
        return;
    }
    case EFFECT_FIRE_SHAPE: {
        float n1[4], n2[4], g[4], mn[4], sh[4];
        effect_texel(e, 0, u * e.p[2] + e.p[0], v * e.p[2] + e.p[1], n1);
        effect_texel(e, 1, u * e.p[5] + e.p[3], v * e.p[5] + e.p[4], n2);
        float w = (e.p[6] + n1[0]) * (n2[0] + e.p[7]);
        effect_texel(e, 2, u - 1.065f + w, v - 0.24f + w, g);
        effect_texel(e, 3, u + e.p[8], v + e.p[9], mn);
        float im = mn[3] > 0 ? e.p[10] / mn[3] : 0.0f;
        effect_texel(e, 4, u + mn[0] * im, v + mn[1] * im, sh);
        /* straight colours (the images are premultiplied) */
        float gr = g[3] > 0 ? g[0] / g[3] : 0.0f, sr = sh[3] > 0 ? sh[0] / sh[3] : 0.0f;
        float a = clampf(smoothstepf(e.p[11], e.p[12], gr * sr), 0, 1);
        for (int k = 0; k < 3; ++k) out[k] = clampf(e.p[13 + k], 0, 1) * a;
        out[3] = a;
        return;
    }
    case EFFECT_SIMPLE_DISTORTION: {
        float n[4], t[4] = { 1, 1, 1, 1 };
        effect_texel(e, 0, u + e.p[0], v + e.p[1], n);
        unpremul(n);
        if (tex) sample(tex->img, u + e.p[2] * n[0], v + e.p[3] * n[0], flags, t);
        unpremul(t);
        float a = clampf(t[3], 0, 1);
        for (int k = 0; k < 3; ++k) out[k] = clampf(e.p[4 + k] * t[k], 0, 1) * a;
        out[3] = a;
        return;
    }
    case EFFECT_ENTANGLED: {
        float d[4] = { 0, 0, 0, 0 }, mt[4] = { 0, 0, 0, 0 }, t[4] = { 0, 0, 0, 0 }, l[4];
        if (tex) {
            sample(tex->img, u * e.p[0] + e.p[2], v * e.p[1] + e.p[3], flags, d);
            sample(tex->img, u, v, flags, mt);
            unpremul(d);
            unpremul(mt);
            sample(tex->img, u + d[2] * e.p[4] * mt[1], v, flags, t);
            unpremul(t);
        }
        effect_texel(e, 0, t[0], t[0], l);
        unpremul(l);
        float a = clampf(col[3] * t[3], 0, 1);
        for (int k = 0; k < 3; ++k) out[k] = clampf(col[k] * l[k], 0, 1) * a;
        out[3] = a;
        return;
    }
    case EFFECT_SPHERE: {
        float n[4], g[4], sm[4], tb[4], t[4] = { 0, 0, 0, 0 };
        effect_texel(e, 0, u * e.p[2] + e.p[0], v * e.p[3] + e.p[1], n);
        effect_texel(e, 1, u, v, g);
        unpremul(n);
        unpremul(g);
        if (tex) sample(tex->img, u + e.p[5] + n[0] * e.p[4], v + e.p[6] - 0.075f * g[1] + n[1] * e.p[4], flags, t);
        unpremul(t);
        effect_texel(e, 2, u, v, sm);
        effect_texel(e, 3, u + e.p[14], v + e.p[15], tb);
        unpremul(sm);
        unpremul(tb);
        float a = clampf(t[3] * smoothstepf(e.p[10], e.p[11], sm[0]) * smoothstepf(e.p[12], e.p[13], tb[0]), 0, 1);
        for (int k = 0; k < 3; ++k) out[k] = clampf(e.p[7 + k] + t[k], 0, 1) * a;
        out[3] = a;
        return;
    }
    case EFFECT_SCREAM_RING: {
        float pu, pv, ring[4] = { 0, 0, 0, 0 }, t[4] = { 0, 0, 0, 0 };
        polar(u, v, 1, &pu, &pv);
        if (tex) {
            sample(tex->img, pu * e.p[0] + e.p[2] + e.p[3], pv * e.p[1], flags, ring);
            sample(tex->img, u, v, flags, t);
        }
        unpremul(ring);
        float a = clampf(col[3] * ring[0] * t[3], 0, 1);
        for (int k = 0; k < 3; ++k) out[k] = clampf(col[k], 0, 1) * a;
        out[3] = a;
        return;
    }
    case EFFECT_HEXED: {
        float pu, pv, nt[4] = { 0, 0, 0, 0 }, t[4] = { 1, 1, 1, 1 };
        polar(u, v, 1, &pu, &pv);
        if (tex) {
            sample(tex->img, pu * e.p[0] + e.p[2], pv * e.p[1] + e.p[3], flags, nt);
            sample(tex->img, u, v, flags, t);
        }
        unpremul(nt);
        unpremul(t);
        float n = smoothstepf(e.p[4], e.p[4] + e.p[5], nt[2]);
        float a = clampf(t[3] * col[3], 0, 1);
        for (int k = 0; k < 3; ++k) {
            float inner = e.p[6 + k] + (e.p[9 + k] - e.p[6 + k]) * n;
            float c = e.p[12 + k] + (inner - e.p[12 + k]) * t[0];
            out[k] = clampf(c * col[k], 0, 1) * a;
        }
        out[3] = a;
        return;
    }
    case EFFECT_RINGING: {
        float f = e.p[0], pu, pv, tr[4], rb[4], t[4] = { 0, 0, 0, 0 }, mp[4] = { 0, 0, 0, 0 };
        polar(u, v, 0.8f, &pu, &pv);
        effect_texel(e, 1, f, f, tr);
        effect_texel(e, 0, f, f, rb);
        unpremul(tr);
        unpremul(rb);
        if (tex) {
            sample(tex->img, u, v, flags, t);
            sample(tex->img, pu * 0.5f + tr[0], pv * 0.5f, flags, mp);
        }
        unpremul(t);
        float a = maxf(maxf(t[0] * rb[0], t[1] * rb[1]), maxf(t[2] * rb[2], mp[3] * tr[1]));
        a = clampf(a, 0, 1);
        for (int k = 0; k < 3; ++k) out[k] = clampf(col[k], 0, 1) * a;
        out[3] = a;
        return;
    }
    case EFFECT_LUT_FLIPBOOK: {
        float total = e.p[1] * e.p[2], f = floorf(e.p[0]);
        f -= total * floorf(f / total);
        float fu = u / e.p[1] + fmodf(f, e.p[1]) / e.p[1], fv = v / e.p[2] + floorf((f + 0.5f) / e.p[1]) / e.p[2];
        float t[4] = { 0, 0, 0, 0 }, l[4];
        if (tex) sample(tex->img, fu, fv, flags, t);
        unpremul(t);
        effect_texel(e, 0, t[0], t[0], l);
        unpremul(l);
        float a = clampf(t[3] * col[3], 0, 1);
        for (int k = 0; k < 3; ++k) out[k] = clampf(l[k] * col[k], 0, 1) * a;
        out[3] = a;
        return;
    }
    case EFFECT_NOISY_DISSOLVE: {
        float n[4], t[4] = { 1, 1, 1, 1 };
        effect_texel(e, 0, u + e.p[2], v + e.p[3], n);
        unpremul(n);
        if (tex) sample(tex->img, u, v, flags, t);
        unpremul(t);
        float a = clampf(smoothstepf(e.p[0], e.p[0] + e.p[1], n[0]) * t[3], 0, 1);
        for (int k = 0; k < 3; ++k) out[k] = clampf(col[k] * t[k], 0, 1) * a;
        out[3] = a;
        return;
    }
    case EFFECT_SMOG: {
        float ru = v, rv = 1 - u, m1[4] = { 0, 0, 0, 0 }, m2[4] = { 0, 0, 0, 0 }, mk[4], l[4];
        if (tex) {
            sample(tex->img, ru * e.p[0] + e.p[2], rv * e.p[1] + e.p[3], flags, m1);
            sample(tex->img, ru * e.p[4] + e.p[6], rv * e.p[5] + e.p[7], flags, m2);
        }
        unpremul(m1);
        unpremul(m2);
        effect_texel(e, 1, u, v, mk);
        unpremul(mk);
        float s = (m1[0] + (m2[0] - m1[0]) * e.p[8]) * mk[0];
        float a = smoothstepf(e.p[9], e.p[9] + e.p[10], s);
        effect_texel(e, 0, a, a, l);
        unpremul(l);
        float alpha = clampf(a * col[3] + mk[1], 0, 1);
        for (int k = 0; k < 3; ++k) out[k] = clampf(col[k] * l[k], 0, 1) * alpha;
        out[3] = alpha;
        return;
    }
    case EFFECT_LEAF: {
        float rt[4] = { 0, 0, 0, 0 }, t[4] = { 0, 0, 0, 0 }, l[4];
        if (tex) sample(tex->img, u * e.p[0] + e.p[2], v * e.p[1] + e.p[2] + e.p[3], flags, rt);
        unpremul(rt);
        float ang = 1.5707963f + e.p[4] * (rt[2] - 0.5f) * 2.0f * 1.5707963f;
        float s = sinf(ang), c = cosf(ang), dx = u - e.p[5], dy = v - e.p[6];
        if (tex) sample(tex->img, dx * s - dy * c + e.p[5], dx * c + dy * s + e.p[6], flags, t);
        unpremul(t);
        effect_texel(e, 0, t[0], t[0], l);
        unpremul(l);
        float a = clampf(col[3] * t[3], 0, 1);
        for (int k = 0; k < 3; ++k) out[k] = clampf(col[k] * l[k], 0, 1) * a;
        out[3] = a;
        return;
    }
    case EFFECT_OIL: {
        float sx = (u - 0.5f) * 2, sy = (v - 0.5f) * 2, d = sqrtf(sx * sx + sy * sy);
        float ang = atan2f(sx, sy) / 6.2831853f;
        float tb = e.p[2], tb5 = 5 * tb - floorf(5 * tb), n[4];
        float a1 = ang - tb, a2 = ang + tb5;
        effect_texel(e, 0, tb, a1 - floorf(a1), n);
        unpremul(n);
        float nv1 = (n[0] - 0.5f) / 10.0f;
        effect_texel(e, 0, a2 - floorf(a2), tb, n);
        unpremul(n);
        float nv = nv1 + (n[0] - 0.5f) / 12.0f;
        float bw = e.p[8], R = e.p[0] + nv;
        float mm = 0;
        if (bw > 0) {
            float L = (d - R) / bw;
            mm = ((R + bw >= d ? 1.0f : 0.0f) - (R >= d ? 1.0f : 0.0f)) * L * L;
        }
        float gm = (R + bw >= d ? 1.0f : 0.0f) - (0 >= d ? 1.0f : 0.0f);
        float gu = u + e.p[3], gv = v + e.p[3], gn[4];
        effect_texel(e, 1, gu - floorf(gu), gv - floorf(gv), gn);
        unpremul(gn);
        float ga = gn[0] * 3.0f, vy = sy + e.p[4];
        float ry = -sinf(ga) * sx + cosf(ga) * vy;
        float md = e.p[5] != 0 ? ry - e.p[5] * floorf(ry / e.p[5]) : 0.0f;
        gm *= md < e.p[6] ? 1.0f : 0.0f;
        float mr = e.p[1], x = d - nv1 * d * 2.0f;
        float mana = (smoothstepf(-0.1f, -0.05f, x) - smoothstepf(mr + 0.05f, mr + 0.1f, x)) * (d < R + bw / 1.5f ? 1.0f : 0.0f);
        float q0 = e.p[14], q1 = e.p[15];
        bool on = q1 >= 65536.0f;
        if (on) q1 -= 65536.0f;
        float mc[4] = { floorf(q0 / 256.0f) / 255.0f, (q0 - 256.0f * floorf(q0 / 256.0f)) / 255.0f,
                        floorf(q1 / 256.0f) / 255.0f, (q1 - 256.0f * floorf(q1 / 256.0f)) / 255.0f };
        float c[4] = { 0, 0, 0, 0 };
        if (mm > 0) {
            for (int k = 0; k < 4; ++k) c[k] = e.p[10 + k];
            c[3] *= mm;
        }
        if (mana > 0 && on) {
            float g = mr != 0 ? d / mr * e.p[9] : 0.0f;
            for (int k = 0; k < 3; ++k) c[k] = mc[k] + mc[k] * g;
            c[3] = mana * mc[3];
        }
        if (gm > 0) {
            float k2 = maxf(mana, mm);
            for (int k = 0; k < 4; ++k) c[k] = c[k] * k2 + (e.p[10 + k] - c[k] * k2) * e.p[7];
        }
        float a = clampf(c[3], 0, 1);
        for (int k = 0; k < 3; ++k) out[k] = clampf(c[k], 0, 1) * a;
        out[3] = a;
        return;
    }
    case EFFECT_GALVANIZED: {
        float bt[4] = { 0, 0, 0, 0 }, dt[4] = { 0, 0, 0, 0 }, t[4] = { 0, 0, 0, 0 }, mt[4] = { 0, 0, 0, 0 }, l[4];
        float pu, pv;
        polar(u, v, 1, &pu, &pv);
        if (tex) {
            sample(tex->img, u * e.p[0] + e.p[1], 0, flags, bt);
            sample(tex->img, pu * e.p[2] + e.p[4], pv * e.p[3] + e.p[5], flags, dt);
        }
        unpremul(bt);
        unpremul(dt);
        float blink = smoothstepf(e.p[8], e.p[8] + e.p[9], bt[2]);
        float o = (dt[2] - 0.5f) * 2.0f * e.p[6], k = dt[2] * -e.p[7];
        if (tex) {
            sample(tex->img, u + (u + o - 0.5f) * k, v + (v + o - 0.5f) * k, flags, t);
            sample(tex->img, u, v, flags, mt);
        }
        unpremul(t);
        effect_texel(e, 0, blink, 0, l);
        unpremul(l);
        float mask = smoothstepf(e.p[10], e.p[10] + e.p[11], mt[3]);
        float a = clampf(col[3] * maxf(t[0], t[1] * l[3] * blink) * mask, 0, 1);
        for (int q = 0; q < 3; ++q) out[q] = clampf(l[q] * col[q], 0, 1) * a;
        out[3] = a;
        return;
    }
    case EFFECT_SWEEP_FLIPBOOK: {
        float x = u - 0.5f, y = v - 0.5f;
        float wz = e.p[2] * x + e.p[5] * y + e.p[8];
        out[0] = out[1] = out[2] = out[3] = 0;
        if (wz <= 0) return;
        float pu = (e.p[0] * x + e.p[3] * y + e.p[6]) * e.p[9] / wz - e.p[10] + 0.5f;
        float pv = (e.p[1] * x + e.p[4] * y + e.p[7]) * e.p[9] / wz - e.p[11] + 0.5f;
        if (pu < 0 || pu > 1 || pv < 0 || pv > 1) return;
        float total = e.p[13] * e.p[14], f = floorf(e.p[12]);
        f -= total * floorf(f / total);
        float fu = pu / e.p[13] + fmodf(f, e.p[13]) / e.p[13], fv = pv / e.p[14] + floorf((f + 0.5f) / e.p[13]) / e.p[14];
        float t[4] = { 0, 0, 0, 0 }, l[4], mk[4];
        if (tex) sample(tex->img, fu, fv, flags, t);
        unpremul(t);
        effect_texel(e, 0, t[0], t[0], l);
        unpremul(l);
        effect_texel(e, 1, pu, pv, mk);
        unpremul(mk);
        float a = clampf(l[3] * col[3] * t[3] * mk[0], 0, 1);
        for (int k = 0; k < 3; ++k) out[k] = clampf(l[k] * col[k], 0, 1) * a;
        out[3] = a;
        return;
    }
    case EFFECT_SCRY: {
        const float g = 11.0f, m = e.p[2];
        float gx = ((u * g - g * 0.5f) * (1 + m * 2) - m) + g * 0.5f, gy = ((v * g - g * 0.5f) * (1 + m * 2) - m) + g * 0.5f;
        float fx = floorf(gx), fy = floorf(gy);
        bool valid = fx >= 0 && fy >= 0 && fx < g && fy < g;
        float cell[4];
        effect_texel(e, 2, (fx + 0.5f) / g, (fy + 0.5f) / g, cell);
        float alpha = valid ? cell[0] : 1.0f;
        float border = 0;
        for (int dx = -1; dx <= 1; ++dx)
            for (int dy = -1; dy <= 1; ++dy) {
                if (!dx && !dy) continue;
                float nx = fx + dx, ny = fy + dy;
                if (nx < 0 || ny < 0 || nx >= g || ny >= g) continue;
                float nc[4];
                effect_texel(e, 2, (nx + 0.5f) / g, (ny + 0.5f) / g, nc);
                float bx = maxf(maxf(nx - gx, 0), gx - (nx + 1)), by = maxf(maxf(ny - gy, 0), gy - (ny + 1));
                float bd = sqrtf(bx * bx + by * by);
                border = maxf(border, smoothstepf(0.05f, 0.02f, bd) * fabsf(alpha - nc[0]));
            }
        float cx = u - 0.5f, cy = v - 0.5f, dist = sqrtf(cx * cx + cy * cy);
        float ang = (atan2f(cy, cx) + 3.14159265f) / 6.2831853f;
        float n1[4], n2[4];
        effect_texel(e, 0, dist * 2.0f, ang + e.p[6], n1);
        effect_texel(e, 1, dist * 0.5f, ang + e.p[7], n2);
        float ns = powf(n1[0], 0.8f) * (n2[0] + 0.4f) * 2.0f;
        float cc[3] = { e.extra[0], e.extra[1], e.extra[2] }, lastmask = 0;
        int circles = (int)e.p[0], outer = (int)e.p[1];
        float hx = 5.0f - fabsf(5.0f - fx), hy = 5.0f - fabsf(5.0f - fy);
        bool corner = hx + hy < 3.0f || !valid;
        for (int i = 0; i < 8 && i < circles; ++i) {
            const float* d = &e.extra[(9 + i) * 4];
            float circle = smoothstepf(d[0] * 0.5f, (d[0] + d[1]) * 0.5f, dist);
            float pw = circle > 0 ? powf(circle, d[2]) : (d[2] == 0 ? 1.0f : 0.0f);
            float mask = (1 + (ns - 1) * pw) * (1 - circle);
            mask = mask >= d[3] ? 1.0f : 0.0f;
            if (i == outer && corner) mask = 0;
            lastmask = mask;
            const float* next = &e.extra[(i + 1) * 4];
            for (int k = 0; k < 3; ++k) cc[k] = next[k] + (cc[k] - next[k]) * mask;
        }
        if (outer < 0) lastmask = corner ? 0.0f : 1.0f;
        float oa = clampf(maxf(alpha, border) * lastmask * col[3], 0, 1);
        for (int k = 0; k < 3; ++k) out[k] = clampf((cc[k] + e.p[3 + k] * border) * col[k], 0, 1) * oa;
        out[3] = oa;
        return;
    }
    case EFFECT_SHINE: {
        float t[4] = { 1, 1, 1, 1 };
        if (tex) sample(tex->img, u, v, flags, t);
        unpremul(t);
        float ang = u - v, o = -1.0f + 2.5f * e.p[0];
        float add = (fabsf(ang - o) <= 0.17f ? 0.75f : 0.0f) + (fabsf(ang - o + 0.3f) <= 0.03f ? 0.5f : 0.0f);
        float a = clampf(t[3] * col[3], 0, 1);
        for (int k = 0; k < 3; ++k) out[k] = clampf(t[k] * col[k] + add, 0, 1) * a;
        out[3] = a;
        return;
    }
    case EFFECT_PORTRAIT_BLUR: {
        static const float w[7] = { 0.0702f, 0.1311f, 0.1907f, 0.2160f, 0.1907f, 0.1311f, 0.0702f };
        float tx = e.p[1], ty = e.p[2];
        if (tex && tex->img.w > 0) {
            tx = 1.0f / (float)tex->img.w;
            ty = 1.0f / (float)tex->img.h;
        }
        float r = e.p[0] * 32.0f / 3.0f;
        float acc[3] = { 0, 0, 0 };
        for (int j = 0; j < 7; ++j)
            for (int i = 0; i < 7; ++i) {
                float t[4] = { 1, 1, 1, 1 };
                if (tex) sample(tex->img, u + (float)(i - 3) * r * tx, v + (float)(j - 3) * r * ty, flags, t);
                float ia = t[3] > 0 ? 1.0f / t[3] : 0.0f;
                for (int k = 0; k < 3; ++k) acc[k] += w[i] * w[j] * t[k] * ia;
            }
        for (int k = 0; k < 3; ++k) out[k] = clampf(acc[k] * col[k], 0, 1) * col[3];
        out[3] = col[3];
        return;
    }
    case EFFECT_DISSOLVE: {
        float thr = e.p[0];
        float wx = -200.0f * e.p[2], wy = 1024.0f * e.p[3];
        float d = u - (thr * 1.9f - 0.55f);
        if (d < 0) d = 0;
        float ox = d * wx, oy = d * wy;
        float t[4] = { 1, 1, 1, 1 };
        if (tex) sample(tex->img, u + ox * ox, v + oy * oy, flags, t);
        float strength[2];
        for (int k = 0; k < 2; ++k) {
            float x = u + (k == 0 ? 0.05f : 0.0f);
            float sx = e.p[2] > 0 ? (1.0f / 400.0f) / e.p[2] : 1.0f, sy = e.p[3] > 0 ? (1.0f / 400.0f) / e.p[3] : 1.0f;
            float n1[4], n2[4];
            effect_texel(e, 0, x * sx + e.p[1], v * sy + x * x * x, n1);
            effect_texel(e, 1, x * sx + e.p[1], v * sy + x * x * x, n2);
            float s1 = (n1[0] + x) * 0.5f, s2 = (n2[0] + x) * 0.5f;
            strength[k] = thr >= s1 + (s2 - s1) * 0.4f ? 1.0f : 0.0f;
        }
        for (int k = 0; k < 3; ++k) t[k] *= strength[0];
        float l = t[0] * 0.299f + t[1] * 0.587f + t[2] * 0.114f;
        for (int k = 0; k < 3; ++k) t[k] = l + (t[k] - l) * thr;
        for (int k = 0; k < 4; ++k) out[k] = t[k] * strength[1];
        return;
    }
    case EFFECT_TEXTURE_TRANSITION: {
        float t[4] = { 1, 1, 1, 1 }, tr[4];
        if (tex) sample(tex->img, u, v, flags, t);
        effect_texel(e, 0, u, v, tr);
        float r = tr[3] > 0 ? tr[0] / tr[3] : 0.0f;
        float a = ((1 - r) <= -0.1f + 1.2f * e.p[0] ? 1.0f : 0.0f) + e.p[0] * e.p[1];
        a = clampf(a * e.p[2], 0, 1);
        float ia = t[3] > 0 ? 1.0f / t[3] : 0.0f;
        for (int k = 0; k < 3; ++k) out[k] = clampf(col[k] * t[k] * ia, 0, 1) * a;
        out[3] = a;
        return;
    }
    case EFFECT_DOOM_FLAME: {
        const float* x = e.extra;
        float n1[4], n2[4], m[4], d1[4], d2[4], t[4] = { 1, 1, 1, 1 };
        effect_texel(e, 0, u * e.p[0] + e.p[2], v * e.p[1] + e.p[3], n1);
        effect_texel(e, 1, u * e.p[4] + e.p[6], v * e.p[5] + e.p[7], n2);
        effect_texel(e, 2, u * e.p[8] + e.p[10], (v - 0.04f) * e.p[9] + 0.04f + e.p[11], m);
        effect_texel(e, 3, u * x[0] + x[2], v * x[1] + x[3], d1);
        effect_texel(e, 4, u * x[4] + x[6], v * x[5] + x[7], d2);
        float du = d1[0] * x[8] + d2[0] * x[9], dv = d1[1] * x[8] + d2[1] * x[9];
        if (tex) sample(tex->img, u + du, v + dv, flags, t);
        float tb = t[3] > 0 ? t[2] / t[3] : 0.0f;
        float mb = e.p[13] != 0 ? 1.0f - m[2] : m[2];
        float val = (e.p[12] * n1[2] * n2[2] + mb) * tb;
        float inner = smoothstepf(e.p[14], e.p[15], val);
        float a = clampf(smoothstepf(x[10], x[11], val), 0, 1);
        for (int k = 0; k < 3; ++k) out[k] = clampf(x[12 + k] + inner * x[16 + k], 0, 1) * a;
        out[3] = a;
        return;
    }
    case EFFECT_FORM_PANNING: {
        float uu = u, vv = v;
        if (e.p[11] != 0) {
            /* rotate(UV, 0.5, angle) then polar_coordinates(.., 0.5, 1, 1) */
            float sa = sinf(e.p[12]), ca = cosf(e.p[12]);
            float x = u - 0.5f, y = v - 0.5f;
            float rx = x * sa - y * ca, ry = x * ca + y * sa;
            float r = sqrtf(rx * rx + ry * ry) * 2.0f;
            float ang = atan2f(ry, rx) / (3.1416f * 2.0f);
            uu = r - floorf(r);
            vv = ang - floorf(ang);
        }
        float na[4] = { 0, 0, 0, 0 }, nb[4] = { 0, 0, 0, 0 }, ov[4], mk[4], l[4];
        if (tex) sample(tex->img, uu * e.p[0] + e.p[2], vv * e.p[1] + e.p[3], flags, na);
        if (tex) sample(tex->img, uu * e.p[4] + e.p[6], vv * e.p[5] + e.p[7], flags, nb);
        float ra = na[3] > 0 ? na[0] / na[3] : 0.0f, rb = nb[3] > 0 ? nb[0] / nb[3] : 0.0f;
        effect_texel(e, 1, u, v, ov);
        effect_texel(e, 2, u, v, mk);
        float ovb = ov[3] > 0 ? ov[2] / ov[3] : 0.0f, mr = mk[3] > 0 ? mk[0] / mk[3] : 0.0f;
        float n = smoothstepf(e.p[8], e.p[8] + e.p[9], ((ra + rb) * 0.5f + ovb) * mr);
        effect_texel(e, 0, n, n, l);
        float ia = l[3] > 0 ? 1.0f / l[3] : 0.0f;
        float a = n * (e.p[13] != 0 ? (1.0f - u) * e.p[10] : 1.0f);
        a = clampf(a * col[3], 0, 1);
        for (int k = 0; k < 3; ++k) out[k] = clampf(l[k] * ia * col[k], 0, 1) * a;
        out[3] = a;
        return;
    }
    case EFFECT_SERPENT_SNAKES: {
        float dm[4] = { 0, 0, 0, 0 }, dt[4] = { 0, 0, 0, 0 }, t[4] = { 0, 0, 0, 0 }, l[4];
        if (tex) sample(tex->img, u, v, flags, dm);
        if (tex) sample(tex->img, u * e.p[0] + e.p[2], v * e.p[1] + e.p[3], flags, dt);
        float b = dm[3] > 0 ? dm[2] / dm[3] : 0.0f, g = dt[3] > 0 ? dt[1] / dt[3] : 0.0f;
        float k2 = (g - 0.5f) * 2.0f * e.p[4] * b;
        if (tex) sample(tex->img, u + k2 * e.p[5], v + k2 * e.p[6], flags, t);
        float ta = t[3], tr = ta > 0 ? t[0] / ta : 0.0f;
        effect_texel(e, 0, tr, tr, l);
        float ia = l[3] > 0 ? 1.0f / l[3] : 0.0f;
        float a = clampf(ta * col[3], 0, 1);
        for (int k = 0; k < 3; ++k) out[k] = clampf(l[k] * ia * col[k], 0, 1) * a;
        out[3] = a;
        return;
    }
    case EFFECT_ECHO_LINES: {
        float s1[4] = { 0, 0, 0, 0 }, s0[4] = { 0, 0, 0, 0 }, mk[4];
        float k = e.p[2] != 0 ? 1.0f / e.p[2] : 1.0f;
        if (tex) sample(tex->img, (u - e.p[0]) * k + e.p[0], (v - e.p[1]) * k + e.p[1], flags, s1);
        if (tex) sample(tex->img, u, v, flags, s0);
        effect_texel(e, 0, u, v, mk);
        float mr = mk[3] > 0 ? mk[0] / mk[3] : 0.0f;
        float a = clampf(smoothstepf(e.p[3], e.p[4], s1[3] - s0[3]) * mr * col[3], 0, 1);
        for (int q = 0; q < 3; ++q) out[q] = clampf(col[q], 0, 1) * a;
        out[3] = a;
        return;
    }
    case EFFECT_ROUND_SMOKE: {
        float d[4] = { 0, 0, 0, 0 }, t[4] = { 0, 0, 0, 0 }, l[4];
        if (tex) sample(tex->img, u + e.p[0], v + e.p[1], flags, d);
        float db = d[3] > 0 ? d[2] / d[3] : 0.0f;
        if (tex) sample(tex->img, u + db * e.p[2], v + db * e.p[2], flags, t);
        float tr = t[3] > 0 ? t[0] / t[3] : 0.0f;
        effect_texel(e, 0, tr, tr, l);
        float ia = l[3] > 0 ? 1.0f / l[3] : 0.0f;
        float a = clampf(smoothstepf(e.p[3], e.p[3] + e.p[4], tr) * col[3], 0, 1);
        for (int k = 0; k < 3; ++k) out[k] = clampf(l[k] * ia * col[k], 0, 1) * a;
        out[3] = a;
        return;
    }
    case EFFECT_SCREAM_STREAKS: {
        float mk[4] = { 0, 0, 0, 0 }, t[4] = { 0, 0, 0, 0 };
        if (tex) sample(tex->img, u * e.p[0] + e.p[2] + e.p[5], v * e.p[1] + e.p[3] + e.p[5], flags, mk);
        float m = ((mk[3] > 0 ? mk[2] / mk[3] : 0.0f) - 0.5f) * 2.0f;
        if (tex) sample(tex->img, u + m * e.p[4], v, flags, t);
        float a = clampf(t[3] * col[3], 0, 1);
        for (int k = 0; k < 3; ++k) out[k] = clampf(col[k], 0, 1) * a;
        out[3] = a;
        return;
    }
    case EFFECT_CARD_REMOVE: {
        float d[4] = { 0, 0, 0, 0 }, t[4] = { 0, 0, 0, 0 }, l[4];
        if (tex) sample(tex->img, u + e.p[0], v + e.p[0], flags, d);
        float db = (d[3] > 0 ? d[2] / d[3] : 0.0f) - 0.5f;
        if (tex) sample(tex->img, u + db * e.p[2], v, flags, t);
        float b = smoothstepf(e.p[3], e.p[3] + e.p[5], t[3]);
        effect_texel(e, 0, b, b, l);
        float ia = l[3] > 0 ? 1.0f / l[3] : 0.0f;
        float a = clampf(smoothstepf(e.p[3], e.p[3] + e.p[4], t[3]) * col[3], 0, 1);
        for (int k = 0; k < 3; ++k) out[k] = clampf(l[k] * ia * col[k], 0, 1) * a;
        out[3] = a;
        return;
    }
    case EFFECT_DIVEBOMB_RINGS: {
        float t[4] = { 0, 0, 0, 0 }, l[4];
        if (tex) sample(tex->img, u, v, flags, t);
        float tr = t[3] > 0 ? t[0] / t[3] : 0.0f;
        effect_texel(e, 0, tr, tr, l);
        float ia = l[3] > 0 ? 1.0f / l[3] : 0.0f;
        float a = clampf(col[3] * smoothstepf(e.p[0], e.p[0] + e.p[1], t[3]), 0, 1);
        for (int k = 0; k < 3; ++k) out[k] = clampf(l[k] * ia * col[k], 0, 1) * a;
        out[3] = a;
        return;
    }
    case EFFECT_HYPERBEAM_LINE: {
        float t[4], ov[4], l[4];
        effect_texel(e, 0, u + e.p[0], v, t);
        effect_texel(e, 1, u, v, ov);
        float tr = t[3] > 0 ? t[0] / t[3] : 0.0f, orr = ov[3] > 0 ? ov[0] / ov[3] : 0.0f;
        effect_texel(e, 2, tr + orr, tr + orr, l);
        float ia = l[3] > 0 ? 1.0f / l[3] : 0.0f;
        float a = clampf(t[3] * col[3], 0, 1);
        for (int k = 0; k < 3; ++k) out[k] = clampf(l[k] * ia * col[k], 0, 1) * a;
        out[3] = a;
        return;
    }
    case EFFECT_HYPERBEAM_SPIKES: {
        float t[4] = { 0, 0, 0, 0 }, mk[4];
        if (tex) sample(tex->img, u * e.p[0] + e.p[2], v * e.p[1] + e.p[4] + e.p[3], flags, t);
        effect_texel(e, 0, u, v, mk);
        float tr = t[3] > 0 ? t[0] / t[3] : 0.0f, mr = mk[3] > 0 ? mk[0] / mk[3] : 0.0f;
        float a = clampf(smoothstepf(0.35f, 0.65f, col[3] * tr * mr), 0, 1);
        for (int k = 0; k < 3; ++k) out[k] = clampf(col[k], 0, 1) * a;
        out[3] = a;
        return;
    }
    case EFFECT_SPOTLIGHT: {
        float ang = atan2f(v - e.p[1], u - e.p[0]) / (2 * 3.14159265f) + e.p[2];
        ang -= floorf(ang);
        float d = e.p[5] != 0 ? clampf(fabsf(ang - e.p[4]) / e.p[5], 0, 1) : 1.0f;
        d = d + (1 - 2 * d) * e.p[3];
        d = smoothstepf(e.p[6], e.p[6] + e.p[7], d);
        float a = clampf(col[3], 0, 1);
        for (int k = 0; k < 3; ++k) out[k] = clampf(d * col[k], 0, 1) * a;
        out[3] = a;
        return;
    }
    case EFFECT_AEONGLASS_RAY: {
        float r[4], em[4];
        effect_texel(e, 0, u + e.p[0], v, r);
        effect_texel(e, 1, u * e.p[7], v * e.p[8], em);
        float rr = r[3] > 0 ? r[0] / r[3] : 0.0f, rg = r[3] > 0 ? r[1] / r[3] : 0.0f, eg = em[3] > 0 ? em[1] / em[3] : 0.0f;
        float a = clampf(rg * eg, 0, 1);
        for (int k = 0; k < 3; ++k) out[k] = clampf(e.p[1 + k] * rr + e.p[4 + k] * rg, 0, 1) * a;
        out[3] = a;
        return;
    }
    case EFFECT_SOUL_FYSH_WAVE: {
        float em[4], w[4];
        effect_texel(e, 0, u, v, em);
        effect_texel(e, 1, u + e.p[0], v, w);
        float er = em[3] > 0 ? em[0] / em[3] : 0.0f, wr = w[3] > 0 ? w[0] / w[3] : 0.0f;
        float a = clampf(smoothstepf(e.p[2], e.p[3], er * wr * e.p[1]), 0, 1);
        for (int k = 0; k < 3; ++k) out[k] = clampf(e.p[4 + k], 0, 1) * a;
        out[3] = a;
        return;
    }
    case EFFECT_WATERFALL_WATER: {
        float mv[4], wt[4];
        effect_texel(e, 0, (u + e.p[0]) * e.p[2] + e.p[4], (v + e.p[1]) * e.p[3] + e.p[5], mv);
        effect_texel(e, 1, u, v, wt);
        float im = mv[3] > 0 ? 1.0f / mv[3] : 0.0f, iw = wt[3] > 0 ? 1.0f / wt[3] : 0.0f;
        float a = clampf(wt[3], 0, 1);
        for (int k = 0; k < 3; ++k) out[k] = clampf(e.p[6 + k] * mv[k] * im + wt[k] * iw, 0, 1) * a;
        out[3] = a;
        return;
    }
    case EFFECT_MAP_POINT: {
        float t[4] = { 1, 1, 1, 1 };
        if (tex) sample(tex->img, u, v, flags, t);
        float ia = t[3] > 0 ? 1.0f / t[3] : 0.0f;
        float rgb[3], d2 = 0;
        for (int k = 0; k < 3; ++k) {
            rgb[k] = t[k] * ia * col[k];
            d2 += (rgb[k] - e.p[k]) * (rgb[k] - e.p[k]);
        }
        float a = clampf(t[3] * col[3], 0, 1);
        for (int k = 0; k < 3; ++k) out[k] = clampf(d2 < 0.25f ? e.p[4 + k] : rgb[k], 0, 1) * a;
        out[3] = a;
        return;
    }
    case EFFECT_COLORIZE_ADD: {
        float t[4] = { 1, 1, 1, 1 };
        if (tex) sample(tex->img, u, v, flags, t);
        float ia = t[3] > 0 ? 1.0f / t[3] : 0.0f;
        float a = clampf(t[3] * col[3], 0, 1);
        for (int k = 0; k < 3; ++k) {
            float tk = t[k] * ia;
            out[k] = clampf(e.p[3] != 0 ? e.p[k] * tk : e.p[k] + tk, 0, 1) * a;
        }
        out[3] = a;
        return;
    }
    case EFFECT_EPOCH_CHAINS: {
        float t[4] = { 1, 1, 1, 1 }, er[4];
        if (tex) sample(tex->img, u, v, flags, t);
        effect_texel(e, 0, u, v, er);
        float ia = t[3] > 0 ? 1.0f / t[3] : 0.0f;
        float erv = smoothstepf(e.p[6], e.p[6] + e.p[7], er[3] > 0 ? er[0] / er[3] : 0.0f);
        erv = 1 + (erv - 1) * e.p[5];
        float a = clampf(t[3] * erv * col[3], 0, 1);
        for (int k = 0; k < 3; ++k) {
            float c0 = t[k] * ia;
            out[k] = clampf((c0 + (e.p[k] - c0) * e.p[4]) * col[k], 0, 1) * a;
        }
        out[3] = a;
        return;
    }
    case EFFECT_POWERUP_AURA: {
        float mix_t[4], m1[4], m2[4], sc[4];
        effect_texel(e, 0, u, v, mix_t);
        effect_texel(e, 1, u, v, m1);
        effect_texel(e, 2, u, v, m2);
        effect_texel(e, 3, u + 1.0f, v + e.p[6], sc);
        float k = mix_t[3] > 0 ? mix_t[0] / mix_t[3] : 0.0f;
        float a1 = m1[3] > 0 ? m1[0] / m1[3] : 0.0f, a2 = m2[3] > 0 ? m2[0] / m2[3] : 0.0f;
        float a = clampf(a1 * a2 * sc[3], 0, 1);
        for (int q = 0; q < 3; ++q) out[q] = clampf(e.p[q] + (e.p[3 + q] - e.p[q]) * k, 0, 1) * a;
        out[3] = a;
        return;
    }
    case EFFECT_POISON_SKULL: {
        float d[4] = { 1, 1, 1, 1 }, t[4] = { 1, 1, 1, 1 }, l[4];
        if (tex) sample(tex->img, u + e.p[0], v + e.p[1], flags, d);
        float db = d[3] > 0 ? d[2] / d[3] : 0.0f;
        if (tex) sample(tex->img, u, v + db * e.p[2], flags, t);
        float ta = t[3], tr = ta > 0 ? t[0] / ta : 0.0f, tg = ta > 0 ? t[1] / ta : 0.0f;
        effect_texel(e, 0, tr, tr, l);
        float ia = l[3] > 0 ? 1.0f / l[3] : 0.0f;
        float a = clampf(smoothstepf(e.p[3], e.p[3] + e.p[4], tg) * ta * col[3], 0, 1);
        for (int k = 0; k < 3; ++k) out[k] = clampf(l[k] * ia * col[k], 0, 1) * a;
        out[3] = a;
        return;
    }
    case EFFECT_PANNING: {
        float t[4] = { 1, 1, 1, 1 }, l[4], m[4];
        if (tex) sample(tex->img, u * e.p[0] + e.p[2], v * e.p[1] + e.p[3], flags, t);
        float ta = t[3], tr = ta > 0 ? t[0] / ta : 0.0f;
        effect_texel(e, 0, tr, 0.5f, l);
        effect_texel(e, 1, u, v, m);
        float ia = l[3] > 0 ? 1.0f / l[3] : 0.0f;
        float a = smoothstepf(e.p[4], e.p[4] + e.p[5], ta + (tr - ta) * e.p[7]) * col[3] * (m[3] > 0 ? m[0] / m[3] : 0.0f);
        a = clampf(a, 0, 1);
        for (int k = 0; k < 3; ++k) out[k] = clampf(l[k] * ia * col[k] * e.p[6], 0, 1) * a;
        out[3] = a;
        return;
    }
    default:
        out[0] = out[1] = out[2] = out[3] = 0;
        return;
    }
}

/* vfx_stepped_shader_fire_*'s fragment() at uv: straight colour, alpha. */
static void fire_fragment(const FireParams& f, const Image* base, float u, float v, int flags, float out[4]) {
    float t[4];
    fire_texel(f.noise2, u * f.noise2_uv[0] + f.noise2_uv[2], v * f.noise2_uv[1] + f.noise2_uv[3], true, t);
    float n2 = f.strength[1] * t[2];
    fire_texel(f.noise1, u * f.noise1_uv[0] + f.noise1_uv[2], v * f.noise1_uv[1] + f.noise1_uv[3], true, t);
    float n1 = f.strength[0] * t[2];
    fire_texel(f.noise_mask, u * f.mask_uv[0] + f.mask_uv[2], (v - 0.04f) * f.mask_uv[1] + 0.04f + f.mask_uv[3], false, t);
    float m = f.strength[2] > 0.5f ? 1.0f - t[2] : t[2];
    float a = n1 * n2 + m;
    float d[4], d1[4], dm[4];
    fire_texel(f.dist2, u * f.dist2_uv[0] + f.dist2_uv[2], v * f.dist2_uv[1] + f.dist2_uv[3], true, d);
    fire_texel(f.dist1, u * f.dist1_uv[0] + f.dist1_uv[2], v * f.dist1_uv[1] + f.dist1_uv[3], true, d1);
    fire_texel(f.dist_mask, u * f.dist_mask_uv[0] + f.dist_mask_uv[2], v * f.dist_mask_uv[1] + f.dist_mask_uv[3], false, dm);
    float du = (d[0] * f.dist_strength[1] + d1[0] * f.dist_strength[0]) * dm[0];
    float dv = (d[1] * f.dist_strength[1] + d1[1] * f.dist_strength[0]) * dm[1];
    float b[4] = { 1, 1, 1, 1 };
    if (base) sample(*base, u + du, v + dv, flags, b);
    float val = a * b[2];
    if (f.strength[3] > 0.5f) {
        float s = smoothstepf(f.steps[0], f.steps[1], 1.0f - val);
        for (int k = 0; k < 3; ++k) out[k] = f.outer[k] * (s + f.inner[k]);
    } else {
        float s = smoothstepf(f.steps[0], f.steps[1], val);
        for (int k = 0; k < 3; ++k) out[k] = f.outer[k] + s * f.inner[k];
    }
    out[3] = smoothstepf(f.steps[2], f.steps[3], val);
}

void SwRenderer::triangle(SwTexture* tex, const Vertex& A, const Vertex& B, const Vertex& C, BlendMode blend, int flags) {
    float area = (B.pos.x - A.pos.x) * (C.pos.y - A.pos.y) - (C.pos.x - A.pos.x) * (B.pos.y - A.pos.y);
    if (fabsf(area) < 1e-6f) return;
    ++tris_;
    int x0 = (int)floorf(minf(A.pos.x, minf(B.pos.x, C.pos.x)));
    int y0 = (int)floorf(minf(A.pos.y, minf(B.pos.y, C.pos.y)));
    int x1 = (int)ceilf(maxf(A.pos.x, maxf(B.pos.x, C.pos.x)));
    int y1 = (int)ceilf(maxf(A.pos.y, maxf(B.pos.y, C.pos.y)));
    if (x0 < 0) x0 = 0;
    if (y0 < 0) y0 = 0;
    if (x1 > fb_.w) x1 = fb_.w;
    if (y1 > fb_.h) y1 = fb_.h;
    if (clip_on_) {
        if (x0 < cx0_) x0 = cx0_;
        if (y0 < cy0_) y0 = cy0_;
        if (x1 > cx1_) x1 = cx1_;
        if (y1 > cy1_) y1 = cy1_;
    }
    if (target_ == &layer_ || (flags & DRAW_GROUP)) {
        /* Group draws and their mask stay inside the group's bounds. */
        if (x0 < gx0_) x0 = gx0_;
        if (y0 < gy0_) y0 = gy0_;
        if (x1 > gx1_) x1 = gx1_;
        if (y1 > gy1_) y1 = gy1_;
    }
    /* Coverage is tested at the pixel centre nudged by a tiny odd offset, so a
     * point never lies exactly on an edge: triangles sharing an edge (the two
     * halves of a quad) cover each pixel exactly once. */
    double inv = 1.0 / area;
    /* MSDF: Godot's px_size = max(0.5 * dot(px_range / tex_size, 1 / fwidth(uv)), 1). */
    float msdf_px_size = 1.0f;
    if ((flags & DRAW_MSDF) && tex) {
        double dx1 = B.pos.x - A.pos.x, dy1 = B.pos.y - A.pos.y, dx2 = C.pos.x - A.pos.x, dy2 = C.pos.y - A.pos.y;
        double du1 = B.uv.x - A.uv.x, du2 = C.uv.x - A.uv.x, dv1 = B.uv.y - A.uv.y, dv2 = C.uv.y - A.uv.y;
        double dudx = (du1 * dy2 - du2 * dy1) / area, dudy = (dx1 * du2 - dx2 * du1) / area;
        double dvdx = (dv1 * dy2 - dv2 * dy1) / area, dvdy = (dx1 * dv2 - dx2 * dv1) / area;
        double fu = fabs(dudx) + fabs(dudy), fv = fabs(dvdx) + fabs(dvdy);
        double dest_x = fu > 0 ? 1.0 / fu : 0, dest_y = fv > 0 ? 1.0 / fv : 0;
        double ps = 0.5 * (msdf_range_ / tex->img.w * dest_x + msdf_range_ / tex->img.h * dest_y);
        msdf_px_size = (float)(ps > 1.0 ? ps : 1.0);
    }
    for (int y = y0; y < y1; ++y) {
        double py = y + 0.5 + 0.000713;
        for (int x = x0; x < x1; ++x) {
            double px = x + 0.5 + 0.001371;
            double d0 = ((B.pos.x - px) * (C.pos.y - py) - (C.pos.x - px) * (B.pos.y - py)) * inv;
            double d1 = ((C.pos.x - px) * (A.pos.y - py) - (A.pos.x - px) * (C.pos.y - py)) * inv;
            double d2 = ((A.pos.x - px) * (B.pos.y - py) - (B.pos.x - px) * (A.pos.y - py)) * inv;
            if (d0 < 0 || d1 < 0 || d2 < 0) continue;
            float w0 = (float)d0, w1 = (float)d1, w2 = (float)d2;
            float col[4];
            col[0] = A.color.r * w0 + B.color.r * w1 + C.color.r * w2;
            col[1] = A.color.g * w0 + B.color.g * w1 + C.color.g * w2;
            col[2] = A.color.b * w0 + B.color.b * w1 + C.color.b * w2;
            col[3] = A.color.a * w0 + B.color.a * w1 + C.color.a * w2;
            float s[4];
            if (tex) {
                float u = A.uv.x * w0 + B.uv.x * w1 + C.uv.x * w2;
                float v = A.uv.y * w0 + B.uv.y * w1 + C.uv.y * w2;
                sample(tex->img, u, v, flags, s);
                if (flags & DRAW_MSDF) {
                    float r = s[0], g = s[1], b = s[2];
                    float d = maxf(minf(r, g), minf(maxf(r, g), b));
                    float a;
                    if (msdf_outline_ > 0) {
                        float cr = clampf(msdf_outline_, 0.0f, msdf_range_ / 2.0f - 1.0f) / msdf_range_;
                        d = minf(d, s[3]);
                        a = clampf((d - 0.5f + cr) * msdf_px_size, 0.0f, 1.0f);
                    } else {
                        a = clampf((d - 0.5f) * msdf_px_size + 0.5f, 0.0f, 1.0f);
                    }
                    s[0] = s[1] = s[2] = s[3] = a; /* premultiplied white */
                }
            } else {
                s[0] = s[1] = s[2] = s[3] = 1.0f;
            }
            if (vfx_on_ && tex) {
                /* The erosion particle shaders (VfxParams): s as straight
                 * colour times its alpha, premultiplied. */
                float u = A.uv.x * w0 + B.uv.x * w1 + C.uv.x * w2;
                float v = A.uv.y * w0 + B.uv.y * w1 + C.uv.y * w2;
                if (vfx_.distortion[3] > 0.5f) {
                    float d[4];
                    sample(tex->img, u + vfx_.distortion[0], v + vfx_.distortion[1], DRAW_REPEAT, d);
                    u += d[2] * vfx_.distortion[2];
                    v += d[2] * vfx_.distortion[2];
                }
                if (vfx_.polar > 0.5f) {
                    float dx = u - 0.5f, dy = v - 0.5f;
                    float r = sqrtf(dx * dx + dy * dy) * 2.0f, a = atan2f(dy, dx) / 6.2831853f;
                    u = r - floorf(r);
                    v = a - floorf(a);
                }
                float t[4];
                sample(tex->img, u, v, flags, t);
                float ta = t[3], tr = ta > 0 ? t[0] / ta : 0.0f;
                float rgb[3] = { 1, 1, 1 };
                if (vfx_.lut) {
                    float l[4];
                    sample(((SwTexture*)vfx_.lut)->img, clampf(tr, 0, 1), 0.5f, 0, l);
                    rgb[0] = l[0];
                    rgb[1] = l[1];
                    rgb[2] = l[2];
                }
                float a = ta;
                if (vfx_.erosion[2] > 0.5f) {
                    float ch = vfx_.erosion[3] > 0.5f ? ta : tr;
                    float w = vfx_.erosion[1] != 0 ? vfx_.erosion[1] : 0.0001f;
                    float k = clampf((ch - vfx_.erosion[0]) / w, 0.0f, 1.0f);
                    a = k * k * (3.0f - 2.0f * k);
                }
                s[0] = rgb[0] * a;
                s[1] = rgb[1] * a;
                s[2] = rgb[2] * a;
                s[3] = a;
            }
            if (fire_on_ && fire_.strength[3] > 1.5f) {
                float tm = fire_.strength[0], scroll[4], n1[4], n2[4], lut[4];
                float u = ((float)x + 0.5f) / (float)fb_.w, v = ((float)y + 0.5f) / (float)fb_.h;
                fire_texel(fire_.noise1, u * 0.1f + tm * 0.0005f + 0.24f,
                               v * 0.1f + tm * 0.0005f + 0.24f, true, scroll);
                fire_texel(fire_.noise1, u + tm * 0.0005f + scroll[0], v + tm * 0.0005f + scroll[0], true, n1);
                fire_texel(fire_.noise1, u - tm * 0.0007f + scroll[0], v - tm * 0.0007f + scroll[0], true, n2);
                float value = smoothstepf(0.4f, 0.6f, n1[0] * 0.75f + n2[0] * 0.35f);
                fire_texel(gm_, value, 0, false, lut);
                s[0] = lut[0] * s[3]; s[1] = lut[1] * s[3]; s[2] = lut[2] * s[3];
                col[0] = col[1] = col[2] = 1;
            } else if (fire_on_) {
                /* The stepped fire: its colour (straight) and alpha, the
                 * vertex colour's alpha only. */
                float u = A.uv.x * w0 + B.uv.x * w1 + C.uv.x * w2;
                float v = A.uv.y * w0 + B.uv.y * w1 + C.uv.y * w2;
                float f[4];
                fire_fragment(fire_, tex ? &tex->img : NULL, u, v, flags, f);
                float fa = f[3] < 0 ? 0 : (f[3] > 1 ? 1 : f[3]);
                s[0] = f[0] * fa;
                s[1] = f[1] * fa;
                s[2] = f[2] * fa;
                s[3] = fa;
                col[0] = col[1] = col[2] = 1.0f;
            }
            if (gm_ && tex && !(fire_on_ && fire_.strength[3] > 1.5f)) {
                /* The gradient map: the texel's (straight) red through the
                 * lut; s is premultiplied again after. */
                float r = s[3] > 0 ? s[0] / s[3] : 0.0f;
                float lut[4];
                sample(gm_->img, clampf(r, 0.0f, 1.0f), 0.5f, 0, lut);
                float na = gm_red_ ? r : s[3];
                s[0] = lut[0] * na;
                s[1] = lut[1] * na;
                s[2] = lut[2] * na;
                s[3] = na;
            }
            if (effect_on_) {
                float u = A.uv.x * w0 + B.uv.x * w1 + C.uv.x * w2;
                float v = A.uv.y * w0 + B.uv.y * w1 + C.uv.y * w2;
                effect_pixel(tex, u, v, flags, ((float)x + 0.5f) / (float)fb_.w, ((float)y + 0.5f) / (float)fb_.h, col, s);
                col[0] = col[1] = col[2] = col[3] = 1.0f;
            }
            if (screen_on_ && screen_levels_ > 0) {
                /* ScreenParams: COLOR is the screen copy (the vertex colour
                 * is not applied). */
                float su = ((float)x + 0.5f) / (float)fb_.w, sv = ((float)y + 0.5f) / (float)fb_.h;
                if (screen_.mode == SCREEN_CHROMA) {
                    float u = A.uv.x * w0 + B.uv.x * w1 + C.uv.x * w2;
                    float v = A.uv.y * w0 + B.uv.y * w1 + C.uv.y * w2;
                    float t[4] = { 0, 0, 0, 0 }, n[4], a[4], b[4], g[4];
                    if (tex) sample(tex->img, u, v, flags, t);
                    float tr = t[3] > 0 ? t[0] / t[3] : 0.0f;
                    float dx = 0.5f - u, dy = 0.5f - v, len = sqrtf(dx * dx + dy * dy);
                    fire_texel(screen_.noise, len, len, true, n);
                    float k = screen_.strength * (n[3] > 0 ? n[0] / n[3] : 0.0f) * tr;
                    float nx = len > 1e-6f ? dx / len : 0.0f, ny = len > 1e-6f ? dy / len : 0.0f;
                    sample_screen(su + nx * 0.5f * k, sv + ny * 0.5f * k, 0, a);
                    sample_screen(su, sv, 0, g);
                    sample_screen(su + nx * k, sv + ny * k, 0, b);
                    s[0] = a[0];
                    s[1] = g[1];
                    s[2] = b[2];
                    s[3] = 1;
                } else if (screen_.mode == SCREEN_DISTORT) {
                    float u = A.uv.x * w0 + B.uv.x * w1 + C.uv.x * w2;
                    float v = A.uv.y * w0 + B.uv.y * w1 + C.uv.y * w2;
                    float t[4], m = 0;
                    if (tex) {
                        sample(tex->img, u, v, flags, t);
                        m = t[3] > 0 ? t[0] / t[3] : 0.0f;
                        if (screen_.count == 1) {
                            float dx = u - 0.5f, dy = v - 0.5f;
                            float r = sqrtf(dx * dx + dy * dy) * 2.0f, an = atan2f(dy, dx) / 6.2831853f;
                            float pu = r * screen_.center[0] + screen_.noise_offset[0];
                            float pv = an * screen_.center[1] + screen_.noise_offset[1];
                            float p[4];
                            sample(tex->img, pu, pv, flags, p);
                            m = t[3] * (p[3] > 0 ? p[0] / p[3] : 0.0f);
                        }
                    }
                    float dx = 0.5f - u, dy = 0.5f - v, len = sqrtf(dx * dx + dy * dy);
                    float k = len > 1e-6f ? screen_.strength * col[3] * m / len : 0.0f;
                    sample_screen(su + dx * k, sv + dy * k, 0, s);
                } else if (screen_.mode == SCREEN_DOOM) {
                    /* doom_overlay: inverse(YIQ) * diag(v, s*v, s*v) * YIQ */
                    float a = col[3], sf = 1 + (screen_.mix - 1) * a, vf = 1 + (screen_.strength - 1) * a;
                    float sc[4];
                    sample_screen(su, sv, 0, sc);
                    float y = 0.2989f * sc[0] + 0.5870f * sc[1] + 0.1140f * sc[2];
                    float i = (0.5959f * sc[0] - 0.2774f * sc[1] - 0.3216f * sc[2]) * sf;
                    float q = (0.2115f * sc[0] - 0.5229f * sc[1] + 0.3114f * sc[2]) * sf;
                    y *= vf;
                    i *= vf;
                    q *= vf;
                    float g = powf(sv, 6.0f) * screen_.power * a;
                    s[0] = clampf(y + 0.956f * i + 0.621f * q + g * screen_.color[0], 0, 1);
                    s[1] = clampf(y - 0.272f * i - 0.647f * q + g * screen_.color[1], 0, 1);
                    s[2] = clampf(y - 1.106f * i + 1.703f * q + g * screen_.color[2], 0, 1);
                    s[3] = clampf(sc[3] + g * screen_.color[3], 0, 1);
                } else if (screen_.mode == SCREEN_GLASS) {
                    float u = A.uv.x * w0 + B.uv.x * w1 + C.uv.x * w2;
                    float v = A.uv.y * w0 + B.uv.y * w1 + C.uv.y * w2;
                    float m[4], sc[4];
                    fire_texel(screen_.mask, u, v, false, m);
                    sample_screen(su, sv, screen_.lod, sc);
                    float ia = sc[3] > 0 ? 1.0f / sc[3] : 0.0f, mg = m[3] > 0 ? m[1] / m[3] : 0.0f;
                    float k = mg * screen_.mix;
                    float f[4] = { sc[0] * ia + screen_.strength, sc[1] * ia + screen_.strength, sc[2] * ia + screen_.strength,
                                   m[3] };
                    for (int q = 0; q < 4; ++q) f[q] = (f[q] + (screen_.color[q] - f[q]) * k) * col[q];
                    float a = clampf(f[3] * s[3] * m[3], 0, 1); /* the texel's alpha (premultiplied s) */
                    s[0] = clampf(f[0], 0, 1) * a;
                    s[1] = clampf(f[1], 0, 1) * a;
                    s[2] = clampf(f[2], 0, 1) * a;
                    s[3] = a;
                } else if (screen_.mode == SCREEN_RADIAL) {
                    int n = screen_.count < 1 ? 1 : (screen_.count > 16 ? 16 : screen_.count);
                    float dx = su - screen_.center[0], dy = sv - screen_.center[1], acc[3] = { 0, 0, 0 };
                    for (int i = 0; i < n; ++i) {
                        float t[4];
                        sample_screen(su - screen_.power * dx * i, sv - screen_.power * dy * i, 0, t);
                        acc[0] += t[0];
                        acc[1] += t[1];
                        acc[2] += t[2];
                    }
                    s[0] = acc[0] / n;
                    s[1] = acc[1] / n;
                    s[2] = acc[2] / n;
                    s[3] = 1;
                } else if (screen_.mode == SCREEN_AT_UV) {
                    float u = A.uv.x * w0 + B.uv.x * w1 + C.uv.x * w2;
                    float v = A.uv.y * w0 + B.uv.y * w1 + C.uv.y * w2;
                    sample_screen(u, v, 0, s);
                } else if (screen_.mode == SCREEN_DARK) {
                    sample_screen(su, sv, screen_.lod, s);
                    /* mix(color, vec4(0, 0, 0, 1), k) on the straight colour */
                    float a = s[3], k = screen_.mix, na = a * (1 - k) + k;
                    float ka = a > 0 ? (1 - k) * na / a : 0.0f;
                    s[0] *= ka;
                    s[1] *= ka;
                    s[2] *= ka;
                    s[3] = na;
                } else {
                    float u = A.uv.x * w0 + B.uv.x * w1 + C.uv.x * w2;
                    float v = A.uv.y * w0 + B.uv.y * w1 + C.uv.y * w2;
                    float n[4], m[4];
                    fire_texel(screen_.noise, u + screen_.noise_offset[0], v + screen_.noise_offset[1], true, n);
                    float d = n[0] * screen_.strength - screen_.strength * 0.5f;
                    sample_screen(su + d, sv + d, 0, s);
                    fire_texel(screen_.mask, u, v, false, m);
                    for (int c = 0; c < 4; ++c) s[c] *= m[3];
                }
                col[0] = col[1] = col[2] = col[3] = 1.0f;
            }
            if (flags & DRAW_GROUP) {
                /* The layer (premultiplied) through the mask's alpha. */
                const u8* l = layer_.at(x, y);
                float m = s[3] * col[3];
                s[0] = l[0] * (1.0f / 255.0f) * m;
                s[1] = l[1] * (1.0f / 255.0f) * m;
                s[2] = l[2] * (1.0f / 255.0f) * m;
                s[3] = l[3] * (1.0f / 255.0f) * m;
                col[0] = col[1] = col[2] = col[3] = 1.0f;
            } else if (cm_on_) {
                float r = s[0], g = s[1], b = s[2];
                s[0] = cm_[0] * r + cm_[1] * g + cm_[2] * b + cm_[3] * s[3];
                s[1] = cm_[4] * r + cm_[5] * g + cm_[6] * b + cm_[7] * s[3];
                s[2] = cm_[8] * r + cm_[9] * g + cm_[10] * b + cm_[11] * s[3];
            }
            if (ripple_on_) {
                /* card_ripple: straight colour kept, alpha from the field. */
                float a = s[3] * col[3];
                float sdf = fmodf(ripple_[0] + a, ripple_[1]);
                if (sdf < 0) sdf += ripple_[1];
                float t = ripple_[2] > 0 ? clampf(sdf / ripple_[2], 0.0f, 1.0f) : 1.0f;
                float b = a - sdf + t * t * (3.0f - 2.0f * t) * ripple_[1];
                float w = ripple_[3];
                float u = w > 0 ? clampf((b - (1.0f - w)) / w, 0.0f, 1.0f) : (b >= 1.0f ? 1.0f : 0.0f);
                float na = u * u * (3.0f - 2.0f * u);
                float inv = s[3] > 0 ? 1.0f / s[3] : 0.0f;
                s[0] *= inv;
                s[1] *= inv;
                s[2] *= inv;
                s[3] = 1.0f;
                col[3] = na; /* s is straight now: the premultiply below uses na */
            }
            /* premultiplied source = texel(premul) * modulate (straight) */
            float sa = s[3] * col[3];
            float sr = s[0] * col[0] * col[3], sg = s[1] * col[1] * col[3], sb = s[2] * col[2] * col[3];
            if (sa <= 0.0f && blend == BLEND_MIX) continue;
            u8* d = target_->at(x, y);
            float dr = d[0] / 255.0f, dg = d[1] / 255.0f, db = d[2] / 255.0f, da = d[3] / 255.0f;
            float r, g, b, a;
            switch (blend) {
            case BLEND_ADD:
                r = dr + sr; g = dg + sg; b = db + sb; a = da + sa * (1 - da);
                break;
            case BLEND_SUB:
                r = dr - sr; g = dg - sg; b = db - sb; a = da;
                break;
            case BLEND_MUL:
                r = dr * (s[0] * col[0]); g = dg * (s[1] * col[1]); b = db * (s[2] * col[2]); a = da;
                break;
            default: /* MIX and PREMULT_ALPHA: premultiplied over */
                r = sr + dr * (1 - sa); g = sg + dg * (1 - sa); b = sb + db * (1 - sa); a = sa + da * (1 - sa);
                break;
            }
            d[0] = (u8)(clampf(r, 0, 1) * 255 + 0.5f);
            d[1] = (u8)(clampf(g, 0, 1) * 255 + 0.5f);
            d[2] = (u8)(clampf(b, 0, 1) * 255 + 0.5f);
            d[3] = (u8)(clampf(a, 0, 1) * 255 + 0.5f);
        }
    }
}

} /* namespace spire */

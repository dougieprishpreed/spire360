#include "sw_renderer.h"

#include <math.h>
#include <string.h>

namespace spire {

SwRenderer::SwRenderer()
    : target_(&fb_), gx0_(0), gy0_(0), gx1_(0), gy1_(0), clip_on_(false), cx0_(0), cy0_(0), cx1_(0), cy1_(0), tris_(0), msdf_range_(16), msdf_outline_(0), cm_on_(false), ripple_on_(false), gm_(NULL), gm_red_(false), fire_on_(false), vfx_on_(false) {
    for (int i = 0; i < 4; ++i) ripple_[i] = 0;
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
}

bool SwRenderer::begin_group(const Rect2& b) {
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

static float smoothstepf(float e0, float e1, float x) {
    float t = e1 != e0 ? (x - e0) / (e1 - e0) : (x >= e1 ? 1.0f : 0.0f);
    t = t < 0 ? 0 : (t > 1 ? 1 : t);
    return t * t * (3.0f - 2.0f * t);
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
            if (fire_on_) {
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
            if (gm_ && tex) {
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

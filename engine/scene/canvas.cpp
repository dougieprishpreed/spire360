#include "canvas.h"

#include "../core/pack.h"

#include <math.h>

namespace spire {

Canvas::Canvas(Renderer* r, const Pack* pack)
    : r_(r), pack_(pack), blend_(BLEND_MIX), tex_flags_(0), calls_(0), count_only_(false) {}

void Canvas::set_item(const Xform2D& xform, const Color& modulate, BlendMode blend, int tex_flags) {
    xform_ = xform;
    modulate_ = modulate;
    blend_ = blend;
    tex_flags_ = tex_flags;
}

void Canvas::set_clip(const Rect2* screen_rect) { r_->set_clip(screen_rect); }

Texture* Canvas::gpu_texture(ImageTextureRes* img) {
    if (!img) return NULL;
    if (!img->loaded) {
        img->loaded = true;
        std::vector<u8> data;
        TextureData td;
        if (pack_ && pack_->read(img->path.c_str(), data) && Pack::parse_texture(data, td)) {
            img->gpu = r_->create_texture(td);
        } else {
            log_warn("texture %s: not in the pack", img->path.c_str());
        }
    }
    return img->gpu;
}

void Canvas::emit_quad(Texture* tex, const Vec2& p0, const Vec2& p1, const Vec2& uv0, const Vec2& uv1, const Color& c,
                       int flags) {
    Vec2 a = xform_.xform(p0), b = xform_.xform(Vec2(p1.x, p0.y));
    Vec2 cc = xform_.xform(p1), d = xform_.xform(Vec2(p0.x, p1.y));
    Vertex v[6];
    v[0] = Vertex(a, uv0, c);
    v[1] = Vertex(b, Vec2(uv1.x, uv0.y), c);
    v[2] = Vertex(cc, uv1, c);
    v[3] = v[0];
    v[4] = v[2];
    v[5] = Vertex(d, Vec2(uv0.x, uv1.y), c);
    if (!count_only_) r_->draw_triangles(tex, v, 6, blend_, flags | tex_flags_);
    ++calls_;
}

void Canvas::draw_rect(const Rect2& r, const Color& c) {
    emit_quad(NULL, r.pos, r.end(), Vec2(0, 0), Vec2(1, 1), modulate_ * c, 0);
}

/* A rect of an image: negative dst sizes flip in place, as Godot's
 * canvas_item_add_texture_rect(_region) does. src is in the image's layout
 * pixels. */
void Canvas::image_quad(ImageTextureRes* img, Rect2 dst, Rect2 src, const Color& mod, int extra_flags) {
    Texture* tex = gpu_texture(img);
    if (!tex) return;
    Vec2 ts = img->size();
    if (ts.x <= 0 || ts.y <= 0) return;
    bool fh = false, fv = false;
    if (dst.size.x < 0) {
        fh = true;
        dst.size.x = -dst.size.x;
    }
    if (dst.size.y < 0) {
        fv = true;
        dst.size.y = -dst.size.y;
    }
    if (src.size.x < 0) {
        fh = !fh;
        src.pos.x += src.size.x;
        src.size.x = -src.size.x;
    }
    if (src.size.y < 0) {
        fv = !fv;
        src.pos.y += src.size.y;
        src.size.y = -src.size.y;
    }
    Vec2 uv0(src.pos.x / ts.x, src.pos.y / ts.y);
    Vec2 uv1((src.pos.x + src.size.x) / ts.x, (src.pos.y + src.size.y) / ts.y);
    if (fh) {
        float t = uv0.x;
        uv0.x = uv1.x;
        uv1.x = t;
    }
    if (fv) {
        float t = uv0.y;
        uv0.y = uv1.y;
        uv1.y = t;
    }
    emit_quad(tex, dst.pos, dst.end(), uv0, uv1, modulate_ * mod, extra_flags);
}

void Canvas::draw_texture_rect(Texture2DRes* t, const Rect2& r, bool tile, const Color& mod) {
    if (!t) return;
    if (t->kind == Resource::RES_IMAGE_TEXTURE) {
        ImageTextureRes* img = (ImageTextureRes*)t;
        if (tile) {
            image_quad(img, r, Rect2(0, 0, fabsf(r.size.x), fabsf(r.size.y)), mod, DRAW_REPEAT);
        } else {
            image_quad(img, r, Rect2(Vec2(0, 0), img->size()), mod, 0);
        }
        return;
    }
    /* AtlasTexture::draw_rect: no tiling. */
    Vec2 s = t->size();
    Rect2 dr, sr;
    ImageTextureRes* img = NULL;
    if (t->get_rect_region(r, Rect2(0, 0, s.x, s.y), dr, sr, &img)) image_quad(img, dr, sr, mod, 0);
}

void Canvas::draw_texture_rect_region(Texture2DRes* t, const Rect2& r, const Rect2& src, const Color& mod) {
    if (!t) return;
    Rect2 dr, sr;
    ImageTextureRes* img = NULL;
    if (t->get_rect_region(r, src, dr, sr, &img)) image_quad(img, dr, sr, mod, 0);
}

struct Seg {
    float d0, d1, s0, s1;
};

/* One axis of the nine patch, as the canvas shader's map_ninepatch_axis:
 * the begin margin wins when the rect is smaller than both margins. Returns
 * the index range of the centre segments in [*c0, *c1). */
static void np_axis(float size, float src0, float src_size, float mb, float me, int mode, std::vector<Seg>& out,
                    int* c0, int* c1) {
    out.clear();
    float x1 = minf(mb, size);
    float x2 = maxf(x1, size - me);
    Seg s;
    if (x1 > 0) {
        s.d0 = 0;
        s.d1 = x1;
        s.s0 = src0;
        s.s1 = src0 + x1;
        out.push_back(s);
    }
    *c0 = (int)out.size();
    float area = x2 - x1;
    float src_area = src_size - mb - me;
    /* A source centre of zero or negative width (margins wider than the
     * texture) still stretches: the shader maps the centre linearly from
     * the begin margin to the end one, backwards when they overlap. Tiling
     * needs a centre to repeat. */
    if (area > 0 && (src_area > 0 || (src_area <= 0 && mode == AXIS_STRETCH))) {
        if (mode == AXIS_TILE) {
            for (float p = 0; p < area; p += src_area) {
                float len = minf(src_area, area - p);
                s.d0 = x1 + p;
                s.d1 = x1 + p + len;
                s.s0 = src0 + mb;
                s.s1 = src0 + mb + len;
                out.push_back(s);
            }
        } else if (mode == AXIS_TILE_FIT) {
            float n = maxf(1.0f, floorf(area / src_area + 0.5f));
            float step = area / n;
            for (int i = 0; i < (int)n; ++i) {
                s.d0 = x1 + step * i;
                s.d1 = x1 + step * (i + 1);
                s.s0 = src0 + mb;
                s.s1 = src0 + src_size - me;
                out.push_back(s);
            }
        } else {
            s.d0 = x1;
            s.d1 = x2;
            s.s0 = src0 + mb;
            s.s1 = src0 + src_size - me;
            out.push_back(s);
        }
    }
    *c1 = (int)out.size();
    if (size > x2) {
        s.d0 = x2;
        s.d1 = size;
        s.s0 = src0 + src_size - (size - x2);
        s.s1 = src0 + src_size;
        out.push_back(s);
    }
}

void Canvas::draw_nine_patch(Texture2DRes* t, const Rect2& rect_in, const Rect2& src_in, const float margin[4],
                             int axis_h, int axis_v, bool draw_center, const Color& mod) {
    if (!t) return;
    Rect2 rect, src;
    ImageTextureRes* img = NULL;
    if (!t->get_rect_region(rect_in, src_in, rect, src, &img)) return;
    Texture* tex = gpu_texture(img);
    if (!tex) return;
    if (src.size.x == 0 || src.size.y == 0) src = Rect2(Vec2(0, 0), img->size());
    Vec2 ts = img->size();
    std::vector<Seg> hx, vy;
    int hc0, hc1, vc0, vc1;
    np_axis(rect.size.x, src.pos.x, src.size.x, margin[SIDE_LEFT], margin[SIDE_RIGHT], axis_h, hx, &hc0, &hc1);
    np_axis(rect.size.y, src.pos.y, src.size.y, margin[SIDE_TOP], margin[SIDE_BOTTOM], axis_v, vy, &vc0, &vc1);
    Color c = modulate_ * mod;
    for (int j = 0; j < (int)vy.size(); ++j) {
        bool vcen = j >= vc0 && j < vc1;
        for (int i = 0; i < (int)hx.size(); ++i) {
            bool hcen = i >= hc0 && i < hc1;
            if (vcen && hcen && !draw_center) continue;
            const Seg& a = hx[i];
            const Seg& b = vy[j];
            emit_quad(tex, Vec2(rect.pos.x + a.d0, rect.pos.y + b.d0), Vec2(rect.pos.x + a.d1, rect.pos.y + b.d1),
                      Vec2(a.s0 / ts.x, b.s0 / ts.y), Vec2(a.s1 / ts.x, b.s1 / ts.y), c, 0);
        }
    }
}

void Canvas::draw_msdf(ImageTextureRes* page, const Rect2& dst, const Rect2& uv, const Color& color, float px_range,
                       float outline) {
    Texture* tex = gpu_texture(page);
    if (!tex) return;
    Vec2 ts = page->size();
    if (!count_only_) r_->set_msdf(px_range, outline);
    emit_quad(tex, dst.pos, dst.end(), Vec2(uv.pos.x / ts.x, uv.pos.y / ts.y),
              Vec2((uv.pos.x + uv.size.x) / ts.x, (uv.pos.y + uv.size.y) / ts.y), modulate_ * color, DRAW_MSDF);
}

void Canvas::draw_colored_triangles(const std::vector<Vec2>& v, const std::vector<int>& idx,
                                    const std::vector<Color>& colors) {
    if (idx.empty()) return;
    verts_.resize(idx.size());
    for (size_t i = 0; i < idx.size(); ++i) {
        int k = idx[i];
        verts_[i] = Vertex(xform_.xform(v[k]), Vec2(0, 0), modulate_ * colors[k]);
    }
    if (!count_only_) r_->draw_triangles(NULL, &verts_[0], (int)verts_.size(), blend_, tex_flags_);
    ++calls_;
}

void Canvas::draw_triangles(ImageTextureRes* img, const std::vector<Vec2>& v, const std::vector<Vec2>& uv,
                            const std::vector<Color>& colors, const std::vector<int>& idx) {
    Texture* tex = img ? gpu_texture(img) : NULL;
    if (img && !tex) return;
    if (idx.empty()) return;
    verts_.resize(idx.size());
    for (size_t i = 0; i < idx.size(); ++i) {
        int k = idx[i];
        verts_[i] = Vertex(xform_.xform(v[k]), uv[k], modulate_ * (colors.empty() ? Color() : colors[k]));
    }
    if (!count_only_) r_->draw_triangles(tex, &verts_[0], (int)verts_.size(), blend_, tex_flags_);
    ++calls_;
}

} /* namespace spire */

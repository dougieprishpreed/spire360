#include "canvas.h"

#include "../core/pack.h"

#include <math.h>

namespace spire {

Canvas::Canvas(Renderer* r, const Pack* pack)
    : texture_bytes_(0), texture_budget_(80 * 1024 * 1024), texture_clock_(0), frame_(0), console_sizes_(false),
      dump_pending_(false), dumping_(false), dump_draws_(0), dump_glyphs_(0), dump_what_(NULL),
      decoded_w_(0), decoded_h_(0),
      r_(r), pack_(pack), blend_(BLEND_MIX), tex_flags_(0), calls_(0), count_only_(false), wind_on_(false), wind_(0),
      wind_height_(0) {
    stats_.created = stats_.evicted = stats_.over_frames = stats_.starved = 0;
    stats_.peak_bytes = 0;
}

Canvas::~Canvas() {
    for (std::map<std::string, CachedTexture>::iterator it = textures_.begin(); it != textures_.end(); ++it)
        if (it->second.texture) r_->destroy_texture(it->second.texture);
}

static std::vector<Texture*> g_released;

void Canvas::release_later(Texture* t) {
    if (t) g_released.push_back(t);
}

void Canvas::begin_frame() {
    ++frame_;
    dumping_ = dump_pending_;
    dump_pending_ = false;
    dump_draws_ = dump_glyphs_ = 0;
    if (dumping_) log_info("draw dump: frame %lu, %u textures, %.1f MB", (unsigned long)frame_, (unsigned)texture_count(),
                           texture_bytes_ / 1048576.0);
    for (size_t i = 0; i < g_released.size(); ++i) r_->destroy_texture(g_released[i]);
    g_released.clear();
    /* Each tree is a canvas of its own: its first screen-reading item
     * copies the screen again. */
    r_->screen_barrier();
}

size_t Canvas::texture_count() const {
    size_t count = 0;
    for (std::map<std::string, CachedTexture>::const_iterator it = textures_.begin(); it != textures_.end(); ++it)
        if (it->second.texture) ++count;
    return count;
}

void Canvas::end_frame() {
    /* Samplers and queued batches may hold textures until the last draw.
     * Evict only here, after the complete scene has submitted its draws. */
    r_->set_gradient_map(NULL, false);
    r_->set_fire(NULL);
    r_->set_vfx(NULL);
    r_->set_screen(NULL);
    r_->set_effect(NULL);
    if (!decoded_path_.empty()) {
        decoded_path_.clear();
        std::vector<u8>().swap(decoded_.px);
        decoded_.w = decoded_.h = 0;
    }
    if (dumping_) {
        log_info("draw dump: %u draws, %u glyphs", dump_draws_, dump_glyphs_);
        dumping_ = false;
    }
    if (texture_bytes_ > stats_.peak_bytes) stats_.peak_bytes = texture_bytes_;
    /* Over the budget the least recently drawn go first, but never one this
     * frame drew: a frame that needs more than the budget keeps it all
     * (evicting what was just drawn made it load again each frame, and the
     * console's GPU read it after it was freed). */
    if (texture_bytes_ > texture_budget_) ++stats_.over_frames;
    for (;;) {
        std::map<std::string, CachedTexture>::iterator oldest = textures_.end();
        for (std::map<std::string, CachedTexture>::iterator it = textures_.begin(); it != textures_.end(); ++it) {
            if (!it->second.texture) continue;
            bool expired = !it->second.kept && frame_ > it->second.frame && frame_ - it->second.frame > 120;
            if (texture_bytes_ <= texture_budget_ && !expired) continue;
            if (!expired && it->second.frame == frame_) continue;
            if (oldest == textures_.end() || it->second.touched < oldest->second.touched) oldest = it;
        }
        if (oldest == textures_.end()) break;
        texture_bytes_ -= oldest->second.bytes;
        r_->destroy_texture(oldest->second.texture);
        textures_.erase(oldest);
        ++stats_.evicted;
    }
    if (texture_bytes_ > texture_budget_) ++stats_.starved;
}

Canvas::TextureStats Canvas::take_texture_stats() {
    TextureStats s = stats_;
    stats_.created = stats_.evicted = stats_.over_frames = stats_.starved = 0;
    stats_.peak_bytes = texture_bytes_;
    return s;
}

/* Xbox 360 texture memory: blocks (DXT) or texels tiled in 32 x 32, the
 * levels in 4 KB pages (the D3D9 renderer's count on the console). */
static size_t console_texture_bytes(const TextureData& d) {
    size_t total = 0;
    bool block = d.format == TEX_DXT1 || d.format == TEX_DXT5;
    int levels = d.levels.empty() ? 0 : 1; /* the console uploads the base level only */
    for (int lv = 0; lv < levels; ++lv) {
        int w = d.width >> lv, h = d.height >> lv;
        if (w < 1) w = 1;
        if (h < 1) h = 1;
        size_t bx = block ? (size_t)(w + 3) / 4 : (size_t)w, by = block ? (size_t)(h + 3) / 4 : (size_t)h;
        size_t bytes = ((bx + 31) & ~(size_t)31) * ((by + 31) & ~(size_t)31) *
                       (size_t)(block ? (d.format == TEX_DXT1 ? 8 : 16) : 4);
        total += (bytes + 4095) & ~(size_t)4095;
    }
    return total;
}

Texture* Canvas::cached_texture(const std::string& key) {
    std::map<std::string, CachedTexture>::iterator it = textures_.find(key);
    if (it == textures_.end()) return NULL;
    it->second.touched = ++texture_clock_;
    it->second.frame = frame_;
    return it->second.texture;
}

Texture* Canvas::cache_texture(const std::string& key, const TextureData& data) {
    Texture* found = cached_texture(key);
    if (found) return found;
    CachedTexture entry;
    entry.texture = r_->create_texture(data);
    if (!entry.texture) return NULL;
    /* Use the backend's allocation estimate, including tiled alignment and
     * decoded software pixels, rather than compressed pack payload bytes. */
    entry.bytes = console_sizes_ ? console_texture_bytes(data) : entry.texture->memory_bytes;
    ++stats_.created;
    entry.touched = ++texture_clock_;
    entry.frame = frame_;
    textures_[key] = entry;
    texture_bytes_ += entry.bytes;
    return entry.texture;
}

void Canvas::set_item(const Xform2D& xform, const Color& modulate, BlendMode blend, int tex_flags) {
    xform_ = xform;
    modulate_ = modulate;
    blend_ = blend;
    tex_flags_ = tex_flags;
}

void Canvas::set_clip(const Rect2* screen_rect) { r_->set_clip(screen_rect); }

/* DerivedImage: the source's pixels (decoded from the pack, bilinear) at
 * width x height as a stretched TextureRect draws them, the colour matrix,
 * over the clear colour; the dark_blur of that (its mip chain, the level at
 * lod, mixed toward black). */
bool Canvas::derive(const DerivedImage& d, TextureData& out) {
    if (!d.source || d.width <= 0 || d.height <= 0 || !pack_) return false;
    Rect2 od, os;
    ImageTextureRes* src = NULL;
    Rect2 full(Vec2(0, 0), Vec2((float)d.width, (float)d.height));
    if (!d.source->get_rect_region(full, Rect2(), od, os, &src) || !src || src->derived) return false;
    if (os.size.x == 0 && os.size.y == 0) os = Rect2(Vec2(0, 0), src->size());
    if (decoded_path_ != src->path) {
        std::vector<u8> data;
        TextureData td;
        if (!pack_->read(src->path.c_str(), data) || !Pack::parse_texture(data, td) || td.levels.empty() ||
            td.levels[0].empty() ||
            !decode_texture_level(td.format, td.width, td.height, &td.levels[0][0], td.levels[0].size(), decoded_,
                                  (td.flags & TEXF_DATA) == 0))
            return false;
        decoded_path_ = src->path;
        decoded_w_ = td.orig_width > 0 ? td.orig_width : td.width;
        decoded_h_ = td.orig_height > 0 ? td.orig_height : td.height;
    }
    std::vector<Image> levels(1);
    Image& base = levels[0];
    base.resize(d.width, d.height);
    for (int y = 0; y < d.height; ++y)
        for (int x = 0; x < d.width; ++x) {
            float px = x + 0.5f, py = y + 0.5f, c[4] = { 0, 0, 0, 0 };
            if (px >= od.pos.x && py >= od.pos.y && px < od.pos.x + od.size.x && py < od.pos.y + od.size.y) {
                float lx = os.pos.x + (px - od.pos.x) / od.size.x * os.size.x;
                float ly = os.pos.y + (py - od.pos.y) / od.size.y * os.size.y;
                image_bilinear(decoded_, lx / decoded_w_, ly / decoded_h_, c);
            }
            if (d.cm_on) {
                float r = c[0], g = c[1], b = c[2];
                c[0] = d.cm[0] * r + d.cm[1] * g + d.cm[2] * b + d.cm[3] * c[3];
                c[1] = d.cm[4] * r + d.cm[5] * g + d.cm[6] * b + d.cm[7] * c[3];
                c[2] = d.cm[8] * r + d.cm[9] * g + d.cm[10] * b + d.cm[11] * c[3];
            }
            float k = 1 - c[3];
            float o[4] = { c[0] + d.clear.r * d.clear.a * k, c[1] + d.clear.g * d.clear.a * k,
                           c[2] + d.clear.b * d.clear.a * k, c[3] + d.clear.a * k };
            u8* p = base.at(x, y);
            for (int i = 0; i < 4; ++i) p[i] = (u8)(clampf(o[i], 0, 1) * 255 + 0.5f);
        }
    int need = (int)ceilf(d.lod);
    while ((int)levels.size() <= need) {
        const Image& top = levels.back();
        if (top.w <= 1 && top.h <= 1) break;
        levels.push_back(Image());
        blur_mip_level(levels[levels.size() - 2], levels.back());
    }
    out.format = TEX_ARGB8;
    out.flags = 0; /* straight alpha: premultiplied on upload */
    out.width = out.orig_width = d.width;
    out.height = out.orig_height = d.height;
    out.levels.assign(1, std::vector<u8>((size_t)d.width * d.height * 4));
    for (int y = 0; y < d.height; ++y)
        for (int x = 0; x < d.width; ++x) {
            float c[4];
            image_lod(&levels[0], (int)levels.size(), (x + 0.5f) / d.width, (y + 0.5f) / d.height, d.lod, c);
            /* mix(color, vec4(0, 0, 0, 1), mix): straight */
            float a = c[3], na = a * (1 - d.mix) + d.mix;
            float ka = a > 0 ? (1 - d.mix) / a : 0.0f;
            u8* p = &out.levels[0][((size_t)y * d.width + x) * 4]; /* B G R A */
            p[0] = (u8)(clampf(c[2] * ka, 0, 1) * 255 + 0.5f);
            p[1] = (u8)(clampf(c[1] * ka, 0, 1) * 255 + 0.5f);
            p[2] = (u8)(clampf(c[0] * ka, 0, 1) * 255 + 0.5f);
            p[3] = (u8)(clampf(na, 0, 1) * 255 + 0.5f);
        }
    return true;
}

bool Canvas::prefetch(const std::string& path) {
    const std::string key = "pack:" + path;
    if (textures_.count(key)) return false;
    std::vector<u8> data;
    TextureData td;
    double started = load_trace_start();
    if (!pack_ || !pack_->read(path.c_str(), data) || !Pack::parse_texture(data, td) || !cache_texture(key, td))
        return false;
    textures_[key].kept = true;
    load_traced("texture (warm)", path, started);
    return true;
}

Texture* Canvas::gpu_texture(ImageTextureRes* img) {
    if (!img) return NULL;
    if (img->render_target) return img->rendered;
    const std::string key = "pack:" + img->path;
    std::map<std::string, CachedTexture>::iterator it = textures_.find(key);
    if (it != textures_.end()) return cached_texture(key);
    std::vector<u8> data;
    TextureData td;
    double started = load_trace_start();
    if (img->derived) {
        if (derive(*img->derived, td)) {
            Texture* t = cache_texture(key, td);
            load_traced("texture", img->path, started);
            return t;
        }
        log_warn("texture %s: could not be made", img->path.c_str());
        textures_[key] = CachedTexture();
        return NULL;
    }
    if (pack_ && pack_->read(img->path.c_str(), data) && Pack::parse_texture(data, td)) {
        Texture* t = cache_texture(key, td);
        load_traced("texture", img->path, started);
        return t;
    }
    log_warn("texture %s: not in the pack", img->path.c_str());
    textures_[key] = CachedTexture(); /* warn once for a missing resource */
    return NULL;
}

void Canvas::emit_quad(Texture* tex, const Vec2& p0, const Vec2& p1, const Vec2& uv0, const Vec2& uv1, const Color& c,
                       int flags) {
    Vec2 a = xform_.xform(sway(p0, uv0)), b = xform_.xform(sway(Vec2(p1.x, p0.y), Vec2(uv1.x, uv0.y)));
    Vec2 cc = xform_.xform(sway(p1, uv1)), d = xform_.xform(sway(Vec2(p0.x, p1.y), Vec2(uv0.x, uv1.y)));
    Vertex v[6];
    v[0] = Vertex(a, uv0, c);
    v[1] = Vertex(b, Vec2(uv1.x, uv0.y), c);
    v[2] = Vertex(cc, uv1, c);
    v[3] = v[0];
    v[4] = v[2];
    v[5] = Vertex(d, Vec2(uv0.x, uv1.y), c);
    if (dumping_ && !count_only_ && (flags & DRAW_MSDF)) ++dump_glyphs_;
    else if (dumping_ && !count_only_)
        log_info("draw %u: %s %dx%d f%d (%.0f, %.0f) (%.0f, %.0f) colour %.2f %.2f %.2f %.2f blend %d", ++dump_draws_,
                 dump_what_ ? dump_what_ : "rect", tex ? tex->width : 0, tex ? tex->height : 0, tex ? tex->format : 0, a.x,
                 a.y, cc.x, cc.y, c.r, c.g, c.b, c.a, (int)blend_);
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
    if (dumping_ && !tex && !count_only_) log_info("draw %u: %s (no texture)", ++dump_draws_, img->path.c_str());
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
    dump_what_ = img->path.c_str();
    emit_quad(tex, dst.pos, dst.end(), uv0, uv1, modulate_ * mod, extra_flags);
    dump_what_ = NULL;
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
                            const std::vector<Color>& colors, const std::vector<int>& idx, int flags) {
    Texture* tex = img ? gpu_texture(img) : NULL;
    if (dumping_ && !count_only_)
        log_info("draw %u: triangles %s %dx%d f%d, %d vertices%s", ++dump_draws_, img ? img->path.c_str() : "(colour)",
                 tex ? tex->width : 0, tex ? tex->height : 0, tex ? tex->format : 0, (int)idx.size(),
                 img && !tex ? " (no texture)" : "");
    if (img && !tex) return;
    if (idx.empty()) return;
    verts_.resize(idx.size());
    for (size_t i = 0; i < idx.size(); ++i) {
        int k = idx[i];
        verts_[i] = Vertex(xform_.xform(sway(v[k], uv[k])), uv[k], modulate_ * (colors.empty() ? Color() : colors[k]));
    }
    if (!count_only_) r_->draw_triangles(tex, &verts_[0], (int)verts_.size(), blend_, tex_flags_ | flags);
    ++calls_;
}

} /* namespace spire */

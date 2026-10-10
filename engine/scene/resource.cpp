/* Texture region mapping and style box drawing, ported from Godot 4.5.1
 * (scene/resources/atlas_texture.cpp, style_box_flat.cpp,
 * style_box_texture.cpp; MIT, see THIRD_PARTY.md). */
#include "resource.h"

#include "canvas.h"

#include <float.h>
#include <math.h>

namespace spire {

static const float PI_F = 3.14159265358979f;

bool ImageTextureRes::get_rect_region(const Rect2& dst, const Rect2& src, Rect2& out_dst, Rect2& out_src,
                                      ImageTextureRes** image) {
    out_dst = dst;
    out_src = src;
    *image = this;
    return true;
}

Rect2 AtlasTextureRes::region_rect() const {
    Rect2 rc = region;
    if (atlas) {
        if (rc.size.x == 0) rc.size.x = atlas->size().x;
        if (rc.size.y == 0) rc.size.y = atlas->size().y;
    }
    return rc;
}

Vec2 AtlasTextureRes::size() const {
    Vec2 s;
    s.x = region.size.x == 0 ? (atlas ? atlas->size().x : 1) : region.size.x + margin.size.x;
    s.y = region.size.y == 0 ? (atlas ? atlas->size().y : 1) : region.size.y + margin.size.y;
    return s;
}

bool AtlasTextureRes::get_rect_region(const Rect2& p_rect, const Rect2& p_src, Rect2& r_rect, Rect2& r_src,
                                      ImageTextureRes** image) {
    if (!atlas) return false;
    Rect2 src = p_src;
    if (src.size.x == 0 && src.size.y == 0) src.size = region.size;
    if (src.size.x == 0 && src.size.y == 0) src.size = atlas->size();
    Vec2 scale(p_rect.size.x / src.size.x, p_rect.size.y / src.size.y);
    src.pos = src.pos + (region.pos - margin.pos);
    /* Rect2::intersection (empty when they do not overlap) */
    Rect2 rr = region_rect();
    float x0 = maxf(rr.pos.x, src.pos.x), y0 = maxf(rr.pos.y, src.pos.y);
    float x1 = minf(rr.pos.x + rr.size.x, src.pos.x + src.size.x);
    float y1 = minf(rr.pos.y + rr.size.y, src.pos.y + src.size.y);
    if (x1 <= x0 || y1 <= y0) return false;
    Rect2 clipped(x0, y0, x1 - x0, y1 - y0);
    Vec2 ofs = clipped.pos - src.pos;
    if (scale.x < 0) ofs.x += clipped.size.x - src.size.x;
    if (scale.y < 0) ofs.y += clipped.size.y - src.size.y;
    Rect2 dst(p_rect.pos + ofs * scale, clipped.size * scale);
    /* The atlas is an image texture in every case the game uses; nested
     * atlases map again. */
    return atlas->get_rect_region(dst, clipped, r_rect, r_src, image);
}

/* ---- StyleBoxFlat ---- */

StyleBoxFlatRes::StyleBoxFlatRes()
    : StyleBoxRes(RES_STYLEBOX_FLAT, "StyleBoxFlat"), bg_color(0.6f, 0.6f, 0.6f, 1.0f),
      border_color(0.8f, 0.8f, 0.8f, 1.0f), shadow_color(0, 0, 0, 0.6f), corner_detail(8), shadow_size(0),
      draw_center(true), border_blend(false), anti_aliased(true), aa_size(1.0f) {
    for (int i = 0; i < 4; ++i) {
        border_width[i] = 0;
        corner_radius[i] = 0;
        expand_margin[i] = 0;
    }
}

static Rect2 grow_individual(const Rect2& r, float l, float t, float rr, float b) {
    return Rect2(r.pos.x - l, r.pos.y - t, r.size.x + l + rr, r.size.y + t + b);
}

static Rect2 grow_side(const Rect2& r, int side, float amount) {
    switch (side) {
    case SIDE_LEFT: return grow_individual(r, amount, 0, 0, 0);
    case SIDE_TOP: return grow_individual(r, 0, amount, 0, 0);
    case SIDE_RIGHT: return grow_individual(r, 0, 0, amount, 0);
    default: return grow_individual(r, 0, 0, 0, amount);
    }
}

static void set_inner_corner_radius(const Rect2& style_rect, const Rect2& inner_rect, const float corner_radius[4],
                                    float* inner) {
    float bl = inner_rect.pos.x - style_rect.pos.x;
    float bt = inner_rect.pos.y - style_rect.pos.y;
    float br = style_rect.size.x - inner_rect.size.x - bl;
    float bb = style_rect.size.y - inner_rect.size.y - bt;
    inner[0] = maxf(corner_radius[0] - minf(bt, bl), 0);
    inner[1] = maxf(corner_radius[1] - minf(bt, br), 0);
    inner[2] = maxf(corner_radius[2] - minf(bb, br), 0);
    inner[3] = maxf(corner_radius[3] - minf(bb, bl), 0);
}

static Vec2 vmax0(const Vec2& v, float m) { return Vec2(maxf(v.x, m), maxf(v.y, m)); }
static Vec2 vmin0(const Vec2& v, float m) { return Vec2(minf(v.x, m), minf(v.y, m)); }

enum { CORNER_TOP_LEFT, CORNER_TOP_RIGHT, CORNER_BOTTOM_RIGHT, CORNER_BOTTOM_LEFT };

static void set_corner_scale(const Rect2& style_rect, const Rect2& inner_rect, const float corner_radius[4],
                             Vec2* inner_scale) {
    float bl = inner_rect.pos.x - style_rect.pos.x;
    float bt = inner_rect.pos.y - style_rect.pos.y;
    float br = style_rect.size.x - inner_rect.size.x - bl;
    float bb = style_rect.size.y - inner_rect.size.y - bt;
    float edge_overflow[4] = {
        -minf(0, inner_rect.size.y - corner_radius[CORNER_TOP_LEFT] - corner_radius[CORNER_BOTTOM_LEFT]),
        -minf(0, inner_rect.size.x - corner_radius[CORNER_TOP_LEFT] - corner_radius[CORNER_TOP_RIGHT]),
        -minf(0, inner_rect.size.y - corner_radius[CORNER_TOP_RIGHT] - corner_radius[CORNER_BOTTOM_RIGHT]),
        -minf(0, inner_rect.size.x - corner_radius[CORNER_BOTTOM_LEFT] - corner_radius[CORNER_BOTTOM_RIGHT])
    };
    float hb = bl + br, vb = bt + bb;
    float ratios[4] = { hb > 0 ? bl / hb : 0, vb > 0 ? bt / vb : 0, hb > 0 ? br / hb : 0, vb > 0 ? bb / vb : 0 };
    Vec2 red[4] = {
        Vec2(edge_overflow[SIDE_TOP] * ratios[SIDE_LEFT], edge_overflow[SIDE_LEFT] * ratios[SIDE_TOP]),
        Vec2(edge_overflow[SIDE_TOP] * ratios[SIDE_RIGHT], edge_overflow[SIDE_RIGHT] * ratios[SIDE_TOP]),
        Vec2(edge_overflow[SIDE_BOTTOM] * ratios[SIDE_RIGHT], edge_overflow[SIDE_RIGHT] * ratios[SIDE_BOTTOM]),
        Vec2(edge_overflow[SIDE_BOTTOM] * ratios[SIDE_LEFT], edge_overflow[SIDE_LEFT] * ratios[SIDE_BOTTOM])
    };
    Vec2 pcr[4], left[4], dist[4];
    for (int i = 0; i < 4; ++i) pcr[i] = Vec2(corner_radius[i], corner_radius[i]);
    for (int i = 0; i < 4; ++i) left[i] = vmin0(pcr[i] - red[i], 0) * -1.0f;
    dist[0] = vmax0(pcr[0] - red[0] - left[3] - left[1], 0);
    dist[1] = vmax0(pcr[1] - red[1] - left[0] - left[2], 0);
    dist[2] = vmax0(pcr[2] - red[2] - left[1] - left[3], 0);
    dist[3] = vmax0(pcr[3] - red[3] - left[2] - left[0], 0);
    for (int i = 0; i < 4; ++i) {
        Vec2 unshrinkable = vmax0(left[(i + 1) % 4] + left[(i + 3) % 4] - dist[i], 0);
        Vec2 d = vmax0(pcr[i] - unshrinkable, FLT_EPSILON);
        inner_scale[i] = Vec2(dist[i].x / d.x, dist[i].y / d.y);
    }
}

static void draw_rounded_rectangle(std::vector<Vec2>& verts, std::vector<int>& indices, std::vector<Color>& colors,
                                   const Rect2& style_rect, const float corner_radius[4], const Rect2& ring_rect,
                                   const Rect2& inner_rect, const Color& inner_color, const Color& outer_color,
                                   int corner_detail, const Vec2& skew, bool is_filled) {
    int vert_offset = (int)verts.size();
    bool any = corner_radius[0] > 0 || corner_radius[1] > 0 || corner_radius[2] > 0 || corner_radius[3] > 0;
    int detail = any ? corner_detail : 1;
    bool draw_border = !is_filled;

    float ring_r[4];
    set_inner_corner_radius(style_rect, ring_rect, corner_radius, ring_r);
    Vec2 ring_s[4];
    set_corner_scale(style_rect, ring_rect, ring_r, ring_s);
    Vec2 outer_points[4] = {
        ring_rect.pos + Vec2(ring_r[0], ring_r[0]) * ring_s[0],
        Vec2(ring_rect.pos.x + ring_rect.size.x - ring_r[1] * ring_s[1].x, ring_rect.pos.y + ring_r[1] * ring_s[1].y),
        ring_rect.pos + ring_rect.size - Vec2(ring_r[2], ring_r[2]) * ring_s[2],
        Vec2(ring_rect.pos.x + ring_r[3] * ring_s[3].x, ring_rect.pos.y + ring_rect.size.y - ring_r[3] * ring_s[3].y)
    };
    float inner_r[4];
    set_inner_corner_radius(style_rect, inner_rect, corner_radius, inner_r);
    Vec2 inner_s[4];
    set_corner_scale(style_rect, inner_rect, inner_r, inner_s);
    Vec2 inner_points[4] = {
        inner_rect.pos + Vec2(inner_r[0], inner_r[0]) * inner_s[0],
        Vec2(inner_rect.pos.x + inner_rect.size.x - inner_r[1] * inner_s[1].x, inner_rect.pos.y + inner_r[1] * inner_s[1].y),
        inner_rect.pos + inner_rect.size - Vec2(inner_r[2], inner_r[2]) * inner_s[2],
        Vec2(inner_rect.pos.x + inner_r[3] * inner_s[3].x, inner_rect.pos.y + inner_rect.size.y - inner_r[3] * inner_s[3].y)
    };
    Vec2 center = style_rect.pos + style_rect.size * 0.5f;
    int n = (detail + 1) * (draw_border ? 8 : 4);
    size_t base = verts.size();
    verts.resize(base + n);
    colors.resize(base + n);
    for (int ci = 0; ci < 4; ++ci) {
        for (int d = 0; d <= detail; ++d) {
            int idx = (detail + 1) * ci + d;
            if (draw_border) idx *= 2;
            double ang = (ci + d / (double)detail) * (PI_F / 2.0) + PI_F;
            float co = (float)cos(ang), si = (float)sin(ang);
            {
                float x = inner_r[ci] * co * inner_s[ci].x + inner_points[ci].x;
                float y = inner_r[ci] * si * inner_s[ci].y + inner_points[ci].y;
                verts[base + idx] = Vec2(x - skew.x * (y - center.y), y - skew.y * (x - center.x));
                colors[base + idx] = inner_color;
            }
            if (draw_border) {
                float x = ring_r[ci] * co * ring_s[ci].x + outer_points[ci].x;
                float y = ring_r[ci] * si * ring_s[ci].y + outer_points[ci].y;
                verts[base + idx + 1] = Vec2(x - skew.x * (y - center.y), y - skew.y * (x - center.x));
                colors[base + idx + 1] = outer_color;
            }
        }
    }
    int ring_count = (int)verts.size() - vert_offset;
    if (draw_border) {
        for (int i = 0; i < ring_count; ++i) {
            indices.push_back(vert_offset + i % ring_count);
            indices.push_back(vert_offset + (i + 2) % ring_count);
            indices.push_back(vert_offset + (i + 1) % ring_count);
        }
    }
    if (is_filled) {
        int stripes = ring_count / 2 - 1;
        int last = ring_count - 1;
        for (int i = 0; i < stripes; ++i) {
            indices.push_back(vert_offset + i);
            indices.push_back(vert_offset + last - i - 1);
            indices.push_back(vert_offset + i + 1);
            indices.push_back(vert_offset + i);
            indices.push_back(vert_offset + last - i);
            indices.push_back(vert_offset + last - i - 1);
        }
    }
}

static void adapt_values(int a, int b, float* adapted, const float* values, float width, float max_a, float max_b) {
    float va = values[a], vb = values[b];
    float factor = minf(1.0f, width / (va + vb));
    adapted[a] = minf(minf(va * factor, max_a), adapted[a]);
    adapted[b] = minf(minf(vb * factor, max_b), adapted[b]);
}

void StyleBoxFlatRes::draw(Canvas& canvas, const Rect2& p_rect) const {
    bool has_border = border_width[0] > 0 || border_width[1] > 0 || border_width[2] > 0 || border_width[3] > 0;
    bool has_shadow = shadow_size > 0;
    if (!has_border && !draw_center && !has_shadow) return;
    Rect2 style_rect = grow_individual(p_rect, expand_margin[0], expand_margin[1], expand_margin[2], expand_margin[3]);
    if (fabsf(style_rect.size.x) < 1e-5f || fabsf(style_rect.size.y) < 1e-5f) return;
    bool rounded = corner_radius[0] > 0 || corner_radius[1] > 0 || corner_radius[2] > 0 || corner_radius[3] > 0;
    bool aa_on = (rounded || fabsf(skew.x) > 1e-5f || fabsf(skew.y) > 1e-5f) && anti_aliased;
    bool blend_on = border_blend && has_border;

    Color border_alpha(border_color.r, border_color.g, border_color.b, 0);
    Color border_blend_c = draw_center ? bg_color : border_alpha;
    Color border_inner = blend_on ? border_blend_c : border_color;

    float width = maxf(style_rect.size.x, 0), height = maxf(style_rect.size.y, 0);
    float bw[4] = { (float)border_width[0], (float)border_width[1], (float)border_width[2], (float)border_width[3] };
    float ab[4] = { 1000000.0f, 1000000.0f, 1000000.0f, 1000000.0f };
    adapt_values(SIDE_TOP, SIDE_BOTTOM, ab, bw, height, height, height);
    adapt_values(SIDE_LEFT, SIDE_RIGHT, ab, bw, width, width, width);
    float ac[4] = { 1000000.0f, 1000000.0f, 1000000.0f, 1000000.0f };
    adapt_values(CORNER_TOP_RIGHT, CORNER_BOTTOM_RIGHT, ac, corner_radius, height, height - ab[SIDE_BOTTOM], height - ab[SIDE_TOP]);
    adapt_values(CORNER_TOP_LEFT, CORNER_BOTTOM_LEFT, ac, corner_radius, height, height - ab[SIDE_BOTTOM], height - ab[SIDE_TOP]);
    adapt_values(CORNER_TOP_LEFT, CORNER_TOP_RIGHT, ac, corner_radius, width, width - ab[SIDE_RIGHT], width - ab[SIDE_LEFT]);
    adapt_values(CORNER_BOTTOM_LEFT, CORNER_BOTTOM_RIGHT, ac, corner_radius, width, width - ab[SIDE_RIGHT], width - ab[SIDE_LEFT]);

    Rect2 infill = grow_individual(style_rect, -ab[SIDE_LEFT], -ab[SIDE_TOP], -ab[SIDE_RIGHT], -ab[SIDE_BOTTOM]);
    Rect2 border_style_rect = style_rect;
    /* Godot divides the feather by the stretch scale (canvas_items stretch);
     * our layout runs in the game's units, so the same applies: the canvas
     * transform's scale. */
    float aa = 1.0f;
    if (aa_on) {
        const Xform2D& xf = canvas.xform();
        float sx = sqrtf(xf.x.x * xf.x.x + xf.x.y * xf.x.y), sy = sqrtf(xf.y.x * xf.y.x + xf.y.y * xf.y.y);
        float s = minf(sx, sy);
        aa = aa_size / (s > 0 ? s : 1.0f);
        for (int i = 0; i < 4; ++i)
            if (border_width[i] > 0) border_style_rect = grow_side(border_style_rect, i, -aa);
    }

    std::vector<Vec2> verts;
    std::vector<int> idx;
    std::vector<Color> colors;
    if (has_shadow) {
        Rect2 sh_inner = style_rect;
        sh_inner.pos = sh_inner.pos + shadow_offset;
        Rect2 sh = grow_individual(style_rect, (float)shadow_size, (float)shadow_size, (float)shadow_size, (float)shadow_size);
        sh.pos = sh.pos + shadow_offset;
        Color sh_t(shadow_color.r, shadow_color.g, shadow_color.b, 0);
        draw_rounded_rectangle(verts, idx, colors, sh_inner, ac, sh, sh_inner, shadow_color, sh_t, corner_detail, skew, false);
        if (draw_center)
            draw_rounded_rectangle(verts, idx, colors, sh_inner, ac, sh_inner, sh_inner, shadow_color, shadow_color,
                                   corner_detail, skew, true);
    }
    if (has_border && !aa_on)
        draw_rounded_rectangle(verts, idx, colors, border_style_rect, ac, border_style_rect, infill, border_inner,
                               border_color, corner_detail, skew, false);
    if (draw_center && (!aa_on || blend_on))
        draw_rounded_rectangle(verts, idx, colors, border_style_rect, ac, infill, infill, bg_color, bg_color,
                               corner_detail, skew, true);
    if (aa_on) {
        float abw[4], abwh[4], afw[4], afwh[4];
        for (int i = 0; i < 4; ++i) {
            bool b = has_border && border_width[i] > 0;
            abw[i] = b ? aa : 0;
            abwh[i] = b ? aa * 0.5f : 0;
            afw[i] = b ? 0 : aa;
            afwh[i] = b ? 0 : aa * 0.5f;
        }
        if (draw_center) {
            Rect2 t = grow_individual(infill, afwh[0], afwh[1], afwh[2], afwh[3]);
            Rect2 c = grow_individual(t, -afw[0], -afw[1], -afw[2], -afw[3]);
            if (!blend_on)
                draw_rounded_rectangle(verts, idx, colors, border_style_rect, ac, c, c, bg_color, bg_color,
                                       corner_detail, skew, true);
            if (!blend_on || !has_border) {
                Color alpha_bg(bg_color.r, bg_color.g, bg_color.b, 0);
                draw_rounded_rectangle(verts, idx, colors, border_style_rect, ac, t, c, bg_color, alpha_bg,
                                       corner_detail, skew, false);
            }
        }
        if (has_border) {
            Rect2 inner_c = grow_individual(infill, abwh[0], abwh[1], abwh[2], abwh[3]);
            Rect2 inner_t = grow_individual(inner_c, -abw[0], -abw[1], -abw[2], -abw[3]);
            Rect2 outer_t = grow_individual(style_rect, abwh[0], abwh[1], abwh[2], abwh[3]);
            Rect2 outer_c = grow_individual(border_style_rect, abwh[0], abwh[1], abwh[2], abwh[3]);
            draw_rounded_rectangle(verts, idx, colors, border_style_rect, ac, outer_c, blend_on ? infill : inner_c,
                                   border_inner, border_color, corner_detail, skew, false);
            if (!blend_on)
                draw_rounded_rectangle(verts, idx, colors, border_style_rect, ac, inner_c, inner_t, border_blend_c,
                                       border_color, corner_detail, skew, false);
            draw_rounded_rectangle(verts, idx, colors, border_style_rect, ac, outer_t, outer_c, border_color,
                                   border_alpha, corner_detail, skew, false);
        }
    }
    canvas.draw_colored_triangles(verts, idx, colors);
}

/* ---- StyleBoxLine ---- */

void StyleBoxLineRes::draw(Canvas& canvas, const Rect2& p) const {
    /* Rect2i r = p_rect */
    float x = (float)(int)p.pos.x, y = (float)(int)p.pos.y, w = (float)(int)p.size.x, h = (float)(int)p.size.y;
    if (vertical) {
        y -= (int)grow_begin;
        h += (int)(grow_begin + grow_end);
        w = (float)thickness;
    } else {
        x -= (int)grow_begin;
        w += (int)(grow_begin + grow_end);
        h = (float)thickness;
    }
    canvas.draw_rect(Rect2(x, y, w, h), color);
}

/* ---- StyleBoxTexture ---- */

StyleBoxTextureRes::StyleBoxTextureRes()
    : StyleBoxRes(RES_STYLEBOX_TEXTURE, "StyleBoxTexture"), texture(NULL), axis_h(0), axis_v(0), draw_center(true) {
    for (int i = 0; i < 4; ++i) {
        texture_margin[i] = 0;
        expand_margin[i] = 0;
    }
}

void StyleBoxTextureRes::draw(Canvas& canvas, const Rect2& p_rect) const {
    if (!texture) return;
    Rect2 rect = grow_individual(p_rect, expand_margin[0], expand_margin[1], expand_margin[2], expand_margin[3]);
    canvas.draw_nine_patch(texture, rect, region_rect, texture_margin, axis_h, axis_v, draw_center, modulate);
}

} /* namespace spire */

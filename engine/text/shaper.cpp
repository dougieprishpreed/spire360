#include "shaper.h"

#include "../scene/canvas.h"
#include "../scene/resource.h"

#include <math.h>

namespace spire {

void utf8_decode(const std::string& s, std::vector<u32>& out) {
    out.clear();
    out.reserve(s.size());
    const unsigned char* p = (const unsigned char*)s.c_str();
    const unsigned char* e = p + s.size();
    while (p < e) {
        u32 c = *p++;
        int extra = 0;
        if (c >= 0xF0) {
            c &= 0x07;
            extra = 3;
        } else if (c >= 0xE0) {
            c &= 0x0F;
            extra = 2;
        } else if (c >= 0xC0) {
            c &= 0x1F;
            extra = 1;
        } else if (c >= 0x80) {
            c = 0xFFFD;
        }
        for (; extra > 0 && p < e; --extra) c = (c << 6) | (*p++ & 0x3F);
        out.push_back(c);
    }
}

/* Godot's is_whitespace / is_linebreak (servers/text_server.h). */
bool is_whitespace_cp(u32 c) {
    return c == 0x0020 || c == 0x00A0 || c == 0x1680 || (c >= 0x2000 && c <= 0x200A) || c == 0x202F || c == 0x205F ||
           c == 0x3000 || c == 0x2028 || c == 0x2029 || (c >= 0x0009 && c <= 0x000D) || c == 0x0085;
}

bool is_linebreak_cp(u32 c) { return (c >= 0x000A && c <= 0x000D) || c == 0x0085 || c == 0x2028 || c == 0x2029; }

static bool is_cjk(u32 c) {
    return (c >= 0x2E80 && c <= 0x9FFF) || (c >= 0xAC00 && c <= 0xD7AF) || (c >= 0xF900 && c <= 0xFAFF) ||
           (c >= 0xFF00 && c <= 0xFFEF) || (c >= 0x20000 && c <= 0x2FFFF);
}

static bool is_alnum_cp(u32 c) {
    return (c >= '0' && c <= '9') || (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') ||
           (c >= 0xC0 && c <= 0x24F && c != 0xD7 && c != 0xF7) || (c >= 0x370 && c <= 0x52F);
}

static FontFace* face_of(const FontRes* f) {
    FontLibrary* lib = FontLibrary::current();
    return lib ? lib->face_for(f) : NULL;
}

float font_ascent(const FontRes* font, int size) {
    FontFace* f = face_of(font);
    float a = f ? f->ascent * size / f->base_size : 0.0f;
    return a + (font ? font->spacing_top : 0);
}

float font_descent(const FontRes* font, int size) {
    FontFace* f = face_of(font);
    float d = f ? f->descent * size / f->base_size : 0.0f;
    return d + (font ? font->spacing_bottom : 0);
}

float font_height(const FontRes* font, int size) { return font_ascent(font, size) + font_descent(font, size); }

static bool zero_width(u32 c) { return (c >= 0x200B && c <= 0x200D) || c == 0x2060 || c == 0xFEFF; }

void shape_run(const std::vector<u32>& text, int start, int end, const FontRes* font, int size, bool last_run,
               ShapedText& out) {
    FontFace* face = face_of(font);
    if (out.glyphs.empty()) out.start = start;
    out.end = end;
    float sp_gl = font ? (float)font->spacing_glyph : 0.0f;
    float sp_sp = font ? (float)font->spacing_space : 0.0f;
    float ea = 0;
    if (face) {
        float scale = size / face->base_size;
        /* _get_extra_advance: embolden * (msdf_source_size * 64) / 4096 */
        if (font && font->embolden != 0) ea = font->embolden * face->base_size * 64.0f / 4096.0f;
        out.ascent = maxf(out.ascent, face->ascent * scale + (font ? font->spacing_top : 0));
        out.descent = maxf(out.descent, face->descent * scale + (font ? font->spacing_bottom : 0));
        (void)scale;
    }
    double adv_rem = 0.0;
    for (int i = start; i < end; ++i) {
        u32 c = text[i];
        ShapedGlyph g;
        g.face = face;
        g.cp = c;
        g.font_size = size;
        g.x_off = g.y_off = 0;
        g.start = i;
        g.end = i + 1;
        g.count = 1;
        g.flags = 0;
        g.gid = 0;
        g.advance = 0;
        g.object = -1;
        g.asc = g.desc = 0;
        if (face) {
            g.asc = face->ascent * size / face->base_size + (font ? font->spacing_top : 0);
            g.desc = face->descent * size / face->base_size + (font ? font->spacing_bottom : 0);
        }
        if (face && !zero_width(c)) {
            float scale = size / face->base_size;
            bool subpos = scale != 1.0f; /* MSDF fonts scale; at base size Godot rounds */
            double base_adv;
            u32 lig = i + 1 < end ? face->ligature(c, text[i + 1]) : 0;
            if (lig) {
                const GlyphInfo* gi = face->glyph(lig);
                g.gid = lig;
                g.end = i + 2;
                base_adv = gi ? gi->advance : 0.0;
            } else {
                g.gid = face->glyph_for(c);
                base_adv = face->char_advance(c);
                if (i + 1 < end) base_adv += face->kerning(c, text[i + 1]);
            }
            if (c == 0x0009 || is_whitespace_cp(c) || is_linebreak_cp(c)) adv_rem = 0.0;
            if (is_linebreak_cp(c)) {
                g.gid = 0;
                base_adv = 0;
            }
            double a = base_adv * scale + ea;
            if (subpos) {
                g.advance = (float)a;
            } else {
                double full = adv_rem + a;
                g.advance = (float)floor(full + 0.5);
                adv_rem = full - g.advance;
            }
            if (lig) ++i;
        } else if (!face) {
            g.face = NULL;
        }
        bool is_last = g.end >= end;
        if (!last_run || !is_last) {
            if (sp_sp != 0 && is_whitespace_cp(c)) g.advance += sp_sp;
            else g.advance += sp_gl;
        }
        out.width += g.advance;
        out.glyphs.push_back(g);
    }
}

void shape_object(ShapedText& out, int pos, float width, int object) {
    if (out.glyphs.empty()) out.start = pos;
    out.end = pos + 1;
    ShapedGlyph g;
    g.face = NULL;
    g.gid = 0;
    g.cp = 0xFFFC;
    g.font_size = 0;
    g.advance = width;
    g.x_off = g.y_off = 0;
    g.asc = g.desc = 0;
    g.start = pos;
    g.end = pos + 1;
    g.count = 1;
    g.flags = GF_OBJECT;
    g.object = object;
    out.width += width;
    out.glyphs.push_back(g);
}

void shape_finish(const std::vector<u32>& text, ShapedText& out) {
    /* Break opportunities (a subset of ICU line breaking): position p means
     * "a line may start at p". */
    int n = out.end;
    std::vector<char> brk(n + 1, 0); /* 1 soft, 2 hard */
    for (int i = out.start; i < n; ++i) {
        u32 c = text[i];
        u32 nx = i + 1 < n ? text[i + 1] : 0;
        u32 pv = i > out.start ? text[i - 1] : 0;
        if (is_linebreak_cp(c)) {
            if (!(c == 0x0D && nx == 0x0A)) brk[i + 1] = 2;
        } else if ((c == 0x20 || c == 0x09 || c == 0x3000 || (c >= 0x2000 && c <= 0x200A && c != 0x2007)) &&
                   !(nx == 0x20 || nx == 0x09)) {
            if (i + 1 <= n) brk[i + 1] = 1;
        } else if ((c == 0x2D || c == 0x2010 || c == 0x2013) && is_alnum_cp(pv) && is_alnum_cp(nx) &&
                   !(nx >= '0' && nx <= '9')) {
            brk[i + 1] = 1;
        } else if (c == 0x2014 && nx) {
            brk[i + 1] = 1;
            if (i > out.start) brk[i] = brk[i] ? brk[i] : 1;
        } else if (is_cjk(c) && nx && i + 1 < n) {
            brk[i + 1] = 1;
        }
        if (nx == 0xFFFC && i + 1 < n) brk[i + 1] = brk[i + 1] ? brk[i + 1] : 1; /* LB20: break before objects */
    }
    std::vector<ShapedGlyph> res;
    res.reserve(out.glyphs.size() + 8);
    for (size_t i = 0; i < out.glyphs.size(); ++i) {
        ShapedGlyph g = out.glyphs[i];
        u32 c = text[g.start];
        if (g.flags & GF_OBJECT) {
            res.push_back(g);
            continue;
        }
        if (is_whitespace_cp(c)) g.flags |= GF_SPACE;
        bool virt = false;
        if (g.end <= n && brk[g.end]) {
            if (brk[g.end] == 2 && is_linebreak_cp(c)) g.flags |= GF_BREAK_HARD;
            else if (is_whitespace_cp(c) || c == 0x00AD) g.flags |= GF_BREAK_SOFT;
            else if (g.end != out.end) virt = true;
        }
        res.push_back(g);
        if (virt) {
            ShapedGlyph v = g;
            v.face = NULL;
            v.gid = 0;
            v.advance = 0;
            v.count = 1;
            v.flags = GF_BREAK_SOFT | GF_VIRTUAL | GF_SPACE;
            res.push_back(v);
        }
    }
    out.glyphs.swap(res);
}

/* TextServer::shaped_text_get_line_breaks (LTR, no indent trimming). */
void line_breaks(const ShapedText& t, float p_width, int flags, std::vector<int>& lines) {
    lines.clear();
    const std::vector<ShapedGlyph>& l_gl = t.glyphs;
    int l_size = (int)l_gl.size();
    double width = 0;
    int line_start = t.start, last_end = line_start;
    int prev_safe_break = 0, last_safe_break = -1, word_count = 0;
    bool trim_next = false;
    const u32 EDGE = GF_SPACE | GF_BREAK_HARD | GF_BREAK_SOFT;
    double l_width = p_width;
    bool trim_start = (flags & BREAK_TRIM_START_EDGE_SPACES) != 0, trim_end = (flags & BREAK_TRIM_END_EDGE_SPACES) != 0;
    for (int i = 0; i < l_size; ++i) {
        if (l_gl[i].count > 0) {
            float adv = 0;
            for (int j = i; j < l_size && l_gl[i].end == l_gl[j].end && l_gl[i].start == l_gl[j].start; ++j)
                adv += l_gl[j].advance;
            if (l_width > 0 && width + adv > l_width && last_safe_break >= 0) {
                int cur = last_safe_break;
                if (trim_start || trim_end) {
                    int sp = prev_safe_break, ep = last_safe_break;
                    while (trim_start && trim_next && sp < ep && (l_gl[sp].flags & EDGE)) sp += l_gl[sp].count;
                    while (trim_end && sp < ep && (l_gl[ep].flags & EDGE)) ep -= l_gl[ep].count;
                    if (last_end <= l_gl[sp].start) {
                        lines.push_back(l_gl[sp].start);
                        lines.push_back(l_gl[ep].end);
                        cur = last_safe_break;
                        last_end = l_gl[ep].end;
                    }
                    trim_next = true;
                } else if (last_end <= line_start) {
                    lines.push_back(line_start);
                    lines.push_back(l_gl[last_safe_break].end);
                    last_end = l_gl[last_safe_break].end;
                }
                line_start = l_gl[cur].end;
                prev_safe_break = cur + 1;
                while (prev_safe_break < l_size && l_gl[prev_safe_break].end == line_start) ++prev_safe_break;
                i = cur;
                last_safe_break = -1;
                width = 0;
                word_count = 0;
                continue;
            }
            if ((flags & BREAK_MANDATORY) && (l_gl[i].flags & GF_BREAK_HARD)) {
                int cur = i;
                if (trim_start || trim_end) {
                    int sp = prev_safe_break, ep = i;
                    while (trim_start && trim_next && sp < ep && (l_gl[sp].flags & EDGE)) sp += l_gl[sp].count;
                    while (trim_end && sp < ep && (l_gl[ep].flags & EDGE)) ep -= l_gl[ep].count;
                    trim_next = true;
                    if (last_end <= l_gl[sp].start) {
                        lines.push_back(l_gl[sp].start);
                        lines.push_back(l_gl[ep].end);
                        last_end = l_gl[i].end;
                        cur = i;
                    }
                } else if (last_end <= line_start) {
                    lines.push_back(line_start);
                    lines.push_back(l_gl[i].end);
                    last_end = l_gl[i].end;
                }
                line_start = l_gl[cur].end;
                prev_safe_break = cur + 1;
                while (prev_safe_break < l_size && l_gl[prev_safe_break].end == line_start) ++prev_safe_break;
                last_safe_break = -1;
                width = 0;
                continue;
            }
            if (flags & BREAK_WORD_BOUND) {
                if (l_gl[i].flags & GF_BREAK_SOFT) {
                    last_safe_break = i;
                    ++word_count;
                }
                if ((flags & BREAK_ADAPTIVE) && word_count == 0) last_safe_break = i;
            }
            if (flags & BREAK_GRAPHEME_BOUND) last_safe_break = i;
        }
        width += l_gl[i].advance;
    }
    if (l_size > 0) {
        if (lines.empty() || (lines.back() < t.end && prev_safe_break < l_size)) {
            if (trim_start) {
                int sp = prev_safe_break < l_size ? prev_safe_break : l_size - 1;
                if (last_end <= l_gl[sp].start) {
                    int ep = l_size - 1;
                    while (trim_next && sp < ep && (l_gl[sp].flags & EDGE)) sp += l_gl[sp].count;
                    lines.push_back(l_gl[sp].start);
                } else {
                    lines.push_back(last_end);
                }
            } else {
                lines.push_back(last_end > line_start ? last_end : line_start);
            }
            lines.push_back(t.end);
        }
    } else {
        lines.push_back(0);
        lines.push_back(0);
    }
}

void shaped_substr(const ShapedText& t, int start, int end, ShapedText& out) {
    out.clear();
    out.start = start;
    out.end = end;
    bool any = false;
    for (size_t i = 0; i < t.glyphs.size(); ++i) {
        const ShapedGlyph& g = t.glyphs[i];
        if (g.start >= start && g.end <= end) {
            out.glyphs.push_back(g);
            out.width += g.advance;
            if (g.face) {
                out.ascent = any ? maxf(out.ascent, g.asc) : g.asc;
                out.descent = any ? maxf(out.descent, g.desc) : g.desc;
                any = true;
            }
        }
    }
    if (!any) {
        out.ascent = t.ascent;
        out.descent = t.descent;
    }
}

void draw_glyph(Canvas& c, const ShapedGlyph& g, const Vec2& pos, const Color& color, int outline_size) {
    if (!g.face || !g.gid) return;
    const GlyphInfo* gi = g.face->glyph(g.gid);
    if (!gi || gi->page < 0 || gi->page >= (int)g.face->pages.size()) return;
    float s = g.font_size / g.face->base_size;
    Vec2 p(pos.x + g.x_off + gi->offset.x * s, pos.y + g.y_off + gi->offset.y * s);
    Rect2 dst(p, gi->size * s);
    c.draw_msdf(g.face->pages[gi->page], dst, gi->uv, color, g.face->px_range,
                outline_size > 0 ? (float)outline_size / s / 4.0f : 0.0f);
}

} /* namespace spire */

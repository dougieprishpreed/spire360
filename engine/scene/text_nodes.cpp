#include "text_nodes.h"

#include "canvas.h"
#include "value.h"

#include <math.h>

namespace spire {

/* ---- Label ---- */

Label::Label()
    : h_align(HALIGN_LEFT), v_align(VALIGN_TOP), autowrap(AUTOWRAP_OFF),
      trim_flags(BREAK_TRIM_START_EDGE_SPACES | BREAK_TRIM_END_EDGE_SPACES), overrun(0), clip_text(false),
      uppercase(false), visible_characters(-1), max_lines_visible(-1), lines_skipped(0), visible_ratio(1.0f),
      font_(NULL), font_size_(16) {}

bool Label::set(const std::string& key, const Json& v, const PropCtx& ctx) {
    if (key == "text") text = v.str();
    else if (key == "horizontal_alignment") h_align = v.i();
    else if (key == "vertical_alignment") v_align = v.i();
    else if (key == "autowrap_mode") autowrap = v.i();
    else if (key == "autowrap_trim_flags") trim_flags = v.i();
    else if (key == "text_overrun_behavior") overrun = v.i();
    else if (key == "clip_text") clip_text = v.b();
    else if (key == "uppercase") uppercase = v.b();
    else if (key == "visible_characters") visible_characters = v.i(-1);
    else if (key == "visible_ratio") visible_ratio = jv_float(v, 1.0f);
    else if (key == "max_lines_visible") max_lines_visible = v.i(-1);
    else if (key == "lines_skipped") lines_skipped = v.i();
    else return Control::set(key, v, ctx);
    return true;
}

static void to_upper(std::vector<u32>& cps) {
    for (size_t i = 0; i < cps.size(); ++i) {
        u32 c = cps[i];
        if ((c >= 'a' && c <= 'z') || (c >= 0xE0 && c <= 0xFE && c != 0xF7)) cps[i] = c - 32;
        else if (c >= 0x430 && c <= 0x44F) cps[i] = c - 32;
        else if (c >= 0x450 && c <= 0x45F) cps[i] = c - 80;
        else if (c >= 0x3B1 && c <= 0x3C9 && c != 0x3C2) cps[i] = c - 32;
        else if (c >= 0x100 && c <= 0x17F && (c & 1) && c != 0x131 && c != 0x138 && c != 0x149 && c != 0x17F)
            cps[i] = c - 1;
    }
}

int Label::line_count() {
    shape();
    int n = 0;
    for (size_t i = 0; i < paras_.size(); ++i) n += (int)paras_[i].lines.size();
    return n;
}

/* Label::_shape (+ _update_visible for the minimum height). */
void Label::shape() {
    font_ = theme_font("font");
    font_size_ = theme_font_size("font_size");
    StyleBoxRes* style = theme_stylebox("normal");
    Vec2 smin = style ? style->minimum_size() : Vec2();
    float width = (float)(int)(size.x - smin.x);
    std::vector<u32> all;
    utf8_decode(text, all);
    if (uppercase) to_upper(all);
    if (visible_characters >= 0 && visible_characters < (int)all.size()) all.resize(visible_characters);
    else if (visible_ratio < 1.0f) all.resize((size_t)(all.size() * maxf(visible_ratio, 0.0f)));
    paras_.clear();
    size_t s0 = 0;
    for (size_t i = 0; i <= all.size(); ++i) {
        if (i == all.size() || all[i] == '\n') {
            Para p;
            p.cps.assign(all.begin() + s0, all.begin() + i);
            p.cps.push_back(0x200B);
            paras_.push_back(p);
            s0 = i + 1;
        }
    }
    int flags = BREAK_MANDATORY;
    if (autowrap == AUTOWRAP_WORD_SMART) flags = BREAK_WORD_BOUND | BREAK_ADAPTIVE | BREAK_MANDATORY;
    else if (autowrap == AUTOWRAP_WORD) flags = BREAK_WORD_BOUND | BREAK_MANDATORY;
    else if (autowrap == AUTOWRAP_ARBITRARY) flags = BREAK_GRAPHEME_BOUND | BREAK_MANDATORY;
    flags |= trim_flags;
    float font_h = font_height(font_, font_size_);
    if (autowrap == AUTOWRAP_OFF) minsize_.x = 0;
    std::vector<int> br;
    for (size_t p = 0; p < paras_.size(); ++p) {
        Para& pa = paras_[p];
        pa.shaped.clear();
        shape_run(pa.cps, 0, (int)pa.cps.size(), font_, font_size_, true, pa.shaped);
        shape_finish(pa.cps, pa.shaped);
        line_breaks(pa.shaped, autowrap == AUTOWRAP_OFF ? 0.0f : width, flags, br);
        pa.lines.clear();
        for (size_t i = 0; i + 1 < br.size(); i += 2) {
            ShapedText l;
            shaped_substr(pa.shaped, br[i], br[i + 1], l);
            pa.lines.push_back(l);
            if (autowrap == AUTOWRAP_OFF && l.width > minsize_.x) minsize_.x = l.width;
        }
    }
    if (text.empty()) {
        minsize_ = Vec2(1, font_h);
        return;
    }
    int line_spacing = theme_constant("line_spacing"), para_spacing = theme_constant("paragraph_spacing");
    int total = 0;
    for (size_t p = 0; p < paras_.size(); ++p) total += (int)paras_[p].lines.size();
    int visible = total;
    if (max_lines_visible >= 0 && visible > max_lines_visible) visible = max_lines_visible;
    int last_line = total < visible + lines_skipped ? total : visible + lines_skipped;
    float h = 0;
    int idx = 0;
    for (size_t p = 0; p < paras_.size(); ++p) {
        int n = (int)paras_[p].lines.size();
        if (idx + n <= lines_skipped) {
            idx += n;
            continue;
        }
        int st = idx < lines_skipped ? lines_skipped - idx : 0;
        int en = idx + n < last_line ? n : last_line - idx;
        if (en <= 0) break;
        for (int i = st; i < en; ++i) h += line_height(paras_[p].lines[i], font_h) + line_spacing;
        h += para_spacing;
        idx += n;
    }
    if (h > 0) h -= line_spacing + para_spacing;
    minsize_.y = h;
}

float Label::line_height(const ShapedText& l, float font_h) const {
    double asc = l.ascent, dsc = l.descent;
    if (asc + dsc < font_h) {
        double diff = font_h - (asc + dsc);
        asc += diff / 2;
        dsc += diff - diff / 2;
    }
    return (float)(asc + dsc);
}

Vec2 Label::get_minimum_size() {
    shape();
    Vec2 m = minsize_;
    m.y = maxf(m.y, font_height(font_, font_size_));
    StyleBoxRes* style = theme_stylebox("normal");
    Vec2 smin = style ? style->minimum_size() : Vec2();
    bool clip = clip_text || overrun != 0;
    if (autowrap != AUTOWRAP_OFF) return Vec2(1, clip ? 1 : m.y) + smin;
    if (clip) m.x = 1;
    return m + smin;
}

void Label::draw(Canvas& c) {
    shape();
    StyleBoxRes* style = theme_stylebox("normal");
    if (style) style->draw(c, Rect2(Vec2(0, 0), size));
    float font_h = font_height(font_, font_size_);
    Color font_color = theme_color("font_color");
    Color shadow_color = theme_color("font_shadow_color");
    Color outline_color = theme_color("font_outline_color");
    Vec2 shadow_ofs((float)theme_constant("shadow_offset_x"), (float)theme_constant("shadow_offset_y"));
    int outline_size = theme_constant("outline_size");
    int shadow_outline = theme_constant("shadow_outline_size");
    int line_spacing = theme_constant("line_spacing"), para_spacing = theme_constant("paragraph_spacing");
    Vec2 smin = style ? style->minimum_size() : Vec2();
    Vec2 soff = style ? style->offset() : Vec2();
    float smargin_r = style ? style->margin(SIDE_RIGHT) : 0;

    /* get_layout_data */
    int total = 0;
    for (size_t p = 0; p < paras_.size(); ++p) total += (int)paras_[p].lines.size();
    float total_h = 0;
    int lines_visible = 0, idx = 0;
    for (size_t p = 0; p < paras_.size(); ++p) {
        int n = (int)paras_[p].lines.size();
        if (idx + n <= lines_skipped) {
            idx += n;
            continue;
        }
        int st = idx < lines_skipped ? lines_skipped - idx : 0;
        bool stop = false;
        for (int i = st; i < n; ++i) {
            total_h += line_height(paras_[p].lines[i], font_h) + line_spacing;
            if (total_h > ceilf(size.y - smin.y + line_spacing)) {
                stop = true;
                break;
            }
            ++lines_visible;
        }
        total_h += para_spacing;
        idx += n;
        if (stop) break;
    }
    if (max_lines_visible >= 0 && lines_visible > max_lines_visible) lines_visible = max_lines_visible;
    int last_line = total < lines_visible + lines_skipped ? total : lines_visible + lines_skipped;
    total_h = 0;
    idx = 0;
    for (size_t p = 0; p < paras_.size(); ++p) {
        int n = (int)paras_[p].lines.size();
        if (idx + n <= lines_skipped) {
            idx += n;
            continue;
        }
        int st = idx < lines_skipped ? lines_skipped - idx : 0;
        int en = idx + n < last_line ? n : last_line - idx;
        if (en <= 0) break;
        for (int i = st; i < en; ++i) total_h += line_height(paras_[p].lines[i], font_h) + line_spacing;
        total_h += para_spacing;
        idx += n;
    }
    total_h += smin.y;
    int vbegin = 0, vsep = 0;
    if (lines_visible > 0) {
        if (v_align == VALIGN_CENTER) vbegin = (int)((size.y - (total_h - line_spacing - para_spacing)) / 2);
        else if (v_align == VALIGN_BOTTOM) vbegin = (int)(size.y - (total_h - line_spacing - para_spacing));
        else if (v_align == VALIGN_FILL && lines_visible > 1)
            vsep = (int)((size.y - (total_h - line_spacing - para_spacing)) / (lines_visible - 1));
    }
    Vec2 ofs(0, soff.y + vbegin);
    int lsp = line_spacing + vsep;

    idx = 0;
    for (size_t p = 0; p < paras_.size(); ++p) {
        int n = (int)paras_[p].lines.size();
        if (idx + n <= lines_skipped) {
            idx += n;
            continue;
        }
        int st = idx < lines_skipped ? lines_skipped - idx : 0;
        int en = idx + n < last_line ? n : last_line - idx;
        if (en <= 0) break;
        for (int i = st; i < en; ++i) {
            const ShapedText& l = paras_[p].lines[i];
            /* _get_line_rect */
            float lx;
            if (h_align == HALIGN_CENTER) lx = (float)((int)(size.x - l.width) / 2);
            else if (h_align == HALIGN_RIGHT) lx = (float)(int)(size.x - smargin_r - l.width);
            else lx = soff.x;
            double asc = l.ascent, dsc = l.descent;
            if (asc + dsc < font_h) {
                double diff = font_h - (asc + dsc);
                asc += diff / 2;
                dsc += diff - diff / 2;
            }
            ofs.y += (float)asc;
            for (int pass = 0; pass < 4; ++pass) {
                Color col;
                int outline = 0;
                Vec2 off;
                if (pass == 0) {
                    if (!(shadow_color.a != 0 && shadow_outline > 0)) continue;
                    col = shadow_color;
                    outline = shadow_outline;
                    off = shadow_ofs;
                } else if (pass == 1) {
                    if (!(shadow_color.a > 0)) continue;
                    col = shadow_color;
                    off = shadow_ofs;
                } else if (pass == 2) {
                    if (!(outline_size > 0 && outline_color.a != 0)) continue;
                    col = outline_color;
                    outline = outline_size;
                } else {
                    col = font_color;
                }
                float x = lx;
                for (size_t g = 0; g < l.glyphs.size(); ++g) {
                    draw_glyph(c, l.glyphs[g], Vec2(x, ofs.y) + off, col, outline);
                    x += l.glyphs[g].advance;
                }
            }
            ofs.y += (float)dsc + lsp;
        }
        ofs.y += para_spacing;
        idx += n;
    }
}

/* ---- RichTextLabel ---- */

bool RichTextLabel::set(const std::string& key, const Json& v, const PropCtx& ctx) {
    if (key == "text") text = v.str();
    else if (key == "bbcode_enabled") bbcode = v.b();
    else if (key == "fit_content") fit_content = v.b();
    else if (key == "scroll_active") scroll_active = v.b(true);
    else if (key == "autowrap_mode") autowrap = v.i();
    else if (key == "horizontal_alignment") h_align = v.i();
    else if (key == "vertical_alignment") v_align = v.i();
    else return Control::set(key, v, ctx);
    return true;
}

} /* namespace spire */

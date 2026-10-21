#include "controls.h"

#include "canvas.h"
#include "shaders.h"
#include "value.h"
#include "viewport.h"

#include <math.h>
#include <string.h>

namespace spire {

static Texture2DRes* as_texture(Resource* r) { return r && r->is_texture() ? (Texture2DRes*)r : NULL; }

/* ---- TextureRect ---- */

bool TextureRect::set(const std::string& key, const Json& v, const PropCtx& ctx) {
    if (key == "texture") texture = bind_texture(this, ctx.resource(v));
    else if (key == "expand_mode") expand_mode = v.i();
    else if (key == "expand") expand_mode = v.b() ? EXPAND_IGNORE_SIZE : EXPAND_KEEP_SIZE; /* Godot 3 name */
    else if (key == "stretch_mode") stretch_mode = v.i();
    else if (key == "flip_h") flip_h = v.b();
    else if (key == "flip_v") flip_v = v.b();
    else return Control::set(key, v, ctx);
    return true;
}

Vec2 TextureRect::get_minimum_size() {
    if (!texture) return Vec2();
    Vec2 ts = texture->size();
    switch (expand_mode) {
    case EXPAND_KEEP_SIZE: return ts;
    case EXPAND_IGNORE_SIZE: return Vec2();
    case EXPAND_FIT_WIDTH: return Vec2(size.y, 0);
    case EXPAND_FIT_WIDTH_PROPORTIONAL: return Vec2(size.y * (ts.x / ts.y), 0);
    case EXPAND_FIT_HEIGHT: return Vec2(0, size.x);
    case EXPAND_FIT_HEIGHT_PROPORTIONAL: return Vec2(0, size.x * (ts.y / ts.x));
    }
    return Vec2();
}

void TextureRect::draw(Canvas& c) {
    if (!texture) return;
    Vec2 sz, ofs;
    Rect2 region;
    bool tile = false, has_region = false;
    Vec2 ts = texture->size();
    switch (stretch_mode) {
    case STRETCH_SCALE: sz = size; break;
    case STRETCH_TILE:
        sz = size;
        tile = true;
        break;
    case STRETCH_KEEP: sz = ts; break;
    case STRETCH_KEEP_CENTERED:
        ofs = (size - ts) * 0.5f;
        sz = ts;
        break;
    case STRETCH_KEEP_ASPECT_CENTERED:
    case STRETCH_KEEP_ASPECT: {
        sz = size;
        int tw = (int)(ts.x * sz.y / ts.y);
        int th = (int)sz.y;
        if (tw > sz.x) {
            tw = (int)sz.x;
            th = (int)(ts.y * tw / ts.x);
        }
        if (stretch_mode == STRETCH_KEEP_ASPECT_CENTERED) {
            ofs.x += (sz.x - tw) / 2;
            ofs.y += (sz.y - th) / 2;
        }
        sz = Vec2((float)tw, (float)th);
        break;
    }
    case STRETCH_KEEP_ASPECT_COVERED: {
        sz = size;
        Vec2 sc(sz.x / ts.x, sz.y / ts.y);
        float s = sc.x > sc.y ? sc.x : sc.y;
        Vec2 scaled = ts * s;
        Vec2 d = (scaled - sz) / s;
        region.pos = Vec2(fabsf(d.x) / 2.0f, fabsf(d.y) / 2.0f);
        region.size = sz / s;
        has_region = region.has_area();
        break;
    }
    }
    if (flip_h) sz.x = -sz.x;
    if (flip_v) sz.y = -sz.y;
    if (has_region) c.draw_texture_rect_region(texture, Rect2(ofs, sz), region);
    else c.draw_texture_rect(texture, Rect2(ofs, sz), tile);
}

/* ---- NinePatchRect ---- */

bool NinePatchRect::set(const std::string& key, const Json& v, const PropCtx& ctx) {
    if (key == "texture") texture = as_texture(ctx.resource(v));
    else if (key == "draw_center") draw_center = v.b(true);
    else if (key == "region_rect") jv_rect2(v, region_rect);
    else if (key == "patch_margin_left") margin[SIDE_LEFT] = jv_float(v);
    else if (key == "patch_margin_top") margin[SIDE_TOP] = jv_float(v);
    else if (key == "patch_margin_right") margin[SIDE_RIGHT] = jv_float(v);
    else if (key == "patch_margin_bottom") margin[SIDE_BOTTOM] = jv_float(v);
    else if (key == "axis_stretch_horizontal") axis_h = v.i();
    else if (key == "axis_stretch_vertical") axis_v = v.i();
    else return Control::set(key, v, ctx);
    return true;
}

void NinePatchRect::draw(Canvas& c) {
    if (!texture) return;
    c.draw_nine_patch(texture, Rect2(Vec2(0, 0), size), region_rect, margin, axis_h, axis_v, draw_center, Color());
}

/* ---- ColorRect, Panel ---- */

bool ColorRect::set(const std::string& key, const Json& v, const PropCtx& ctx) {
    if (key == "color") jv_color(v, color);
    else return Control::set(key, v, ctx);
    return true;
}

void ColorRect::draw(Canvas& c) { c.draw_rect(Rect2(Vec2(0, 0), size), color); }

void Panel::draw(Canvas& c) {
    StyleBoxRes* sb = theme_stylebox("panel");
    if (sb) sb->draw(c, Rect2(Vec2(0, 0), size));
}

/* ---- Container ---- */

Control* Container::sortable(Node* n) {
    if (!n->is_control()) return NULL;
    Control* c = (Control*)n;
    if (c->top_level || !c->visible) return NULL;
    return c;
}

void Container::fit_child_in_rect(Control* child, const Rect2& p_rect) {
    Vec2 m = child->combined_minimum_size();
    Rect2 r = p_rect;
    if (!(child->size_flags_h & SIZE_FILL)) {
        r.size.x = m.x;
        if (child->size_flags_h & SIZE_SHRINK_END) r.pos.x += p_rect.size.x - m.x;
        else if (child->size_flags_h & SIZE_SHRINK_CENTER) r.pos.x += floorf((p_rect.size.x - m.x) / 2);
    }
    if (!(child->size_flags_v & SIZE_FILL)) {
        r.size.y = m.y;
        if (child->size_flags_v & SIZE_SHRINK_END) r.pos.y += p_rect.size.y - m.y;
        else if (child->size_flags_v & SIZE_SHRINK_CENTER) r.pos.y += floorf((p_rect.size.y - m.y) / 2);
    }
    child->set_rect(r);
    child->rotation = 0;
    child->scale = Vec2(1, 1);
}

/* ---- BoxContainer ---- */

bool BoxContainer::set(const std::string& key, const Json& v, const PropCtx& ctx) {
    if (key == "alignment") alignment = v.i();
    else if (key == "vertical") vertical = v.b();
    else return Container::set(key, v, ctx);
    return true;
}

Vec2 BoxContainer::get_minimum_size() {
    int sep = theme_constant("separation");
    int mw = 0, mh = 0;
    bool first = true;
    for (size_t i = 0; i < children.size(); ++i) {
        Control* c = sortable(children[i]);
        if (!c) continue;
        Vec2 s = c->combined_minimum_size();
        int w = (int)s.x, h = (int)s.y;
        if (vertical) {
            if (w > mw) mw = w;
            mh += h + (first ? 0 : sep);
        } else {
            if (h > mh) mh = h;
            mw += w + (first ? 0 : sep);
        }
        first = false;
    }
    return Vec2((float)mw, (float)mh);
}

struct MinSizeCache {
    Control* c;
    int min_size;
    bool will_stretch;
    int final_size;
};

void BoxContainer::sort_children() {
    int sep = theme_constant("separation");
    int nw = (int)size.x, nh = (int)size.y;
    std::vector<MinSizeCache> cache;
    int stretch_min = 0, stretch_avail = 0;
    float ratio_total = 0;
    for (size_t i = 0; i < children.size(); ++i) {
        Control* c = sortable(children[i]);
        if (!c) continue;
        Vec2 s = c->combined_minimum_size();
        MinSizeCache m;
        m.c = c;
        m.min_size = vertical ? (int)s.y : (int)s.x;
        m.will_stretch = ((vertical ? c->size_flags_v : c->size_flags_h) & SIZE_EXPAND) != 0;
        stretch_min += m.min_size;
        if (m.will_stretch) {
            stretch_avail += m.min_size;
            ratio_total += c->stretch_ratio;
        }
        m.final_size = m.min_size;
        cache.push_back(m);
    }
    int count = (int)cache.size();
    if (!count) return;
    int stretch_max = (vertical ? nh : nw) - (count - 1) * sep;
    int stretch_diff = stretch_max - stretch_min;
    if (stretch_diff < 0) stretch_diff = 0;
    stretch_avail += stretch_diff;

    bool has_stretched = false;
    while (ratio_total > 0) {
        has_stretched = true;
        bool refit_ok = true;
        float error = 0;
        for (int i = 0; i < count; ++i) {
            MinSizeCache& m = cache[i];
            if (!m.will_stretch) continue;
            float final_px = stretch_avail * m.c->stretch_ratio / ratio_total;
            error += final_px - (int)final_px;
            if (final_px < m.min_size) {
                m.will_stretch = false;
                ratio_total -= m.c->stretch_ratio;
                refit_ok = false;
                stretch_avail -= m.min_size;
                m.final_size = m.min_size;
                break;
            }
            m.final_size = (int)final_px;
            if (error >= 1) {
                m.final_size += 1;
                error -= 1;
            }
        }
        if (refit_ok) break;
    }

    int ofs = 0;
    if (!has_stretched) {
        if (alignment == ALIGN_CENTER) ofs = stretch_diff / 2;
        else if (alignment == ALIGN_END) ofs = stretch_diff;
    }
    for (int i = 0; i < count; ++i) {
        MinSizeCache& m = cache[i];
        if (i > 0) ofs += sep;
        int from = ofs, to = ofs + m.final_size;
        if (m.will_stretch && i == count - 1) to = vertical ? nh : nw;
        int sz = to - from;
        Rect2 r = vertical ? Rect2(0, (float)from, (float)nw, (float)sz) : Rect2((float)from, 0, (float)sz, (float)nh);
        fit_child_in_rect(m.c, r);
        ofs = to;
    }
}

/* ---- MarginContainer ---- */

Vec2 MarginContainer::get_minimum_size() {
    Vec2 mx;
    for (size_t i = 0; i < children.size(); ++i) {
        Control* c = sortable(children[i]);
        if (!c) continue;
        Vec2 s = c->combined_minimum_size();
        mx = Vec2(maxf(mx.x, s.x), maxf(mx.y, s.y));
    }
    mx.x += theme_constant("margin_left") + theme_constant("margin_right");
    mx.y += theme_constant("margin_top") + theme_constant("margin_bottom");
    return mx;
}

void MarginContainer::sort_children() {
    int ml = theme_constant("margin_left"), mt = theme_constant("margin_top");
    int mr = theme_constant("margin_right"), mb = theme_constant("margin_bottom");
    for (size_t i = 0; i < children.size(); ++i) {
        Control* c = sortable(children[i]);
        if (!c) continue;
        int w = (int)(size.x - ml - mr), h = (int)(size.y - mt - mb);
        fit_child_in_rect(c, Rect2((float)ml, (float)mt, (float)w, (float)h));
    }
}

/* ---- PanelContainer ---- */

Vec2 PanelContainer::get_minimum_size() {
    Vec2 ms;
    for (size_t i = 0; i < children.size(); ++i) {
        Control* c = sortable(children[i]);
        if (!c) continue;
        Vec2 s = c->combined_minimum_size();
        ms = Vec2(maxf(ms.x, s.x), maxf(ms.y, s.y));
    }
    StyleBoxRes* sb = theme_stylebox("panel");
    if (sb) ms = ms + sb->minimum_size();
    return ms;
}

void PanelContainer::sort_children() {
    Vec2 sz = size, ofs;
    StyleBoxRes* sb = theme_stylebox("panel");
    if (sb) {
        sz = sz - sb->minimum_size();
        ofs = sb->offset();
    }
    for (size_t i = 0; i < children.size(); ++i) {
        Control* c = sortable(children[i]);
        if (c) fit_child_in_rect(c, Rect2(ofs, sz));
    }
}

void PanelContainer::draw(Canvas& c) {
    StyleBoxRes* sb = theme_stylebox("panel");
    if (sb) sb->draw(c, Rect2(Vec2(0, 0), size));
}

/* ---- CenterContainer ---- */

bool CenterContainer::set(const std::string& key, const Json& v, const PropCtx& ctx) {
    if (key == "use_top_left") use_top_left = v.b();
    else return Container::set(key, v, ctx);
    return true;
}

Vec2 CenterContainer::get_minimum_size() {
    if (use_top_left) return Vec2();
    Vec2 ms;
    for (size_t i = 0; i < children.size(); ++i) {
        Control* c = sortable(children[i]);
        if (!c) continue;
        Vec2 s = c->combined_minimum_size();
        ms = Vec2(maxf(ms.x, s.x), maxf(ms.y, s.y));
    }
    return ms;
}

void CenterContainer::sort_children() {
    for (size_t i = 0; i < children.size(); ++i) {
        Control* c = sortable(children[i]);
        if (!c) continue;
        Vec2 m = c->combined_minimum_size();
        Vec2 ofs = use_top_left ? Vec2(floorf(-m.x * 0.5f), floorf(-m.y * 0.5f))
                                : Vec2(floorf((size.x - m.x) / 2.0f), floorf((size.y - m.y) / 2.0f));
        fit_child_in_rect(c, Rect2(ofs, m));
    }
}

/* ---- GridContainer ---- */

bool GridContainer::set(const std::string& key, const Json& v, const PropCtx& ctx) {
    if (key == "columns") columns = v.i() < 1 ? 1 : v.i();
    else return Container::set(key, v, ctx);
    return true;
}

Vec2 GridContainer::get_minimum_size() {
    std::map<int, int> colw, rowh;
    int max_row = 0, max_col = 0, idx = 0;
    for (size_t i = 0; i < children.size(); ++i) {
        Control* c = sortable(children[i]);
        if (!c) continue;
        int row = idx / columns, col = idx % columns;
        ++idx;
        Vec2 s = c->combined_minimum_size();
        colw[col] = colw.count(col) ? (colw[col] > (int)s.x ? colw[col] : (int)s.x) : (int)s.x;
        rowh[row] = rowh.count(row) ? (rowh[row] > (int)s.y ? rowh[row] : (int)s.y) : (int)s.y;
        if (col > max_col) max_col = col;
        if (row > max_row) max_row = row;
    }
    Vec2 ms;
    for (std::map<int, int>::iterator it = colw.begin(); it != colw.end(); ++it) ms.x += it->second;
    for (std::map<int, int>::iterator it = rowh.begin(); it != rowh.end(); ++it) ms.y += it->second;
    ms.y += (float)(theme_constant("v_separation") * max_row);
    ms.x += (float)(theme_constant("h_separation") * max_col);
    return ms;
}

void GridContainer::sort_children() {
    int hsep = theme_constant("h_separation"), vsep = theme_constant("v_separation");
    std::map<int, int> colw, rowh;
    std::vector<bool> col_exp(columns, false);
    std::map<int, bool> row_exp;
    int idx = 0;
    for (size_t i = 0; i < children.size(); ++i) {
        Control* c = sortable(children[i]);
        if (!c) continue;
        int row = idx / columns, col = idx % columns;
        ++idx;
        Vec2 s = c->combined_minimum_size();
        colw[col] = colw.count(col) ? (colw[col] > (int)s.x ? colw[col] : (int)s.x) : (int)s.x;
        rowh[row] = rowh.count(row) ? (rowh[row] > (int)s.y ? rowh[row] : (int)s.y) : (int)s.y;
        if (c->size_flags_h & SIZE_EXPAND) col_exp[col] = true;
        if (c->size_flags_v & SIZE_EXPAND) row_exp[row] = true;
    }
    int max_col = idx < columns ? idx : columns;
    int max_row = (int)ceilf((float)idx / (float)columns);
    for (int i = idx; i < columns; ++i) col_exp[i] = true;
    float rw = size.x, rh = size.y;
    for (std::map<int, int>::iterator it = colw.begin(); it != colw.end(); ++it)
        if (!col_exp[it->first]) rw -= it->second;
    for (std::map<int, int>::iterator it = rowh.begin(); it != rowh.end(); ++it)
        if (!row_exp[it->first]) rh -= it->second;
    rh -= vsep * (max_row - 1 > 0 ? max_row - 1 : 0);
    rw -= hsep * (max_col - 1 > 0 ? max_col - 1 : 0);

    /* Drop the widest expanded column (tallest row) while the space does not fit. */
    for (;;) {
        int n = 0, maxi = -1;
        bool fit = true;
        for (int c = 0; c < columns; ++c) {
            if (!col_exp[c]) continue;
            ++n;
            if (maxi < 0 || colw[c] > colw[maxi]) maxi = c;
        }
        if (!n) break;
        for (int c = 0; c < columns; ++c)
            if (col_exp[c] && rw / n < colw[c]) fit = false;
        if (fit) break;
        col_exp[maxi] = false;
        rw -= colw[maxi];
    }
    for (;;) {
        int n = 0, maxi = -1;
        bool fit = true;
        for (std::map<int, bool>::iterator it = row_exp.begin(); it != row_exp.end(); ++it) {
            if (!it->second) continue;
            ++n;
            if (maxi < 0 || rowh[it->first] > rowh[maxi]) maxi = it->first;
        }
        if (!n) break;
        for (std::map<int, bool>::iterator it = row_exp.begin(); it != row_exp.end(); ++it)
            if (it->second && rh / n < rowh[it->first]) fit = false;
        if (fit) break;
        row_exp[maxi] = false;
        rh -= rowh[maxi];
    }
    int ncol = 0, nrow = 0;
    for (int c = 0; c < columns; ++c) ncol += col_exp[c] ? 1 : 0;
    for (std::map<int, bool>::iterator it = row_exp.begin(); it != row_exp.end(); ++it) nrow += it->second ? 1 : 0;
    int col_expand = 0, col_rem = 0, row_expand = 0, row_rem = 0;
    if (ncol) {
        col_expand = (int)(rw / ncol);
        col_rem = (int)(rw - ncol * col_expand);
    }
    if (nrow) {
        row_expand = (int)(rh / nrow);
        row_rem = (int)(rh - nrow * row_expand);
    }
    int col_rem_idx = 0, row_rem_idx = 0;
    for (int i = 0; i < max_col && col_rem; ++i)
        if (col_exp[i]) {
            col_rem_idx = i + 1;
            --col_rem;
        }
    for (int i = 0; i < max_row && row_rem; ++i)
        if (row_exp[i]) {
            row_rem_idx = i + 1;
            --row_rem;
        }
    int col_ofs = 0, row_ofs = 0;
    idx = 0;
    for (size_t i = 0; i < children.size(); ++i) {
        Control* c = sortable(children[i]);
        if (!c) continue;
        int row = idx / columns, col = idx % columns;
        ++idx;
        if (col == 0) {
            col_ofs = 0;
            if (row > 0) {
                row_ofs += (row_exp[row - 1] ? row_expand : rowh[row - 1]) + vsep;
                if (row_exp[row - 1] && row - 1 < row_rem_idx) ++row_ofs;
            }
        }
        Vec2 s((float)(col_exp[col] ? col_expand : colw[col]), (float)(row_exp[row] ? row_expand : rowh[row]));
        if (col_exp[col] && col < col_rem_idx) s.x += 1;
        if (row_exp[row] && row < row_rem_idx) s.y += 1;
        fit_child_in_rect(c, Rect2(Vec2((float)col_ofs, (float)row_ofs), s));
        col_ofs += (int)s.x + hsep;
    }
}

/* ---- FlowContainer (last_wrap_alignment: inherit) ---- */

bool FlowContainer::set(const std::string& key, const Json& v, const PropCtx& ctx) {
    if (key == "alignment") alignment = v.i();
    else if (key == "vertical") vertical = v.b();
    else if (key == "reverse_fill") reverse_fill = v.b();
    else return Container::set(key, v, ctx);
    return true;
}

Vec2 FlowContainer::get_minimum_size() {
    Vec2 m;
    for (size_t i = 0; i < children.size(); ++i) {
        Control* c = sortable(children[i]);
        if (!c) continue;
        Vec2 s = c->combined_minimum_size();
        if (vertical) {
            m.y = maxf(m.y, (float)(int)s.y);
            m.x = cached_size;
        } else {
            m.x = maxf(m.x, (float)(int)s.x);
            m.y = cached_size;
        }
    }
    return m;
}

struct FlowLine {
    int child_count, min_line_height, line_length, stretch_avail;
    float stretch_ratio_total;
};

void FlowContainer::sort_children() {
    if (!is_visible_in_tree()) return;
    int hsep = theme_constant("h_separation"), vsep = theme_constant("v_separation");
    std::vector<FlowLine> lines;
    std::vector<Control*> kids;
    std::vector<Vec2> mins;
    int ox = 0, oy = 0, line_height = 0, in_line = 0;
    float ratio = 0;
    int container = vertical ? (int)size.y : (int)size.x;
    for (size_t i = 0; i < children.size(); ++i) {
        Control* c = sortable(children[i]);
        if (!c) continue;
        Vec2 ms = c->combined_minimum_size();
        int mw = (int)ms.x, mh = (int)ms.y;
        int& along = vertical ? oy : ox;
        int& across = vertical ? ox : oy;
        int sep_along = vertical ? vsep : hsep, sep_across = vertical ? hsep : vsep;
        int len = vertical ? mh : mw, thick = vertical ? mw : mh;
        if (in_line > 0) along += sep_along;
        if (along + len > container) {
            FlowLine l;
            l.child_count = in_line;
            l.min_line_height = line_height;
            l.line_length = along - sep_along;
            l.stretch_avail = container - l.line_length;
            l.stretch_ratio_total = ratio;
            lines.push_back(l);
            across += line_height + sep_across;
            along = 0;
            line_height = 0;
            ratio = 0;
            in_line = 0;
        }
        if (thick > line_height) line_height = thick;
        if ((vertical ? c->size_flags_v : c->size_flags_h) & SIZE_EXPAND) ratio += c->stretch_ratio;
        along += len;
        kids.push_back(c);
        mins.push_back(Vec2((float)mw, (float)mh));
        ++in_line;
    }
    FlowLine last;
    last.child_count = in_line;
    last.min_line_height = line_height;
    last.line_length = vertical ? oy : ox;
    last.stretch_avail = container - last.line_length;
    last.stretch_ratio_total = ratio;
    lines.push_back(last);

    int li = 0, in_idx = 0;
    ox = oy = 0;
    for (size_t k = 0; k < kids.size(); ++k) {
        Control* c = kids[k];
        Vec2 cs = mins[k];
        FlowLine ld = lines[li];
        if (in_idx >= ld.child_count) {
            ++li;
            in_idx = 0;
            if (vertical) {
                ox += ld.min_line_height + hsep;
                oy = 0;
            } else {
                ox = 0;
                oy += ld.min_line_height + vsep;
            }
            ld = lines[li];
        }
        if (in_idx == 0 && ld.stretch_ratio_total == 0) {
            int a = 0;
            if (alignment == ALIGN_CENTER) a = (int)(ld.stretch_avail * 0.5f);
            else if (alignment == ALIGN_END) a = ld.stretch_avail;
            if (vertical) oy += a;
            else ox += a;
        }
        if (vertical) {
            if (c->size_flags_h & (SIZE_FILL | SIZE_SHRINK_CENTER | SIZE_SHRINK_END)) cs.x = (float)ld.min_line_height;
            if (c->size_flags_v & SIZE_EXPAND) cs.y += (float)(int)(ld.stretch_avail * c->stretch_ratio / ld.stretch_ratio_total);
        } else {
            if (c->size_flags_v & (SIZE_FILL | SIZE_SHRINK_CENTER | SIZE_SHRINK_END)) cs.y = (float)ld.min_line_height;
            if (c->size_flags_h & SIZE_EXPAND) cs.x += (float)(int)(ld.stretch_avail * c->stretch_ratio / ld.stretch_ratio_total);
        }
        Rect2 r(Vec2((float)ox, (float)oy), cs);
        if (reverse_fill && !vertical) r.pos.y = size.y - r.pos.y - r.size.y;
        if (reverse_fill && vertical) r.pos.x = size.x - r.pos.x - r.size.x;
        fit_child_in_rect(c, r);
        if (vertical) oy += (int)cs.y + vsep;
        else ox += (int)cs.x + hsep;
        ++in_idx;
    }
    cached_size = (float)((vertical ? ox : oy) + line_height);
}

/* ---- Sprite2D ---- */

bool Sprite2D::set(const std::string& key, const Json& v, const PropCtx& ctx) {
    if (key == "texture") texture = bind_texture(this, ctx.resource(v));
    else if (key == "centered") centered = v.b(true);
    else if (key == "offset") jv_vec2(v, offset);
    else if (key == "flip_h") flip_h = v.b();
    else if (key == "flip_v") flip_v = v.b();
    else if (key == "region_enabled") region_enabled = v.b();
    else if (key == "region_rect") jv_rect2(v, region_rect);
    else if (key == "hframes") hframes = v.i() < 1 ? 1 : v.i();
    else if (key == "vframes") vframes = v.i() < 1 ? 1 : v.i();
    else if (key == "frame") frame = v.i();
    else return Node2D::set(key, v, ctx);
    return true;
}

bool AnimatedSprite2D::set(const std::string& key, const Json& v, const PropCtx& ctx) {
    if (key == "sprite_frames") {
        Resource* r = ctx.resource(v);
        animations.clear();
        const Json& as = r && r->props ? (*r->props)["animations"] : Json();
        for (size_t i = 0; i < as.size(); ++i) {
            Animation a;
            a.name = as[i]["name"].str();
            a.speed = as[i]["speed"].f(5.0f);
            a.loop = as[i]["loop"].b(true);
            const Json& fs = as[i]["frames"];
            for (size_t k = 0; k < fs.size(); ++k) {
                Frame f;
                f.texture = as_texture(ctx.resource(fs[k]["texture"]));
                f.duration = fs[k]["duration"].f(1.0f);
                a.frames.push_back(f);
            }
            animations.push_back(a);
        }
    } else if (key == "animation") animation = v.str();
    else if (key == "autoplay") {
        if (!v.str().empty()) {
            animation = v.str();
            playing = true;
        }
    } else if (key == "frame") frame = v.i();
    else if (key == "frame_progress") progress = v.f();
    else if (key == "speed_scale") speed_scale = v.f(1.0f);
    else if (key == "centered") centered = v.b(true);
    else if (key == "offset") jv_vec2(v, offset);
    else if (key == "flip_h") flip_h = v.b();
    else if (key == "flip_v") flip_v = v.b();
    else return Node2D::set(key, v, ctx);
    return true;
}

const AnimatedSprite2D::Animation* AnimatedSprite2D::current() const {
    std::string name = animation.empty() ? std::string("default") : animation;
    for (size_t i = 0; i < animations.size(); ++i)
        if (animations[i].name == name) return &animations[i];
    return NULL;
}

void AnimatedSprite2D::play(const std::string& name) {
    if (name != animation) {
        frame = 0;
        progress = 0;
    }
    animation = name;
    playing = true;
}

/* AnimatedSprite2D::_notification(INTERNAL_PROCESS). */
void AnimatedSprite2D::process(double dt) {
    const Animation* a = current();
    if (!playing || !a || a->frames.empty()) return;
    double speed = a->speed * speed_scale;
    if (speed == 0) return;
    double remaining = dt;
    int guard = 0;
    while (remaining > 0 && ++guard < 1000) {
        if (frame < 0 || frame >= (int)a->frames.size()) frame = 0;
        double frame_len = a->frames[(size_t)frame].duration / (speed < 0 ? -speed : speed);
        if (frame_len <= 0) frame_len = 1e-6;
        double left = (1.0 - progress) * frame_len;
        if (remaining < left) {
            progress += (float)(remaining / frame_len);
            break;
        }
        remaining -= left;
        progress = 0;
        if (frame + 1 < (int)a->frames.size()) {
            ++frame;
        } else if (a->loop) {
            frame = 0;
        } else {
            progress = 1;
            playing = false;
            break;
        }
    }
}

void AnimatedSprite2D::draw(Canvas& c) {
    const Animation* a = current();
    if (!a || frame < 0 || frame >= (int)a->frames.size()) return;
    Texture2DRes* t = a->frames[(size_t)frame].texture;
    if (!t) return;
    Vec2 fs = t->size();
    Vec2 d = offset;
    if (centered) d = d - fs * 0.5f;
    /* the VFX shaders' vertex(): VERTEX += pivot_offset / TEXTURE_PIXEL_SIZE */
    if (material && material->kind == Resource::RES_SHADER_MATERIAL) {
        Vec2 pv = shader_param_vec2(material, "pivot_offset", Vec2(0, 0));
        d = d + Vec2(pv.x * fs.x, pv.y * fs.y);
    }
    Rect2 dst(d, fs);
    if (flip_h) dst.size.x = -dst.size.x;
    if (flip_v) dst.size.y = -dst.size.y;
    c.draw_texture_rect_region(t, dst, Rect2(Vec2(0, 0), fs));
}

bool CanvasGroup::set(const std::string& key, const Json& v, const PropCtx& ctx) {
    if (key == "fit_margin") fit_margin = v.f(10);
    else if (key == "clear_margin") {
    } else {
        bool ok = Node2D::set(key, v, ctx);
        if (key == "material") refresh();
        return ok;
    }
    return true;
}

static bool mask_material(const MaterialRes* m) {
    if (!m || m->kind != Resource::RES_SHADER_MATERIAL || m->shader != "shaders/blur/canvas_group_mask_blur.gdshader")
        return false;
    return m->props && (*m->props)["shader_parameter/mask"].b();
}

void CanvasGroup::refresh() { clip_children = mask_material(material) ? 1 : 0; }

/* The visible children's rects in its coordinates, grown by fit_margin. */
Rect2 CanvasGroup::item_rect() const {
    bool any = false;
    float x0 = 0, y0 = 0, x1 = 0, y1 = 0;
    for (size_t i = 0; i < children.size(); ++i) {
        Node* ch = children[i];
        if (!ch->is_canvas_item() || !((CanvasItem*)ch)->visible) continue;
        CanvasItem* ci = (CanvasItem*)ch;
        Rect2 r = ci->item_rect();
        if (r.size.x <= 0 || r.size.y <= 0) continue;
        Xform2D xf = ci->get_transform();
        Vec2 p[4] = { xf.xform(r.pos), xf.xform(Vec2(r.pos.x + r.size.x, r.pos.y)), xf.xform(r.end()),
                      xf.xform(Vec2(r.pos.x, r.pos.y + r.size.y)) };
        for (int k = 0; k < 4; ++k) {
            if (!any) {
                x0 = x1 = p[k].x;
                y0 = y1 = p[k].y;
                any = true;
            }
            x0 = minf(x0, p[k].x);
            y0 = minf(y0, p[k].y);
            x1 = maxf(x1, p[k].x);
            y1 = maxf(y1, p[k].y);
        }
    }
    if (!any) return Rect2();
    return Rect2(x0 - fit_margin, y0 - fit_margin, x1 - x0 + 2 * fit_margin, y1 - y0 + 2 * fit_margin);
}

/* canvas_group_mask_blur, mask: the group through mask_texture's alpha,
 * the group's UV mapped onto mask_region (in the atlas's pixels) widened by
 * mask_offset; the atlas behind the texture is what the shader samples. */
void CanvasGroup::draw(Canvas& c) {
    if (!clip_children || !material || !material->props) return;
    Resource* r = shader_param_resource(material, "mask_texture");
    if (!r || !r->is_texture()) return;
    Texture2DRes* atlas = (Texture2DRes*)r;
    if (r->kind == Resource::RES_ATLAS_TEXTURE && ((AtlasTextureRes*)r)->atlas) atlas = ((AtlasTextureRes*)r)->atlas;
    const Json& region = (*material->props)["shader_parameter/mask_region"]["v"];
    Vec2 offset = shader_param_vec2(material, "mask_offset", Vec2());
    if (region.size() < 4) return;
    Rect2 src((float)region[(size_t)0].num() - offset.x, (float)region[1].num() - offset.y,
              (float)region[2].num() + offset.x, (float)region[3].num() + offset.y);
    c.draw_texture_rect_region(atlas, item_rect(), src);
}

/* ---- MeshInstance2D ---- */

static int b64_value(char ch) {
    if (ch >= 'A' && ch <= 'Z') return ch - 'A';
    if (ch >= 'a' && ch <= 'z') return ch - 'a' + 26;
    if (ch >= '0' && ch <= '9') return ch - '0' + 52;
    if (ch == '+') return 62;
    if (ch == '/') return 63;
    return -1;
}

/* A PackedByteArray as the converter writes it: {"v": ["<base64>"]}. */
static void packed_bytes(const Json& v, std::vector<u8>& out) {
    out.clear();
    const Json& a = v["v"];
    std::string s = a.size() > 0 && a[(size_t)0].is_string() ? a[(size_t)0].str() : std::string();
    u32 acc = 0;
    int bits = 0;
    for (size_t i = 0; i < s.size(); ++i) {
        int x = b64_value(s[i]);
        if (x < 0) continue;
        acc = (acc << 6) | (u32)x;
        bits += 6;
        if (bits >= 8) {
            bits -= 8;
            out.push_back((u8)((acc >> bits) & 0xFF));
        }
    }
}

static float read_f32(const u8* p) {
    float f;
    memcpy(&f, p, 4);
    return f;
}

bool MeshInstance2D::set(const std::string& key, const Json& v, const PropCtx& ctx) {
    if (key == "texture") {
        texture = as_texture(ctx.resource(v));
        return true;
    }
    if (key == "mesh") {
        load(ctx.resource(v));
        return true;
    }
    return Node2D::set(key, v, ctx);
}

/* Godot 4's surface arrays (format version 2): the vertex stream holds
 * positions (vec2 with ARRAY_FLAG_USE_2D_VERTICES, else vec3) and the
 * packed normal and tangent; the attribute stream colour (RGBA8), uv,
 * uv2 and customs; indices are 16 bit below 65536 vertices. */
void MeshInstance2D::load(const Resource* mesh) {
    loaded = false;
    verts.clear();
    uvs.clear();
    colors.clear();
    idx.clear();
    if (!mesh || !mesh->props || mesh->type != "ArrayMesh") return;
    const Json& surfaces = (*mesh->props)["_surfaces"];
    if (surfaces.size() == 0) return;
    const Json& s = surfaces[(size_t)0];
    u64 format = (u64)s["format"].num();
    int primitive = s["primitive"].i(3);
    int count = s["vertex_count"].i();
    const u64 F_VERTEX = 1, F_NORMAL = 2, F_TANGENT = 4, F_COLOR = 8, F_UV = 16, F_UV2 = 32, F_INDEX = 1u << 12;
    const u64 F_2D = 1u << 25, F_COMPRESS = 1u << 29, F_V2 = (u64)1 << 35;
    if (!(format & F_VERTEX) || (format & F_COMPRESS) || !(format & F_V2) || count <= 0) {
        log_warn("MeshInstance2D: surface format %llx not ported", (unsigned long long)format);
        return;
    }
    std::vector<u8> vd, ad, id;
    packed_bytes(s["vertex_data"], vd);
    packed_bytes(s["attribute_data"], ad);
    int pos_size = (format & F_2D) ? 8 : 12;
    int vstride = pos_size + ((format & F_NORMAL) ? 4 : 0) + ((format & F_TANGENT) ? 4 : 0);
    int astride = ((format & F_COLOR) ? 4 : 0) + ((format & F_UV) ? 8 : 0) + ((format & F_UV2) ? 8 : 0);
    if ((int)vd.size() < vstride * count || (int)ad.size() < astride * count) {
        log_warn("MeshInstance2D: short surface data");
        return;
    }
    for (int i = 0; i < count; ++i) {
        const u8* p = &vd[(size_t)i * vstride];
        verts.push_back(Vec2(read_f32(p), read_f32(p + 4)));
        const u8* a = astride ? &ad[(size_t)i * astride] : NULL;
        Color col(1, 1, 1, 1);
        if (format & F_COLOR) {
            col = Color(a[0] / 255.0f, a[1] / 255.0f, a[2] / 255.0f, a[3] / 255.0f);
            a += 4;
        }
        colors.push_back(col);
        uvs.push_back((format & F_UV) ? Vec2(read_f32(a), read_f32(a + 4)) : Vec2(0, 0));
    }
    std::vector<int> order;
    if (format & F_INDEX) {
        packed_bytes(s["index_data"], id);
        bool wide = count > 65535;
        size_t n = wide ? id.size() / 4 : id.size() / 2;
        for (size_t i = 0; i < n; ++i) {
            int k = wide ? (int)(id[i * 4] | (id[i * 4 + 1] << 8) | (id[i * 4 + 2] << 16) | ((u32)id[i * 4 + 3] << 24))
                         : (int)(id[i * 2] | (id[i * 2 + 1] << 8));
            order.push_back(k < count ? k : 0);
        }
    } else {
        for (int i = 0; i < count; ++i) order.push_back(i);
    }
    if (primitive == 3) {
        idx.assign(order.begin(), order.begin() + (order.size() / 3) * 3);
    } else if (primitive == 4) {
        for (size_t i = 2; i < order.size(); ++i) {
            /* strips alternate their winding; the canvas does not cull */
            idx.push_back(order[i - 2]);
            idx.push_back(order[i - 1]);
            idx.push_back(order[i]);
        }
    } else {
        log_warn("MeshInstance2D: primitive %d not ported", primitive);
        return;
    }
    loaded = true;
}

void MeshInstance2D::draw(Canvas& c) {
    if (!loaded || idx.empty()) return;
    ImageTextureRes* img = NULL;
    if (texture) {
        Vec2 ts = texture->size();
        Rect2 dst, src;
        texture->get_rect_region(Rect2(Vec2(), ts), Rect2(Vec2(), ts), dst, src, &img);
        if (!img) return;
        Vec2 is = img->size();
        std::vector<Vec2> uv(uvs.size());
        for (size_t i = 0; i < uvs.size(); ++i)
            uv[i] = Vec2((src.pos.x + uvs[i].x * src.size.x) / is.x, (src.pos.y + uvs[i].y * src.size.y) / is.y);
        c.draw_triangles(img, verts, uv, colors, idx);
        return;
    }
    c.draw_triangles(NULL, verts, uvs, colors, idx);
}

void Sprite2D::draw(Canvas& c) {
    if (!texture) return;
    Rect2 base = region_enabled ? region_rect : Rect2(Vec2(0, 0), texture->size());
    Vec2 fs(base.size.x / hframes, base.size.y / vframes);
    Vec2 fo((float)(frame % hframes) * fs.x, (float)(frame / hframes) * fs.y);
    Rect2 src(base.pos + fo, fs);
    Vec2 d = offset;
    if (centered) d = d - fs * 0.5f;
    /* the VFX shaders' vertex(): VERTEX += pivot_offset / TEXTURE_PIXEL_SIZE */
    if (material && material->kind == Resource::RES_SHADER_MATERIAL) {
        Vec2 pv = shader_param_vec2(material, "pivot_offset", Vec2(0, 0));
        Vec2 ts = texture->size();
        d = d + Vec2(pv.x * ts.x, pv.y * ts.y);
    }
    Rect2 dst(d, fs);
    if (flip_h) dst.size.x = -dst.size.x;
    if (flip_v) dst.size.y = -dst.size.y;
    c.draw_texture_rect_region(texture, dst, src);
}

} /* namespace spire */

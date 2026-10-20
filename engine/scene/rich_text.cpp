#include "rich_text.h"

#include "canvas.h"
#include "scene.h"
#include "value.h"

#include <math.h>
#include <stdlib.h>
#include <string.h>

namespace spire {

/* ---- custom effects ---- */

struct EffectEntry {
    std::string name;
    RtlEffectFn fn;
};
static std::vector<EffectEntry>& effects() {
    static std::vector<EffectEntry> e;
    return e;
}

void register_rtl_effect(const char* name, RtlEffectFn fn) {
    for (size_t i = 0; i < effects().size(); ++i)
        if (effects()[i].name == name) {
            effects()[i].fn = fn;
            return;
        }
    EffectEntry e;
    e.name = name;
    e.fn = fn;
    effects().push_back(e);
}

static bool g_text_effects = true;

void set_rtl_text_effects(bool on) { g_text_effects = on; }

static bool transforms_text(const std::string& name) {
    return name == "sine" || name == "fly_in" || name == "scramble" || name == "thinky_dots" || name == "ancient_banner";
}

static RtlEffectFn find_effect(const std::string& name) {
    if (!g_text_effects && transforms_text(name)) return NULL;
    for (size_t i = 0; i < effects().size(); ++i)
        if (effects()[i].name == name) return effects()[i].fn;
    return NULL;
}

/* ---- data ---- */

enum { RUN_TEXT, RUN_IMAGE, RUN_NEWLINE };
enum { FONT_NORMAL, FONT_BOLD, FONT_ITALICS, FONT_BOLD_ITALICS, FONT_MONO };
/* Godot's InlineAlignment bits */
enum { IA_TOP_TO = 0, IA_CENTER_TO = 1, IA_BASELINE_TO = 3, IA_BOTTOM_TO = 2, IA_IMAGE_MASK = 3, IA_TO_TOP = 0,
       IA_TO_CENTER = 4, IA_TO_BASELINE = 8, IA_TO_BOTTOM = 12, IA_TEXT_MASK = 12 };

struct RichTextLabel::Style {
    int def_font;             /* FONT_* from b / i / code, -1 none */
    FontRes* font;            /* [font] */
    int font_item_size;       /* size of the font item ([b] etc. use the theme's) */
    int font_size;            /* [font_size], 0 none */
    bool has_color;
    Color color;
    int outline_size;         /* -1: theme */
    bool has_outline_color;
    Color outline_color;
    bool underline, strike;
    /* custom effects, outermost first */
    std::vector<std::string> fx;
    std::vector<std::map<std::string, std::string> > fx_env;
    std::vector<int> fx_start; /* character offset where each opened */
    int align;                /* paragraph alignment, -1 none */
    Style()
        : def_font(-1), font(NULL), font_item_size(0), font_size(0), has_color(false), outline_size(-1),
          has_outline_color(false), underline(false), strike(false), align(-1) {}
};

struct RichTextLabel::Run {
    int type;
    std::vector<u32> text;
    Style st;
    int char_ofs; /* in the whole text */
    Texture2DRes* image;
    Vec2 img_size;
    Color img_color;
    int img_align;
    /* layout */
    FontRes* font;
    int font_size;
    int start, end; /* in the paragraph's text */
    Run() : type(RUN_TEXT), char_ofs(0), image(NULL), img_align(IA_CENTER_TO | IA_TO_CENTER), font(NULL),
            font_size(16), start(0), end(0) {}
};

struct RtlLine {
    ShapedText st;
    std::vector<Rect2> objects; /* by run index of image runs (x within the line, y from the baseline) */
    std::vector<int> object_runs;
};

struct RichTextLabel::Para {
    std::vector<Run> runs;
    int align;
    bool has_content;
    std::vector<u32> cps;
    std::vector<int> run_at; /* run index of each character */
    ShapedText shaped;
    std::vector<RtlLine> lines;
    float offset_y;
    Vec2 size;
    Para() : align(-1), has_content(false), offset_y(0) {}
};

RichTextLabel::RichTextLabel()
    : bbcode(false), fit_content(false), scroll_active(true), autowrap(AUTOWRAP_WORD_SMART),
      trim_flags(BREAK_TRIM_START_EDGE_SPACES | BREAK_TRIM_END_EDGE_SPACES), h_align(HALIGN_LEFT), v_align(VALIGN_TOP),
      visible_characters(-1), visible_ratio(1.0f), elapsed_time(0), paras_(new std::vector<Para>()), laid_width_(-1) {}

RichTextLabel::~RichTextLabel() { delete paras_; }

bool RichTextLabel::set(const std::string& key, const Json& v, const PropCtx& ctx) {
    if (key == "text") text = v.str();
    else if (key == "bbcode_enabled") bbcode = v.b();
    else if (key == "fit_content") fit_content = v.b();
    else if (key == "scroll_active") scroll_active = v.b(true);
    else if (key == "autowrap_mode") autowrap = v.i();
    else if (key == "autowrap_trim_flags") trim_flags = v.i();
    else if (key == "horizontal_alignment") h_align = v.i();
    else if (key == "vertical_alignment") v_align = v.i();
    else if (key == "visible_characters") visible_characters = v.i(-1);
    else if (key == "visible_ratio") visible_ratio = jv_float(v, 1.0f);
    else return Control::set(key, v, ctx);
    return true;
}

/* ---- BBCode (RichTextLabel::append_text) ---- */

static int hexval(u32 c) {
    if (c >= '0' && c <= '9') return (int)(c - '0');
    if (c >= 'a' && c <= 'f') return (int)(c - 'a' + 10);
    if (c >= 'A' && c <= 'F') return (int)(c - 'A' + 10);
    return -1;
}

/* Color::from_string: #hex (rgb, rgba, rrggbb, rrggbbaa) or a name. */
static bool parse_color(const std::string& s0, Color& out) {
    std::string s = s0;
    if (!s.empty() && s[0] == '#') s = s.substr(1);
    bool hex = !s.empty();
    for (size_t i = 0; i < s.size(); ++i)
        if (hexval((u8)s[i]) < 0) hex = false;
    if (hex && (s.size() == 3 || s.size() == 4 || s.size() == 6 || s.size() == 8)) {
        int n = s.size() <= 4 ? 1 : 2;
        float c[4] = { 1, 1, 1, 1 };
        for (size_t k = 0; k < s.size() / n; ++k) {
            int v = n == 1 ? hexval((u8)s[k]) * 17 : hexval((u8)s[2 * k]) * 16 + hexval((u8)s[2 * k + 1]);
            c[k] = v / 255.0f;
        }
        out = Color(c[0], c[1], c[2], c[3]);
        return true;
    }
    static const struct {
        const char* name;
        u32 rgb;
    } names[] = { { "white", 0xFFFFFF }, { "black", 0x000000 }, { "red", 0xFF0000 }, { "green", 0x00FF00 },
                  { "blue", 0x0000FF }, { "yellow", 0xFFFF00 }, { "gray", 0xBEBEBE }, { "grey", 0xBEBEBE },
                  { "orange", 0xFFA500 }, { "purple", 0xA020F0 }, { "pink", 0xFFC0CB }, { "aqua", 0x00FFFF },
                  { "cyan", 0x00FFFF }, { "magenta", 0xFF00FF }, { "gold", 0xFFD700 }, { "lime", 0x00FF00 },
                  { NULL, 0 } };
    std::string l = s0;
    for (size_t i = 0; i < l.size(); ++i) l[i] = (char)tolower((unsigned char)l[i]);
    for (int i = 0; names[i].name; ++i)
        if (l == names[i].name) {
            u32 v = names[i].rgb;
            out = Color(((v >> 16) & 255) / 255.0f, ((v >> 8) & 255) / 255.0f, (v & 255) / 255.0f, 1.0f);
            return true;
        }
    if (l == "transparent") {
        out = Color(1, 1, 1, 0);
        return true;
    }
    return false;
}

bool rtl_parse_color(const std::string& s, Color& out) { return parse_color(s, out); }

static std::string unquote(const std::string& s) {
    if (s.size() >= 2 && (s[0] == '"' || s[0] == '\'') && s[s.size() - 1] == s[0]) return s.substr(1, s.size() - 2);
    return s;
}

static std::string encode_utf8(const std::vector<u32>& v, size_t a, size_t b) {
    std::string out;
    for (size_t i = a; i < b && i < v.size(); ++i) {
        u32 c = v[i];
        if (c < 0x80) out += (char)c;
        else if (c < 0x800) {
            out += (char)(0xC0 | (c >> 6));
            out += (char)(0x80 | (c & 0x3F));
        } else if (c < 0x10000) {
            out += (char)(0xE0 | (c >> 12));
            out += (char)(0x80 | ((c >> 6) & 0x3F));
            out += (char)(0x80 | (c & 0x3F));
        } else {
            out += (char)(0xF0 | (c >> 18));
            out += (char)(0x80 | ((c >> 12) & 0x3F));
            out += (char)(0x80 | ((c >> 6) & 0x3F));
            out += (char)(0x80 | (c & 0x3F));
        }
    }
    return out;
}

struct RtlBuilder {
    std::vector<RichTextLabel::Para>& paras;
    std::vector<std::pair<std::string, RichTextLabel::Style> > stack; /* tag, style after it */
    int char_ofs;
    int default_align;
    explicit RtlBuilder(std::vector<RichTextLabel::Para>& p, int def_align)
        : paras(p), char_ofs(0), default_align(def_align) {
        paras.clear();
        paras.push_back(RichTextLabel::Para());
    }
    RichTextLabel::Style cur() const { return stack.empty() ? RichTextLabel::Style() : stack.back().second; }
    RichTextLabel::Para& para() { return paras.back(); }
    void touch(const RichTextLabel::Style& st) {
        if (!para().has_content) {
            para().align = st.align >= 0 ? st.align : default_align;
            para().has_content = true;
        }
    }
    void newline() {
        RichTextLabel::Run r;
        r.type = RUN_NEWLINE;
        r.st = cur();
        r.char_ofs = char_ofs++;
        touch(r.st);
        para().runs.push_back(r);
        paras.push_back(RichTextLabel::Para());
    }
    void add_text(const std::vector<u32>& t, size_t a, size_t b) {
        size_t s = a;
        for (size_t i = a; i <= b; ++i) {
            if (i == b || t[i] == '\n') {
                if (i > s) {
                    RichTextLabel::Style st = cur();
                    touch(st);
                    std::vector<RichTextLabel::Run>& runs = para().runs;
                    if (!runs.empty() && runs.back().type == RUN_TEXT && runs.back().char_ofs + (int)runs.back().text.size() == char_ofs &&
                        runs.back().st.def_font == st.def_font && runs.back().st.font == st.font &&
                        runs.back().st.font_size == st.font_size && runs.back().st.fx == st.fx &&
                        runs.back().st.fx_start == st.fx_start &&
                        runs.back().st.has_color == st.has_color && runs.back().st.color == st.color &&
                        runs.back().st.outline_size == st.outline_size && runs.back().st.underline == st.underline &&
                        runs.back().st.strike == st.strike && runs.back().st.has_outline_color == st.has_outline_color &&
                        runs.back().st.outline_color == st.outline_color) {
                        runs.back().text.insert(runs.back().text.end(), t.begin() + s, t.begin() + i);
                    } else {
                        RichTextLabel::Run r;
                        r.type = RUN_TEXT;
                        r.st = st;
                        r.char_ofs = char_ofs;
                        r.text.assign(t.begin() + s, t.begin() + i);
                        runs.push_back(r);
                    }
                    char_ofs += (int)(i - s);
                }
                if (i < b) newline();
                s = i + 1;
            }
        }
    }
    void push(const std::string& tag, const RichTextLabel::Style& st) { stack.push_back(std::make_pair(tag, st)); }
    void paragraph(int align) {
        /* push_paragraph: a new line when the current one has content */
        if (para().has_content) paras.push_back(RichTextLabel::Para());
        RichTextLabel::Style st = cur();
        st.align = align;
        push("", st);
        para().align = align;
        para().has_content = false;
    }
};

static int find_unquoted(const std::vector<u32>& s, u32 ch, size_t from) {
    bool in_q = false;
    u32 q = 0;
    for (size_t i = from; i < s.size(); ++i) {
        if (in_q) {
            if (s[i] == q) in_q = false;
        } else if (s[i] == '"' || s[i] == '\'') {
            in_q = true;
            q = s[i];
        } else if (s[i] == ch) {
            return (int)i;
        }
    }
    return -1;
}

static std::vector<std::string> split_unquoted(const std::string& s, char sep) {
    std::vector<std::string> out;
    std::string cur;
    bool in_q = false;
    char q = 0;
    for (size_t i = 0; i < s.size(); ++i) {
        char c = s[i];
        if (in_q) {
            if (c == q) in_q = false;
            cur += c;
        } else if (c == '"' || c == '\'') {
            in_q = true;
            q = c;
            cur += c;
        } else if (c == sep) {
            if (!cur.empty()) out.push_back(cur);
            cur.clear();
        } else {
            cur += c;
        }
    }
    if (!cur.empty()) out.push_back(cur);
    return out;
}

void RichTextLabel::parse() {
    std::vector<u32> t;
    std::string src = text;
    /* \r\n -> \n */
    std::string clean;
    for (size_t i = 0; i < src.size(); ++i)
        if (!(src[i] == '\r' && i + 1 < src.size() && src[i + 1] == '\n')) clean += src[i];
    utf8_decode(clean, t);
    RtlBuilder b(*paras_, h_align);
    if (!bbcode) {
        b.add_text(t, 0, t.size());
        return;
    }
    size_t pos = 0;
    while (pos <= t.size()) {
        int brk = -1;
        for (size_t i = pos; i < t.size(); ++i)
            if (t[i] == '[') {
                brk = (int)i;
                break;
            }
        if (brk < 0) {
            b.add_text(t, pos, t.size());
            break;
        }
        int brk_end = find_unquoted(t, ']', brk + 1);
        if (brk_end < 0) {
            b.add_text(t, pos, t.size());
            break;
        }
        std::string tag = encode_utf8(t, brk + 1, brk_end);
        std::vector<std::string> parts = split_unquoted(tag, ' ');
        std::string name = parts.empty() ? tag : parts[0], value;
        std::map<std::string, std::string> opts;
        for (size_t i = 1; i < parts.size(); ++i) {
            size_t eq = parts[i].find('=');
            if (eq != std::string::npos) opts[parts[i].substr(0, eq)] = unquote(parts[i].substr(eq + 1));
        }
        size_t eq = name.find('=');
        if (eq != std::string::npos) {
            value = name.substr(eq + 1);
            name = name.substr(0, eq);
        }

        if (!tag.empty() && tag[0] == '/' && !b.stack.empty()) {
            /* the top of the tag stack must match */
            std::string want = tag.substr(1);
            bool ok = b.stack.back().first == want;
            if (!ok) {
                b.add_text(t, pos, brk);
                std::vector<u32> lit(1, '[');
                std::vector<u32> rest;
                utf8_decode(tag, rest);
                lit.insert(lit.end(), rest.begin(), rest.end());
                b.add_text(lit, 0, lit.size());
                pos = brk_end; /* "]" is parsed as text next */
                continue;
            }
            b.add_text(t, pos, brk);
            b.stack.pop_back();
            pos = brk_end + 1;
            continue;
        }
        b.add_text(t, pos, brk);
        Style st = b.cur();
        bool pushed = true;
        if (tag == "b" || tag == "i") {
            bool bold = tag == "b";
            bool other = false;
            for (size_t i = 0; i < b.stack.size(); ++i)
                if (b.stack[i].first == (bold ? "i" : "b")) other = true;
            st.def_font = other ? FONT_BOLD_ITALICS : (bold ? FONT_BOLD : FONT_ITALICS);
            st.font = NULL;
            b.push(tag, st);
        } else if (tag == "code") {
            st.def_font = FONT_MONO;
            st.font = NULL;
            b.push(tag, st);
        } else if (tag == "u") {
            st.underline = true;
            b.push(tag, st);
        } else if (tag == "s") {
            st.strike = true;
            b.push(tag, st);
        } else if (tag == "center" || tag == "left" || tag == "right" || tag == "fill") {
            int al = tag == "center" ? HALIGN_CENTER : tag == "right" ? HALIGN_RIGHT : tag == "fill" ? HALIGN_FILL : HALIGN_LEFT;
            b.paragraph(al);
            b.stack.back().first = tag;
        } else if (name == "p") {
            int al = b.cur().align >= 0 ? b.cur().align : h_align;
            std::string a = opts.count("align") ? opts["align"] : std::string();
            if (a == "center" || a == "c") al = HALIGN_CENTER;
            else if (a == "right" || a == "r") al = HALIGN_RIGHT;
            else if (a == "left" || a == "l") al = HALIGN_LEFT;
            else if (a == "fill" || a == "f") al = HALIGN_FILL;
            b.paragraph(al);
            b.stack.back().first = "p";
        } else if (name == "color") {
            if (parse_color(unquote(value), st.color)) st.has_color = true;
            b.push("color", st);
        } else if (name == "font_size") {
            st.font_size = atoi(value.c_str());
            b.push("font_size", st);
        } else if (name == "font" && !value.empty()) {
            std::string p = unquote(value);
            if (p.compare(0, 6, "res://") == 0) p = p.substr(6);
            SceneLibrary* lib = SceneLibrary::current();
            Resource* r = lib ? lib->ext(p) : NULL;
            if (r && r->kind == Resource::RES_FONT) {
                st.font = (FontRes*)r;
                st.def_font = -1;
            }
            b.push("font", st);
        } else if (name == "outline_size") {
            st.outline_size = atoi(value.c_str());
            b.push("outline_size", st);
        } else if (name == "outline_color") {
            if (parse_color(unquote(value), st.outline_color)) st.has_outline_color = true;
            b.push("outline_color", st);
        } else if (name == "url" || name == "hint" || name == "lang" || name == "indent" || name == "bgcolor" ||
                   name == "fgcolor" || name == "wave" || name == "tornado" || name == "shake" || name == "fade" ||
                   name == "rainbow" || name == "pulse" || name == "opentype_features" || name == "otf") {
            b.push(name, st); /* layout-neutral here; effects are drawn static */
        } else if (tag == "lb" || tag == "rb") {
            std::vector<u32> ch(1, tag == "lb" ? (u32)'[' : (u32)']');
            b.add_text(ch, 0, 1);
            pushed = false;
        } else if (name == "img") {
            int end = -1;
            for (size_t i = brk_end; i < t.size(); ++i)
                if (t[i] == '[') {
                    end = (int)i;
                    break;
                }
            if (end < 0) end = (int)t.size();
            std::string path = encode_utf8(t, brk_end + 1, end);
            if (path.compare(0, 6, "res://") == 0) path = path.substr(6);
            SceneLibrary* lib = SceneLibrary::current();
            Resource* r = lib ? lib->ext(path) : NULL;
            if (r && r->is_texture()) {
                Run run;
                run.type = RUN_IMAGE;
                run.st = st;
                run.char_ofs = b.char_ofs++;
                run.image = (Texture2DRes*)r;
                run.img_color = Color();
                if (opts.count("color")) parse_color(opts["color"], run.img_color);
                int w = 0, h = 0;
                if (!value.empty()) {
                    size_t x = value.find('x');
                    w = atoi(value.substr(0, x).c_str());
                    if (x != std::string::npos) h = atoi(value.substr(x + 1).c_str());
                } else {
                    if (opts.count("width")) w = atoi(opts["width"].c_str());
                    if (opts.count("height")) h = atoi(opts["height"].c_str());
                }
                /* _get_image_size */
                Vec2 is = run.image->size();
                if (w > 0 && h > 0) run.img_size = Vec2((float)w, (float)h);
                else if (w > 0) run.img_size = Vec2((float)w, is.x > 0 ? w * is.y / is.x : 0);
                else if (h > 0) run.img_size = Vec2(is.y > 0 ? h * is.x / is.y : 0, (float)h);
                else run.img_size = is;
                b.touch(st);
                b.para().runs.push_back(run);
            }
            b.push("img", st);
            pos = end;
            continue;
        } else {
            /* A custom effect, when installed; otherwise the "[" is text. */
            bool installed = false;
            for (size_t i = 0; i < custom_effects.size(); ++i)
                if (custom_effects[i] == name) installed = true;
            if (installed && !tag.empty()) {
                st.fx.push_back(name);
                if (!value.empty()) opts[""] = value;
                st.fx_env.push_back(opts);
                st.fx_start.push_back(b.char_ofs);
                b.push(name, st);
            } else {
                std::vector<u32> lb(1, '[');
                b.add_text(lb, 0, 1);
                pos = brk + 1;
                continue;
            }
        }
        (void)pushed;
        pos = brk_end + 1;
    }
}

/* ---- layout (_shape_line / TextParagraph / _realign) ---- */

void RichTextLabel::layout(float width) {
    std::string key = text + (bbcode ? "1" : "0");
    if (key != parsed_key_) {
        parse();
        parsed_key_ = key;
        laid_width_ = -1;
    }
    /* Fonts and sizes come from the theme; re-resolve every layout. */
    FontRes* fonts[5] = { theme_font("normal_font"), theme_font("bold_font"), theme_font("italics_font"),
                          theme_font("bold_italics_font"), theme_font("mono_font") };
    int sizes[5] = { theme_font_size("normal_font_size"), theme_font_size("bold_font_size"),
                     theme_font_size("italics_font_size"), theme_font_size("bold_italics_font_size"),
                     theme_font_size("mono_font_size") };
    int line_sep = theme_constant("line_separation"), para_sep = theme_constant("paragraph_separation");
    int flags = BREAK_MANDATORY;
    if (autowrap == AUTOWRAP_WORD_SMART) flags = BREAK_WORD_BOUND | BREAK_ADAPTIVE | BREAK_MANDATORY;
    else if (autowrap == AUTOWRAP_WORD) flags = BREAK_WORD_BOUND | BREAK_MANDATORY;
    else if (autowrap == AUTOWRAP_ARBITRARY) flags = BREAK_GRAPHEME_BOUND | BREAK_MANDATORY;
    flags |= trim_flags;
    float h = 0;
    std::vector<int> br;
    for (size_t p = 0; p < paras_->size(); ++p) {
        Para& pa = (*paras_)[p];
        pa.cps.clear();
        pa.run_at.clear();
        for (size_t r = 0; r < pa.runs.size(); ++r) {
            Run& run = pa.runs[r];
            run.start = (int)pa.cps.size();
            if (run.type == RUN_TEXT) pa.cps.insert(pa.cps.end(), run.text.begin(), run.text.end());
            else pa.cps.push_back(run.type == RUN_IMAGE ? 0xFFFC : 0x200B);
            run.end = (int)pa.cps.size();
            pa.run_at.insert(pa.run_at.end(), run.end - run.start, (int)r);
            /* _find_font / _find_font_size */
            int df = run.st.def_font;
            run.font = run.st.font ? run.st.font : fonts[df >= 0 ? df : 0];
            run.font_size = df >= 0 ? sizes[df] : sizes[0];
            if (run.st.font_size > 0) run.font_size = run.st.font_size;
        }
        pa.shaped.clear();
        for (size_t r = 0; r < pa.runs.size(); ++r) {
            Run& run = pa.runs[r];
            if (run.type == RUN_IMAGE) shape_object(pa.shaped, run.start, run.img_size.x, (int)r);
            else shape_run(pa.cps, run.start, run.end, run.font, run.font_size, run.end == (int)pa.cps.size(), pa.shaped);
        }
        if (pa.runs.empty()) {
            /* an empty paragraph still has the base font's height */
            pa.shaped.ascent = font_ascent(fonts[0], sizes[0]);
            pa.shaped.descent = font_descent(fonts[0], sizes[0]);
        }
        shape_finish(pa.cps, pa.shaped);
        line_breaks(pa.shaped, autowrap == AUTOWRAP_OFF ? 0.0f : width, flags, br);
        pa.lines.clear();
        pa.size = Vec2();
        for (size_t i = 0; i + 1 < br.size(); i += 2) {
            RtlLine l;
            shaped_substr(pa.shaped, br[i], br[i + 1], l.st);
            /* _realign: inline objects against the line's ascent / descent */
            float full_asc = l.st.ascent, full_desc = l.st.descent, x = 0;
            for (size_t g = 0; g < l.st.glyphs.size(); ++g) {
                const ShapedGlyph& gl = l.st.glyphs[g];
                if (gl.flags & GF_OBJECT) {
                    const Run& run = pa.runs[gl.object];
                    Rect2 rc(x, 0, run.img_size.x, run.img_size.y);
                    int ta = run.img_align & IA_TEXT_MASK, ia = run.img_align & IA_IMAGE_MASK;
                    if (ta == IA_TO_TOP) rc.pos.y = -l.st.ascent;
                    else if (ta == IA_TO_CENTER) rc.pos.y = (-l.st.ascent + l.st.descent) / 2;
                    else if (ta == IA_TO_BOTTOM) rc.pos.y = l.st.descent;
                    if (ia == IA_BOTTOM_TO) rc.pos.y -= rc.size.y;
                    else if (ia == IA_CENTER_TO) rc.pos.y -= rc.size.y / 2;
                    full_asc = maxf(full_asc, -rc.pos.y);
                    full_desc = maxf(full_desc, rc.pos.y + rc.size.y);
                    l.objects.push_back(rc);
                    l.object_runs.push_back(gl.object);
                }
                x += gl.advance;
            }
            l.st.ascent = full_asc;
            l.st.descent = full_desc;
            pa.size.x = maxf(pa.size.x, l.st.width);
            pa.size.y += l.st.ascent + l.st.descent;
            pa.lines.push_back(l);
        }
        pa.offset_y = h;
        h = pa.offset_y + pa.size.y + ((int)pa.lines.size() - 1) * line_sep + para_sep;
    }
    laid_width_ = width;
}

float RichTextLabel::content_height() {
    StyleBoxRes* style = theme_stylebox("normal");
    Vec2 smin = style ? style->minimum_size() : Vec2();
    layout(size.x - smin.x);
    if (paras_->empty()) return 0;
    int line_sep = theme_constant("line_separation"), para_sep = theme_constant("paragraph_separation");
    const Para& last = paras_->back();
    float h = last.offset_y + last.size.y + ((int)last.lines.size() - 1) * line_sep;
    if (line_sep >= 0) h += para_sep;
    return (float)(int)h;
}

float RichTextLabel::content_width() {
    StyleBoxRes* style = theme_stylebox("normal");
    Vec2 smin = style ? style->minimum_size() : Vec2();
    layout(size.x - smin.x);
    float w = 0;
    for (size_t i = 0; i < paras_->size(); ++i) w = maxf(w, (*paras_)[i].size.x);
    return (float)(int)w;
}

Vec2 RichTextLabel::get_minimum_size() {
    StyleBoxRes* style = theme_stylebox("normal");
    Vec2 smin = style ? style->minimum_size() : Vec2();
    Vec2 m;
    if (fit_content) m = Vec2(content_width(), content_height());
    return smin + (autowrap != AUTOWRAP_OFF ? Vec2(1, m.y) : m);
}

/* ---- draw (_notification DRAW / _draw_line) ---- */

void RichTextLabel::draw(Canvas& c) {
    StyleBoxRes* style = theme_stylebox("normal");
    if (style) style->draw(c, Rect2(Vec2(0, 0), size));
    Vec2 smin = style ? style->minimum_size() : Vec2();
    Vec2 soff = style ? style->offset() : Vec2();
    Rect2 text_rect(soff, size - smin);
    layout(text_rect.size.x);
    if (paras_->empty()) return;
    int line_sep = theme_constant("line_separation"), para_sep = theme_constant("paragraph_separation");
    Color base_color = theme_color("default_color");
    Color outline_color_t = theme_color("font_outline_color");
    Color shadow_color = theme_color("font_shadow_color");
    int outline_t = theme_constant("outline_size");
    int shadow_outline = theme_constant("shadow_outline_size");
    Vec2 shadow_ofs((float)theme_constant("shadow_offset_x"), (float)theme_constant("shadow_offset_y"));
    float v_limit = style ? style->margin(SIDE_BOTTOM) : 0;

    float total_h = 1e30f;
    if (v_align != VALIGN_TOP) {
        const Para& last = paras_->back();
        total_h = last.offset_y + last.size.y + ((int)last.lines.size() - 1) * line_sep + (line_sep >= 0 ? para_sep : 0);
    }
    float vbegin = 0, vsep = 0;
    if (text_rect.size.y > total_h) {
        if (v_align == VALIGN_CENTER) vbegin = (text_rect.size.y - total_h) / 2;
        else if (v_align == VALIGN_BOTTOM) vbegin = text_rect.size.y - total_h;
        else if (v_align == VALIGN_FILL) {
            int n = 0;
            for (size_t p = 0; p < paras_->size(); ++p) n += (int)(*paras_)[p].lines.size();
            if (n > 1) vsep = (text_rect.size.y - total_h) / (n - 1);
        }
    }
    int total_glyphs = 0;
    for (size_t p = 0; p < paras_->size(); ++p)
        for (size_t l = 0; l < (*paras_)[p].lines.size(); ++l) total_glyphs += (int)(*paras_)[p].lines[l].st.glyphs.size();
    int visible_glyphs = visible_ratio < 1.0f ? (int)(total_glyphs * visible_ratio) : total_glyphs;
    int processed = 0;

    Vec2 ofs = text_rect.pos + Vec2(0, vbegin + (*paras_)[0].offset_y);
    for (size_t p = 0; p < paras_->size() && ofs.y < size.y - v_limit; ++p) {
        Para& pa = (*paras_)[p];
        float width = text_rect.size.x;
        Vec2 off;
        for (size_t li = 0; li < pa.lines.size(); ++li) {
            RtlLine& l = pa.lines[li];
            if (li > 0) off.y += line_sep + vsep;
            if (ofs.y + off.y >= size.y - v_limit) break;
            float length = l.st.width;
            off.x = 0;
            int al = pa.align >= 0 ? pa.align : h_align;
            if (al == HALIGN_CENTER) off.x += floorf((width - length) / 2.0f);
            else if (al == HALIGN_RIGHT) off.x += width - length;
            off.y += l.st.ascent;
            int line_processed = processed;
            for (int step = 0; step < 4; ++step) {
                /* 0 shadow outline, 1 shadow, 2 outline, 3 text */
                if (step == 3) {
                    for (size_t o = 0; o < l.objects.size(); ++o) {
                        const Run& run = pa.runs[l.object_runs[o]];
                        Rect2 rc = l.objects[o];
                        c.draw_texture_rect(run.image, Rect2(ofs + rc.pos + off, rc.size), false, run.img_color);
                    }
                }
                Vec2 pen = off;
                int k = line_processed;
                for (size_t g = 0; g < l.st.glyphs.size(); ++g, ++k) {
                    const ShapedGlyph& gl = l.st.glyphs[g];
                    if (k >= visible_glyphs) break;
                    const Run& run = pa.runs[pa.run_at[gl.start < (int)pa.run_at.size() ? gl.start : 0]];
                    Color font_color = run.st.has_color ? run.st.color : base_color;
                    Vec2 fx_off;
                    bool vis = true;
                    if (!run.st.fx.empty()) {
                        /* _fetch_item_fx_stack: innermost first */
                        RtlCharFx fx;
                        fx.color = font_color;
                        fx.elapsed_time = elapsed_time;
                        for (size_t e = run.st.fx.size(); e-- > 0;) {
                            RtlEffectFn fn = find_effect(run.st.fx[e]);
                            if (!fn) continue;
                            fx.relative_index = run.char_ofs + (gl.start - run.start) - run.st.fx_start[e];
                            fx.env = &run.st.fx_env[e];
                            fn(fx);
                        }
                        font_color = fx.color;
                        fx_off = Vec2(floorf(fx.offset.x + 0.5f), floorf(fx.offset.y + 0.5f));
                        vis = fx.visible;
                    }
                    if (vis && font_color.a != 0 && gl.face) {
                        Vec2 at = ofs + pen + fx_off;
                        if (step == 0 && shadow_color.a != 0 && shadow_outline > 0) {
                            Color sc = shadow_color * Color(1, 1, 1, font_color.a);
                            draw_glyph(c, gl, at + shadow_ofs, sc, shadow_outline);
                        } else if (step == 1 && shadow_color.a != 0) {
                            draw_glyph(c, gl, at + shadow_ofs, shadow_color * Color(1, 1, 1, font_color.a), 0);
                        } else if (step == 2) {
                            int os = run.st.outline_size >= 0 ? run.st.outline_size : outline_t;
                            Color oc = run.st.has_outline_color ? run.st.outline_color : outline_color_t;
                            if (os > 0 && oc.a != 0) draw_glyph(c, gl, at, oc * Color(1, 1, 1, font_color.a), os);
                        } else if (step == 3) {
                            draw_glyph(c, gl, at, font_color, 0);
                            if (run.st.underline || run.st.strike) {
                                float s = gl.face ? gl.font_size / gl.face->base_size : 1.0f;
                                float th = maxf(1.0f, gl.face ? gl.face->underline_thickness * s : 1.0f);
                                float y = run.st.underline ? (gl.face ? gl.face->underline_position * s : 2.0f)
                                                           : (-l.st.ascent + (l.st.ascent + l.st.descent) / 2);
                                Color uc = font_color;
                                uc.a *= (run.st.underline ? theme_constant("underline_alpha")
                                                          : theme_constant("strikethrough_alpha")) / 100.0f;
                                c.draw_rect(Rect2(ofs.x + pen.x, ofs.y + pen.y + y - th / 2, gl.advance, th), uc);
                            }
                        }
                    }
                    pen.x += gl.advance;
                }
                if (step == 3) processed = k;
            }
            off.y += l.st.descent;
        }
        ofs.y += pa.size.y + ((int)pa.lines.size() - 1) * (line_sep + vsep) + (para_sep + vsep);
    }
}

} /* namespace spire */

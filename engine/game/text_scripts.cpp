/* The game's text scripts: MegaLabel and MegaRichTextLabel (font size fitted
 * to the rect by binary search between MinFontSize and MaxFontSize) and the
 * custom RichTextEffects they install ([gold], [blue], [sine], ...).
 * Behaviour reimplemented from the game's addons/mega_text and
 * Core/RichTextTags; colours are StsColors. */
#include "game.h"

#include "../scene/rich_text.h"
#include "../scene/scene.h"
#include "../scene/script.h"
#include "../scene/text_nodes.h"
#include "../scene/value.h"

#include <math.h>
#include <stdlib.h>

namespace spire {

static const float PI_F = 3.14159265358979f;

static Color rgb(u32 v) { return Color(((v >> 16) & 255) / 255.0f, ((v >> 8) & 255) / 255.0f, (v & 255) / 255.0f, 1.0f); }

/* StsColors */
static const Color STS_AQUA = rgb(0x2AEBBE);
static const Color STS_GOLD = rgb(0xEFC851);
static const Color STS_RED = rgb(0xFF5555);
static const Color STS_GREEN = rgb(0x7FFF00);
static const Color STS_BLUE = rgb(0x87CEEB);
static const Color STS_ORANGE = rgb(0xFFA518);
static const Color STS_PINK = rgb(0xFF78A0);
static const Color STS_PURPLE = rgb(0xEE82EE);

/* ---- effects ---- */

static void env_color_visible(RtlCharFx& fx) {
    if (!fx.env) return;
    std::map<std::string, std::string>::const_iterator c = fx.env->find("color");
    if (c != fx.env->end()) rtl_parse_color(c->second, fx.color);
    std::map<std::string, std::string>::const_iterator v = fx.env->find("visible");
    fx.visible = v == fx.env->end() || v->second == "true" || v->second == "1";
}

static double env_num(const RtlCharFx& fx, const char* key, double fallback) {
    if (!fx.env) return fallback;
    std::map<std::string, std::string>::const_iterator it = fx.env->find(key);
    return it == fx.env->end() ? fallback : atof(it->second.c_str());
}

static void fx_aqua(RtlCharFx& fx) { fx.color = STS_AQUA; }
static void fx_blue(RtlCharFx& fx) { fx.color = STS_BLUE; }
static void fx_red(RtlCharFx& fx) { fx.color = STS_RED; }
static void fx_green(RtlCharFx& fx) { fx.color = STS_GREEN; }
static void fx_orange(RtlCharFx& fx) { fx.color = STS_ORANGE; }
static void fx_pink(RtlCharFx& fx) { fx.color = STS_PINK; }
static void fx_purple(RtlCharFx& fx) { fx.color = STS_PURPLE; }
static void fx_gold(RtlCharFx& fx) {
    if (fx.color == STS_GREEN) return; /* upgraded values stay green */
    fx.color = STS_GOLD;
}

static void fx_sine(RtlCharFx& fx) {
    float n = (float)(fx.elapsed_time * 1.5 + (float)fx.relative_index * 0.1f);
    fx.offset.y += 0.8f * sinf(n * PI_F * 2.0f * 0.5f);
    env_color_visible(fx);
}

/* FastNoiseLite stand-in: smooth value noise per character, same amplitude
 * (3 px) and rate; the exact noise pattern is not reproduced. */
static float smooth_noise(int seed, float x) {
    int i = (int)floorf(x);
    float f = x - i;
    unsigned a = (unsigned)(i * 374761393 + seed * 668265263), b = (unsigned)((i + 1) * 374761393 + seed * 668265263);
    a = (a ^ (a >> 13)) * 1274126177u;
    b = (b ^ (b >> 13)) * 1274126177u;
    float va = (a & 0xFFFF) / 32767.5f - 1.0f, vb = (b & 0xFFFF) / 32767.5f - 1.0f;
    float t = f * f * (3 - 2 * f);
    return va + (vb - va) * t;
}

static void fx_jitter(RtlCharFx& fx) {
    float t = (float)fx.elapsed_time * 600.0f * 0.01f;
    fx.offset = fx.offset + Vec2(smooth_noise((fx.relative_index + 1) * 131, t),
                                 smooth_noise((fx.relative_index + 1) * 737, t)) * 3.0f;
    env_color_visible(fx);
}

static void fx_fade_in(RtlCharFx& fx) {
    fx.offset = Vec2();
    double v = fx.elapsed_time * env_num(fx, "speed", 4.0) - fx.relative_index * env_num(fx, "tick", 0.01);
    fx.color.a = clampf((float)v, 0.0f, 1.0f);
    if (fx.env && fx.env->count("visible")) env_color_visible(fx);
    else fx.visible = true;
}

static void fx_fly_in(RtlCharFx& fx) {
    double v = fx.elapsed_time * 3.0 - fx.relative_index * 0.015f;
    fx.color.a = clampf((float)v, 0.0f, 1.0f);
    float e = 1.0f - (1.0f - fx.color.a) * (1.0f - fx.color.a); /* QuadOut */
    Vec2 from((float)env_num(fx, "x", 0), (float)env_num(fx, "y", 0));
    fx.offset = fx.offset + from * (1.0f - e);
}

static void fx_thinky_dots(RtlCharFx& fx) {
    fx.offset = Vec2();
    float v = (float)(fx.elapsed_time * 1.0 - (float)fx.relative_index * 0.1f);
    if (v < 0) v = 0;
    float n = fmodf(v, 4.4f);
    float h = n < 0.4f ? 1.5f * sinf(n / 0.4f * PI_F) : 0.0f;
    fx.offset.y -= h > 0 ? h : 0;
    env_color_visible(fx);
}

static void fx_scramble(RtlCharFx& fx) { env_color_visible(fx); /* glyph swapping: not ported yet */ }

static const char* const MEGA_EFFECTS[] = { "aqua", "blue", "fade_in", "fly_in", "gold", "green", "jitter",
                                            "orange", "pink", "purple", "red", "scramble", "sine", "thinky_dots",
                                            NULL };

/* ---- MegaLabelHelper.EstimateTextSize ---- */

struct TextSeg {
    bool image;
    std::vector<u32> text;
    Vec2 size;
};

/* A TextParagraph of the segments: break flags MANDATORY|WORD_BOUND when
 * wrapping (no trimming), size = lines + (line_spacing + 3) per extra line. */
static Vec2 estimate_text_size(const std::vector<TextSeg>& segs, const FontRes* font, int size, float max_width,
                               bool wrap, float line_spacing) {
    std::vector<u32> cps;
    for (size_t i = 0; i < segs.size(); ++i) {
        if (segs[i].image) cps.push_back(0xFFFC);
        else cps.insert(cps.end(), segs[i].text.begin(), segs[i].text.end());
    }
    ShapedText st;
    int pos = 0;
    for (size_t i = 0; i < segs.size(); ++i) {
        if (segs[i].image) {
            shape_object(st, pos, segs[i].size.x, (int)i);
            ++pos;
        } else {
            int n = (int)segs[i].text.size();
            shape_run(cps, pos, pos + n, font, size, pos + n == (int)cps.size(), st);
            pos += n;
        }
    }
    shape_finish(cps, st);
    std::vector<int> br;
    line_breaks(st, wrap ? max_width : 0.0f, wrap ? (BREAK_MANDATORY | BREAK_WORD_BOUND) : 0, br);
    Vec2 out;
    int lines = 0;
    for (size_t i = 0; i + 1 < br.size(); i += 2) {
        ShapedText l;
        shaped_substr(st, br[i], br[i + 1], l);
        float asc = l.ascent, desc = l.descent;
        for (size_t g = 0; g < l.glyphs.size(); ++g)
            if (l.glyphs[g].flags & GF_OBJECT) {
                /* InlineAlignment 5: centred on the text's centre */
                float h = segs[l.glyphs[g].object].size.y;
                float y = (-l.ascent + l.descent) / 2 - h / 2;
                asc = maxf(asc, -y);
                desc = maxf(desc, y + h);
            }
        out.x = maxf(out.x, l.width);
        out.y += asc + desc;
        ++lines;
    }
    out.y += (line_spacing + 3.0f) * (lines - 1);
    return out;
}

/* ---- MegaLabel ---- */

class MegaLabelScript : public ScriptBehavior {
public:
    MegaLabelScript(Label* l)
        : label_(l), enabled_(true), min_(8), max_(100), last_set_(0), adjusted_(false) {
        const Json* v;
        if ((v = l->get_extra("AutoSizeEnabled"))) enabled_ = v->b(true);
        if ((v = l->get_extra("MinFontSize"))) min_ = v->i(8);
        if ((v = l->get_extra("MaxFontSize"))) max_ = v->i(100);
    }
    virtual void ready(Node*) { adjust(); }
    virtual void resized(Control*) {
        /* SetTextAutoSize: a new text is fitted again too. */
        if (!adjusted_ || fabsf(last_size_.x - label_->size.x) > 0.01f || fabsf(last_size_.y - label_->size.y) > 0.01f ||
            last_text_ != label_->text)
            adjust();
    }

private:
    bool too_big(int size) {
        std::vector<TextSeg> segs(1);
        segs[0].image = false;
        utf8_decode(label_->text, segs[0].text);
        Vec2 e = estimate_text_size(segs, label_->theme_font("font"), size, label_->size.x, label_->autowrap != 0,
                                    (float)label_->theme_constant("line_spacing"));
        return e.x > label_->size.x || e.y > label_->size.y;
    }
    void set_size(int s) {
        if (last_set_ == s) return;
        last_set_ = s;
        size_json_ = Json::make_number(s);
        label_->override_values["font_sizes/font_size"] = &size_json_;
    }
    void adjust() {
        if (!enabled_) return;
        adjusted_ = true;
        last_size_ = label_->size;
        last_text_ = label_->text;
        if (!too_big(max_)) {
            set_size(max_);
            return;
        }
        if (last_set_ >= min_ && last_set_ < max_ && !too_big(last_set_) && too_big(last_set_ + 1)) return;
        int lo = min_, hi = max_;
        while (hi >= lo) {
            int mid = lo + (hi - lo) / 2;
            if (mid == max_ || too_big(mid)) hi = mid - 1;
            else lo = mid + 1;
        }
        set_size(lo < hi ? lo : hi);
    }
    Label* label_;
    bool enabled_;
    int min_, max_, last_set_;
    bool adjusted_;
    Vec2 last_size_;
    std::string last_text_;
    Json size_json_;
};

static ScriptBehavior* make_mega_label(Node* n) {
    return n->type == "Label" || godot_is_a(n->type, "Label") ? new MegaLabelScript((Label*)n) : NULL;
}

/* ---- MegaRichTextLabel ---- */

class MegaRichTextScript : public ScriptBehavior {
public:
    MegaRichTextScript(RichTextLabel* l)
        : rtl_(l), enabled_(true), min_(8), max_(100), last_set_(0), vbound_(true), hbound_(false), adjusted_(false) {
        const Json* v;
        if ((v = l->get_extra("AutoSizeEnabled"))) enabled_ = v->b(true);
        if ((v = l->get_extra("MinFontSize"))) min_ = v->i(8);
        if ((v = l->get_extra("MaxFontSize"))) max_ = v->i(100);
        if ((v = l->get_extra("IsVerticallyBound"))) vbound_ = v->b(true);
        if ((v = l->get_extra("IsHorizontallyBound"))) hbound_ = v->b(false);
        if (l->fit_content) enabled_ = false; /* "not compatible with Fit Content" */
    }
    virtual void ready(Node*) {
        if (rtl_->bbcode) {
            rtl_->custom_effects.clear();
            for (int i = 0; MEGA_EFFECTS[i]; ++i) rtl_->custom_effects.push_back(MEGA_EFFECTS[i]);
            rtl_->invalidate();
        }
        adjust();
    }
    virtual void resized(Control*) {
        if (!enabled_) return;
        if (!adjusted_ || fabsf(last_size_.x - rtl_->size.x) > 0.01f || fabsf(last_size_.y - rtl_->size.y) > 0.01f ||
            last_text_ != rtl_->text)
            adjust();
    }

private:
    /* MegaLabelHelper.ParseBbcode: text outside tags, [img] contents as objects. */
    void segments(std::vector<TextSeg>& out) {
        out.clear();
        std::vector<u32> t;
        utf8_decode(rtl_->text, t);
        std::vector<std::string> stack;
        std::vector<u32> cur;
        size_t i = 0;
        while (i < t.size()) {
            if (t[i] == '[') {
                size_t e = i + 1;
                while (e < t.size() && t[e] != ']') ++e;
                std::string tag;
                for (size_t k = i + 1; k < e; ++k) tag += (char)(t[k] < 128 ? t[k] : '?');
                size_t sp = tag.find_first_of(" =");
                std::string name = tag.substr(0, sp);
                if (name == "lb" || name == "rb") {
                    cur.push_back(name == "lb" ? '[' : ']');
                } else {
                    flush(out, cur, stack);
                    if (!name.empty() && name[0] == '/') {
                        if (!stack.empty()) stack.pop_back();
                    } else {
                        stack.push_back(name);
                    }
                }
                i = e + 1;
            } else {
                cur.push_back(t[i++]);
            }
        }
        flush(out, cur, stack);
    }
    void flush(std::vector<TextSeg>& out, std::vector<u32>& cur, const std::vector<std::string>& stack) {
        if (cur.empty()) return;
        TextSeg s;
        if (!stack.empty() && stack.back() == "img") {
            s.image = true;
            std::string path;
            for (size_t k = 0; k < cur.size(); ++k) path += (char)cur[k];
            if (path.compare(0, 6, "res://") == 0) path = path.substr(6);
            SceneLibrary* lib = SceneLibrary::current();
            Resource* r = lib ? lib->ext(path) : NULL;
            if (r && r->is_texture()) s.size = ((Texture2DRes*)r)->size();
        } else {
            s.image = false;
            s.text = cur;
        }
        out.push_back(s);
        cur.clear();
    }
    bool too_big(const std::vector<TextSeg>& segs, int size) {
        Vec2 e = estimate_text_size(segs, rtl_->theme_font("normal_font"), size, rtl_->size.x, true,
                                    (float)rtl_->theme_constant("line_separation"));
        return (e.x > rtl_->size.x && hbound_) || (e.y > rtl_->size.y && vbound_);
    }
    void set_size(int s) {
        if (last_set_ == s) return;
        last_set_ = s;
        size_json_ = Json::make_number(s);
        rtl_->override_values["font_sizes/normal_font_size"] = &size_json_;
        if (rtl_->bbcode) {
            rtl_->override_values["font_sizes/bold_font_size"] = &size_json_;
            rtl_->override_values["font_sizes/bold_italics_font_size"] = &size_json_;
            rtl_->override_values["font_sizes/italics_font_size"] = &size_json_;
            rtl_->override_values["font_sizes/mono_font_size"] = &size_json_;
        }
        rtl_->invalidate();
    }
    void adjust() {
        if (!enabled_) return;
        adjusted_ = true;
        last_size_ = rtl_->size;
        last_text_ = rtl_->text;
        std::vector<TextSeg> segs;
        segments(segs);
        if (!too_big(segs, max_)) {
            set_size(max_);
            return;
        }
        if (last_set_ >= min_ && last_set_ < max_ && !too_big(segs, last_set_) && too_big(segs, last_set_ + 1)) return;
        int lo = min_, hi = max_;
        while (hi >= lo) {
            int mid = lo + (hi - lo) / 2;
            if (too_big(segs, mid)) hi = mid - 1;
            else lo = mid + 1;
        }
        set_size(lo < hi ? lo : hi);
    }
    RichTextLabel* rtl_;
    bool enabled_;
    int min_, max_, last_set_;
    bool vbound_, hbound_, adjusted_;
    Vec2 last_size_;
    std::string last_text_;
    Json size_json_;
};

static ScriptBehavior* make_mega_rtl(Node* n) {
    return n->type == "RichTextLabel" ? new MegaRichTextScript((RichTextLabel*)n) : NULL;
}

void install_text_scripts() {
    register_rtl_effect("aqua", fx_aqua);
    register_rtl_effect("blue", fx_blue);
    register_rtl_effect("red", fx_red);
    register_rtl_effect("green", fx_green);
    register_rtl_effect("orange", fx_orange);
    register_rtl_effect("pink", fx_pink);
    register_rtl_effect("purple", fx_purple);
    register_rtl_effect("gold", fx_gold);
    register_rtl_effect("sine", fx_sine);
    register_rtl_effect("jitter", fx_jitter);
    register_rtl_effect("fade_in", fx_fade_in);
    register_rtl_effect("fly_in", fx_fly_in);
    register_rtl_effect("thinky_dots", fx_thinky_dots);
    register_rtl_effect("scramble", fx_scramble);
    register_script("res://addons/mega_text/MegaLabel.cs", make_mega_label);
    register_script("res://addons/mega_text/MegaRichTextLabel.cs", make_mega_rtl);
}

} /* namespace spire */

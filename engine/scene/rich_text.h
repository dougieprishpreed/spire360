/* RichTextLabel, ported from Godot 4.5.1 scene/gui/rich_text_label.cpp and
 * scene/resources/text_paragraph.cpp (MIT, see THIRD_PARTY.md): BBCode
 * (b, i, code, u, s, color, font, font_size, outline_size, outline_color,
 * center/left/right/fill/p, indent, img, url, lb/rb, built-in effects as
 * plain text), paragraphs of shaped spans with inline images, line breaking,
 * alignment, and the shadow / outline / text draw passes. Custom effects
 * (RichTextEffect scripts) are C++ functions registered by name. Not ported:
 * tables, lists, dropcaps, selection, scrolling (content is assumed to fit). */
#ifndef SPIRE_RICH_TEXT_H
#define SPIRE_RICH_TEXT_H

#include "../text/shaper.h"
#include "node.h"
#include "text_nodes.h"

namespace spire {

class Texture2DRes;

/* Godot's CharFXTransform, the part custom effects use. */
struct RtlCharFx {
    Color color;
    Vec2 offset;
    bool visible;
    int relative_index; /* character index within the effect's span */
    double elapsed_time;
    const std::map<std::string, std::string>* env; /* tag options */
    RtlCharFx() : visible(true), relative_index(0), elapsed_time(0), env(NULL) {}
};
typedef void (*RtlEffectFn)(RtlCharFx& fx);
void register_rtl_effect(const char* name, RtlEffectFn fn);
/* PrefsSave.TextEffectsEnabled: off, the effects that ask
 * AbstractMegaRichTextEffect.ShouldTransformText (sine, fly_in, scramble,
 * thinky_dots, ancient_banner) leave the text as it is. */
void set_rtl_text_effects(bool on);
/* Color::from_string: #rgb[a], #rrggbb[aa] or a name. */
bool rtl_parse_color(const std::string& s, Color& out);

class RichTextLabel : public Control {
public:
    RichTextLabel();
    ~RichTextLabel();
    std::string text;
    bool bbcode, fit_content, scroll_active;
    int autowrap, trim_flags, h_align, v_align;
    int visible_characters;
    float visible_ratio;
    double elapsed_time;                    /* for animated effects */
    std::vector<std::string> custom_effects; /* installed effect names */

    virtual bool set(const std::string& key, const Json& v, const PropCtx& ctx);
    virtual Vec2 get_minimum_size();
    virtual void draw(Canvas& c);
    /* NOTIFICATION_INTERNAL_PROCESS: the effects' clock. */
    virtual void process(double dt) { elapsed_time += dt; }

    float content_height();
    float content_width();
    void invalidate() { laid_width_ = -1; }

    struct Style;
    struct Run;
    struct Para;

private:
    void parse();
    void layout(float width);
    std::vector<Para>* paras_;
    std::string parsed_text_;
    std::string parsed_key_;
    float laid_width_;
};

} /* namespace spire */

#endif

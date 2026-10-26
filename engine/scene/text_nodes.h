/* Label, ported from Godot 4.5.1 scene/gui/label.cpp (MIT, see
 * THIRD_PARTY.md). Shaping is engine/text; RichTextLabel is rich_text.h. */
#ifndef SPIRE_TEXT_NODES_H
#define SPIRE_TEXT_NODES_H

#include "../text/shaper.h"
#include "node.h"

namespace spire {

enum { AUTOWRAP_OFF, AUTOWRAP_ARBITRARY, AUTOWRAP_WORD, AUTOWRAP_WORD_SMART };
enum { HALIGN_LEFT, HALIGN_CENTER, HALIGN_RIGHT, HALIGN_FILL };
enum { VALIGN_TOP, VALIGN_CENTER, VALIGN_BOTTOM, VALIGN_FILL };

class Label : public Control {
public:
    Label();
    std::string text;
    int h_align, v_align, autowrap, trim_flags, overrun;
    bool clip_text, uppercase;
    int visible_characters, max_lines_visible, lines_skipped;
    float visible_ratio;
    virtual bool set(const std::string& key, const Json& v, const PropCtx& ctx);
    virtual Vec2 get_minimum_size();
    virtual void draw(Canvas& c);
    virtual bool clips() const { return clip_contents || clip_text; }

    void set_text(const std::string& t) { text = t; }
    int line_count();

private:
    struct Para {
        std::vector<u32> cps;
        ShapedText shaped;
        std::vector<ShapedText> lines;
    };
    void shape();
    float line_height(const ShapedText& l, float font_h) const;
    std::vector<Para> paras_;
    Vec2 minsize_;
    FontRes* font_;
    int font_size_;
    /* shape()'s inputs when it last ran: the lines are shaped again only
     * when one of them changes (draw, the minimum size and the line count
     * ask several times a frame) */
    std::string shaped_text_;
    FontRes* shaped_font_;
    int shaped_size_, shaped_autowrap_, shaped_trim_, shaped_visible_, shaped_max_lines_, shaped_skipped_;
    int shaped_line_sp_, shaped_para_sp_;
    float shaped_width_, shaped_ratio_;
    bool shaped_upper_, shaped_valid_;
};

} /* namespace spire */

#endif

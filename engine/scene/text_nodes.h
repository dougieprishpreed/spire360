/* Label and RichTextLabel. Shaping and drawing are in engine/text. */
#ifndef SPIRE_TEXT_NODES_H
#define SPIRE_TEXT_NODES_H

#include "node.h"

namespace spire {

class Label : public Control {
public:
    Label() : h_align(0), v_align(0), autowrap(0), clip_text(false), uppercase(false), visible_ratio(1.0f) {}
    std::string text;
    int h_align, v_align, autowrap;
    bool clip_text, uppercase;
    float visible_ratio;
    virtual bool set(const std::string& key, const Json& v, const PropCtx& ctx);
};

class RichTextLabel : public Control {
public:
    RichTextLabel() : bbcode(false), fit_content(false), scroll_active(true), autowrap(3), h_align(0), v_align(0) {}
    std::string text;
    bool bbcode, fit_content, scroll_active;
    int autowrap, h_align, v_align;
    virtual bool set(const std::string& key, const Json& v, const PropCtx& ctx);
};

} /* namespace spire */

#endif

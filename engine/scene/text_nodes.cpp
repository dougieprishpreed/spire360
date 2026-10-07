#include "text_nodes.h"

#include "value.h"

namespace spire {

bool Label::set(const std::string& key, const Json& v, const PropCtx& ctx) {
    if (key == "text") text = v.str();
    else if (key == "horizontal_alignment") h_align = v.i();
    else if (key == "vertical_alignment") v_align = v.i();
    else if (key == "autowrap_mode") autowrap = v.i();
    else if (key == "clip_text") clip_text = v.b();
    else if (key == "uppercase") uppercase = v.b();
    else if (key == "visible_ratio") visible_ratio = jv_float(v, 1.0f);
    else return Control::set(key, v, ctx);
    return true;
}

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

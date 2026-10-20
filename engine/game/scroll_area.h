/* NScrollableContainer with its NScrollbar (src/Core/Nodes/GodotExtensions):
 * the container's Content (or Mask/Content) slides toward its target at
 * 15 / s, the target eased back within the limits at 12 / s; a focused
 * control is brought to the middle of the view (ProcessGuiFocus); the
 * scrollbar shows when the content is taller than its view, its %Handle
 * following the value (MathHelper.SmoothDamp, 0.05 s). */
#ifndef SPIRE_SCROLL_AREA_H
#define SPIRE_SCROLL_AREA_H

#include "../core/base.h"

namespace spire {

class Control;
class Node;

class ScrollArea {
public:
    ScrollArea();
    /* screen_contents: the NScrollableContainer node. */
    void attach(Node* screen_contents);
    /* InstantlyScrollToTop */
    void to_top();
    /* ProcessGuiFocus: a control inside the content took the focus. */
    void focus(Control* c);
    /* ProcessControllerEvent: up / down with nothing focused (+400 / -400). */
    void scroll_by(float dy) { target_ += dy; }
    void frame(double dt);

private:
    float limit_bottom() const;
    Control* content_;
    Control* bar_;
    Control* handle_;
    float target_, velocity_, value_;
};

} /* namespace spire */

#endif

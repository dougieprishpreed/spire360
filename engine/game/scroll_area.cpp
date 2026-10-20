#include "scroll_area.h"

#include "../scene/node.h"

#include <math.h>

namespace spire {

static Control* control_of(Node* n) { return n && n->is_control() ? (Control*)n : NULL; }

/* Control.Position = p: the offsets moved together (the rect keeps its
 * size, laid out or not). */
static void move_to(Control* c, const Vec2& p) {
    float dx = p.x - c->position.x, dy = p.y - c->position.y;
    c->offset[SIDE_LEFT] += dx;
    c->offset[SIDE_RIGHT] += dx;
    c->offset[SIDE_TOP] += dy;
    c->offset[SIDE_BOTTOM] += dy;
    c->position = p;
}

ScrollArea::ScrollArea() : content_(NULL), bar_(NULL), handle_(NULL), target_(0), velocity_(0), value_(0), disable_if_fits_(false) {}

void ScrollArea::attach(Node* screen_contents) {
    content_ = NULL;
    bar_ = handle_ = NULL;
    if (!screen_contents) return;
    content_ = control_of(screen_contents->get_node("Content"));
    if (!content_) content_ = control_of(screen_contents->get_node("Mask/Content"));
    bar_ = control_of(screen_contents->get_node("Scrollbar"));
    handle_ = bar_ ? control_of(bar_->find_unique("Handle")) : NULL;
    if (bar_) bar_->visible = false;
    target_ = velocity_ = value_ = 0;
}

float ScrollArea::limit_bottom() const {
    Control* view = content_ && content_->parent && content_->parent->is_control() ? (Control*)content_->parent : NULL;
    return content_ && view ? -content_->size.y + view->size.y : 0.0f;
}

void ScrollArea::to_top() {
    target_ = 0;
    value_ = 0;
    if (content_) move_to(content_, Vec2(content_->position.x, 0));
    velocity_ = 0;
}

void ScrollArea::focus(Control* c) {
    if (!content_ || !c) return;
    Control* view = content_->parent && content_->parent->is_control() ? (Control*)content_->parent : NULL;
    if (!view) return;
    float v = content_->global_transform().origin.y - c->global_transform().origin.y + view->size.y * 0.5f;
    float lim = limit_bottom();
    float hi = lim > 0 ? lim : 0, lo = lim < 0 ? lim : 0;
    target_ = v < lo ? lo : (v > hi ? hi : v);
}

/* MathHelper.SmoothDamp */
static float smooth_damp(float current, float target, float* velocity, float smooth_time, float dt) {
    if (dt <= 0) return current;
    smooth_time = smooth_time > 0.0001f ? smooth_time : 0.0001f;
    float omega = 2.0f / smooth_time, x = omega * dt;
    float e = 1.0f / (1.0f + x + 0.48f * x * x + 0.235f * x * x * x);
    float change = current - target, original = target;
    target = current - change;
    float temp = (*velocity + omega * change) * dt;
    *velocity = (*velocity - omega * temp) * e;
    float out = target + (change + temp) * e;
    if ((original - current > 0.0f) == (out > original)) {
        out = original;
        *velocity = (out - original) / dt;
    }
    return out;
}

void ScrollArea::frame(double dt) {
    if (!content_) return;
    Control* view = content_->parent && content_->parent->is_control() ? (Control*)content_->parent : NULL;
    float lim = limit_bottom();
    bool fits = !(view && content_->size.y > view->size.y);
    if (bar_) bar_->visible = !fits;
    /* _Process: no scrolling when the content fits, if so asked */
    if (disable_if_fits_ && fits) return;
    /* UpdateScrollPosition */
    float y = content_->position.y;
    if (fabsf(y - target_) > 1e-4f) {
        float a = y - target_ > 0 ? 1.0f : -1.0f;
        float k = (float)dt * 15.0f;
        float ny = y + (target_ - y) * k;
        float b = ny - target_ > 0 ? 1.0f : (ny - target_ < 0 ? -1.0f : 0.0f);
        if (fabsf(ny - target_) < 0.5f || a != b) ny = target_;
        move_to(content_, Vec2(content_->position.x, ny));
        if (lim < 0) {
            float v = ny / lim;
            value_ = (v < 0 ? 0 : (v > 1 ? 1 : v)) * 100.0f;
        }
    }
    float lo = lim < 0 ? lim : 0, hi = lim > 0 ? lim : 0;
    if (target_ < lo) target_ += (lim - target_) * (float)dt * 12.0f;
    else if (target_ > hi) target_ += (0 - target_) * (float)dt * 12.0f;
    /* NScrollbar.UpdateHandlePosition */
    if (bar_ && handle_ && bar_->visible) {
        float hy = smooth_damp(handle_->position.y, bar_->size.y * value_ / 100.0f - handle_->size.y * 0.5f, &velocity_, 0.05f,
                               (float)dt);
        move_to(handle_, Vec2((bar_->size.x - handle_->size.x) * 0.5f, hy));
    }
}

} /* namespace spire */

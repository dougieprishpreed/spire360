#include "tween.h"

#include <math.h>

namespace spire {

static const float PI_F = 3.14159265358979f;

/* scene/animation/easing_equations.h */
static float sine(TweenEase e, float t, float b, float c, float d) {
    if (e == EASE_IN) return -c * cosf(t / d * (PI_F / 2)) + c + b;
    if (e == EASE_OUT) return c * sinf(t / d * (PI_F / 2)) + b;
    return -c / 2 * (cosf(PI_F * t / d) - 1) + b;
}

static float expo(TweenEase e, float t, float b, float c, float d) {
    if (e == EASE_IN) {
        if (t == 0) return b;
        return c * powf(2, 10 * (t / d - 1)) + b - c * 0.001f;
    }
    if (e == EASE_OUT) {
        if (t == d) return b + c;
        return c * 1.001f * (-powf(2, -10 * t / d) + 1) + b;
    }
    if (t == 0) return b;
    if (t == d) return b + c;
    t = t / d * 2;
    if (t < 1) return c / 2 * powf(2, 10 * (t - 1)) + b - c * 0.0005f;
    return c / 2 * 1.0005f * (-powf(2, -10 * (t - 1)) + 2) + b;
}

static float elastic(TweenEase e, float t, float b, float c, float d) {
    if (e == EASE_IN) {
        if (t == 0) return b;
        t /= d;
        if (t == 1) return b + c;
        t -= 1;
        float p = d * 0.3f, a = c * powf(2, 10 * t), s = p / 4;
        return -(a * sinf((t * d - s) * (2 * PI_F) / p)) + b;
    }
    if (e == EASE_OUT) {
        if (t == 0) return b;
        t /= d;
        if (t == 1) return b + c;
        float p = d * 0.3f, s = p / 4;
        return c * powf(2, -10 * t) * sinf((t * d - s) * (2 * PI_F) / p) + c + b;
    }
    if (t == 0) return b;
    if ((t /= d / 2) == 2) return b + c;
    float p = d * (0.3f * 1.5f), s = p / 4;
    if (t < 1) {
        t -= 1;
        float a = c * powf(2, 10 * t);
        return -0.5f * (a * sinf((t * d - s) * (2 * PI_F) / p)) + b;
    }
    t -= 1;
    float a = c * powf(2, -10 * t);
    return a * sinf((t * d - s) * (2 * PI_F) / p) * 0.5f + c + b;
}

float tween_interpolate(TweenTrans trans, TweenEase ease, float t, float b, float c, float d) {
    if (d <= 0) return b + c;
    switch (trans) {
    case TRANS_SINE: return sine(ease, t, b, c, d);
    case TRANS_EXPO: return expo(ease, t, b, c, d);
    case TRANS_ELASTIC: return elastic(ease, t, b, c, d);
    default: return c * t / d + b;
    }
}

void TweenSet::tween(float* p, float to, float seconds, TweenTrans trans, TweenEase ease) {
    tween_from(p, *p, to, seconds, trans, ease);
}

void TweenSet::tween_from(float* p, float from, float to, float seconds, TweenTrans trans, TweenEase ease) {
    kill(p);
    Item it;
    it.p = p;
    it.from = from;
    it.to = to;
    it.seconds = seconds;
    it.t = 0;
    it.trans = trans;
    it.ease = ease;
    *p = from;
    items_.push_back(it);
}

void TweenSet::tween(Vec2* p, const Vec2& to, float seconds, TweenTrans trans, TweenEase ease) {
    tween_from(p, *p, to, seconds, trans, ease);
}

void TweenSet::tween_from(Vec2* p, const Vec2& from, const Vec2& to, float seconds, TweenTrans trans, TweenEase ease) {
    tween_from(&p->x, from.x, to.x, seconds, trans, ease);
    tween_from(&p->y, from.y, to.y, seconds, trans, ease);
}

void TweenSet::kill(float* p) {
    for (size_t i = 0; i < items_.size(); ++i)
        if (items_[i].p == p) {
            items_.erase(items_.begin() + (long)i);
            return;
        }
}

void TweenSet::kill(Vec2* p) {
    kill(&p->x);
    kill(&p->y);
}

void TweenSet::update(double dt) {
    for (size_t i = 0; i < items_.size();) {
        Item& it = items_[i];
        it.t += (float)dt;
        if (it.t >= it.seconds) {
            *it.p = it.to;
            items_.erase(items_.begin() + (long)i);
            continue;
        }
        *it.p = tween_interpolate(it.trans, it.ease, it.t, it.from, it.to - it.from, it.seconds);
        ++i;
    }
}

} /* namespace spire */

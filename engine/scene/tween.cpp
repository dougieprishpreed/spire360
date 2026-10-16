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

static float back(TweenEase e, float t, float b, float c, float d) {
    const float s = 1.70158f;
    if (e == EASE_IN) {
        t /= d;
        return c * t * t * ((s + 1) * t - s) + b;
    }
    if (e == EASE_OUT) {
        t = t / d - 1;
        return c * (t * t * ((s + 1) * t + s) + 1) + b;
    }
    const float s2 = s * 1.525f;
    t /= d / 2;
    if (t < 1) return c / 2 * (t * t * ((s2 + 1) * t - s2)) + b;
    t -= 2;
    return c / 2 * (t * t * ((s2 + 1) * t + s2) + 2) + b;
}

static float cubic(TweenEase e, float t, float b, float c, float d) {
    if (e == EASE_IN) {
        t /= d;
        return c * t * t * t + b;
    }
    if (e == EASE_OUT) {
        t = t / d - 1;
        return c * (t * t * t + 1) + b;
    }
    t /= d / 2;
    if (t < 1) return c / 2 * t * t * t + b;
    t -= 2;
    return c / 2 * (t * t * t + 2) + b;
}

static float circ(TweenEase e, float t, float b, float c, float d) {
    if (e == EASE_IN) {
        t /= d;
        return -c * (sqrtf(1 - t * t) - 1) + b;
    }
    if (e == EASE_OUT) {
        t = t / d - 1;
        return c * sqrtf(1 - t * t) + b;
    }
    t /= d / 2;
    if (t < 1) return -c / 2 * (sqrtf(1 - t * t) - 1) + b;
    t -= 2;
    return c / 2 * (sqrtf(1 - t * t) + 1) + b;
}

static float quart(TweenEase e, float t, float b, float c, float d) {
    if (e == EASE_IN) return c * powf(t / d, 4) + b;
    if (e == EASE_OUT) return -c * (powf(t / d - 1, 4) - 1) + b;
    t = t / d * 2;
    if (t < 1) return c / 2 * powf(t, 4) + b;
    return -c / 2 * (powf(t - 2, 4) - 2) + b;
}

static float spring_out(float t, float b, float c, float d) {
    t /= d;
    float s = 1.0f - t;
    t = (sinf(t * 3.14159265f * (0.2f + 2.5f * t * t * t)) * powf(s, 2.2f) + t) * (1.0f + (1.2f * s));
    return c * t + b;
}

static float spring(TweenEase e, float t, float b, float c, float d) {
    if (e == EASE_OUT) return spring_out(t, b, c, d);
    if (e == EASE_IN) return c - spring_out(d - t, 0, c, d) + b;
    if (t < d / 2) return c / 2 - spring_out(d - t * 2, 0, c / 2, d) + b;
    return spring_out(t * 2 - d, b + c / 2, c / 2, d);
}

float tween_interpolate(TweenTrans trans, TweenEase ease, float t, float b, float c, float d) {
    if (d <= 0) return b + c;
    switch (trans) {
    case TRANS_SINE: return sine(ease, t, b, c, d);
    case TRANS_EXPO: return expo(ease, t, b, c, d);
    case TRANS_ELASTIC: return elastic(ease, t, b, c, d);
    case TRANS_BACK: return back(ease, t, b, c, d);
    case TRANS_CUBIC: return cubic(ease, t, b, c, d);
    case TRANS_CIRC: return circ(ease, t, b, c, d);
    case TRANS_QUART: return quart(ease, t, b, c, d);
    case TRANS_SPRING: return spring(ease, t, b, c, d);
    default: return c * t / d + b;
    }
}

void TweenSet::add(float* p, float from, bool from_current, float to, float seconds, TweenTrans trans,
                   TweenEase ease, float delay) {
    kill(p);
    Item it;
    it.p = p;
    it.from = from;
    it.from_current = from_current;
    it.to = to;
    it.seconds = seconds;
    it.t = -delay;
    it.trans = trans;
    it.ease = ease;
    if (delay <= 0 && !from_current) *p = from;
    items_.push_back(it);
}

void TweenSet::tween(float* p, float to, float seconds, TweenTrans trans, TweenEase ease, float delay) {
    add(p, *p, delay > 0, to, seconds, trans, ease, delay);
}

void TweenSet::tween_from(float* p, float from, float to, float seconds, TweenTrans trans, TweenEase ease,
                          float delay) {
    add(p, from, false, to, seconds, trans, ease, delay);
}

void TweenSet::tween(Vec2* p, const Vec2& to, float seconds, TweenTrans trans, TweenEase ease, float delay) {
    tween(&p->x, to.x, seconds, trans, ease, delay);
    tween(&p->y, to.y, seconds, trans, ease, delay);
}

void TweenSet::tween_from(Vec2* p, const Vec2& from, const Vec2& to, float seconds, TweenTrans trans, TweenEase ease,
                          float delay) {
    tween_from(&p->x, from.x, to.x, seconds, trans, ease, delay);
    tween_from(&p->y, from.y, to.y, seconds, trans, ease, delay);
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
        bool waiting = it.t < 0;
        it.t += (float)dt;
        if (it.t < 0) {
            ++i;
            continue;
        }
        if (waiting) {
            /* Started this frame: its start value is taken now. */
            if (it.from_current) it.from = *it.p;
            it.t = 0;
        }
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

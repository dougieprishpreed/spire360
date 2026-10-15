/* Property tweens, as the game's scripts make them with Godot's Tween:
 * a float (or each component of a Vec2) moves from its value to a target
 * over a time, with Godot's transition and ease curves. A new tween on the
 * same property replaces the running one (the scripts' _tween?.Kill()). The
 * owner calls update() each frame and keeps the properties alive while
 * their tweens run (clear() when it drops them). Ported from Godot 4.5.1
 * (scene/animation/easing_equations.h; MIT, see THIRD_PARTY.md). */
#ifndef SPIRE_TWEEN_H
#define SPIRE_TWEEN_H

#include "../core/base.h"

namespace spire {

enum TweenTrans { TRANS_LINEAR, TRANS_SINE, TRANS_EXPO, TRANS_ELASTIC };
enum TweenEase { EASE_IN, EASE_OUT, EASE_IN_OUT };

/* Godot's Tween.interpolate_value: t in [0, d] seconds, from b by c. */
float tween_interpolate(TweenTrans trans, TweenEase ease, float t, float b, float c, float d);

class TweenSet {
public:
    /* Tweens *p to `to` over `seconds` (from its current value). */
    void tween(float* p, float to, float seconds, TweenTrans trans = TRANS_LINEAR, TweenEase ease = EASE_IN_OUT);
    /* The same from a given start (Godot's .From(v)). */
    void tween_from(float* p, float from, float to, float seconds, TweenTrans trans = TRANS_LINEAR,
                    TweenEase ease = EASE_IN_OUT);
    void tween(Vec2* p, const Vec2& to, float seconds, TweenTrans trans = TRANS_LINEAR, TweenEase ease = EASE_IN_OUT);
    void tween_from(Vec2* p, const Vec2& from, const Vec2& to, float seconds, TweenTrans trans = TRANS_LINEAR,
                    TweenEase ease = EASE_IN_OUT);
    /* Stops the tween on *p (the value stays where it is). */
    void kill(float* p);
    void kill(Vec2* p);
    void update(double dt);
    void clear() { items_.clear(); }
    bool running() const { return !items_.empty(); }

private:
    struct Item {
        float* p;
        float from, to, seconds, t;
        TweenTrans trans;
        TweenEase ease;
    };
    std::vector<Item> items_;
};

} /* namespace spire */

#endif

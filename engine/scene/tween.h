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

enum TweenTrans { TRANS_LINEAR, TRANS_SINE, TRANS_EXPO, TRANS_ELASTIC, TRANS_BACK, TRANS_CUBIC, TRANS_CIRC, TRANS_QUART,
                  TRANS_SPRING, TRANS_QUAD, TRANS_BOUNCE, TRANS_QUINT };
enum TweenEase { EASE_IN, EASE_OUT, EASE_IN_OUT };

/* Godot's Tween.interpolate_value: t in [0, d] seconds, from b by c. */
float tween_interpolate(TweenTrans trans, TweenEase ease, float t, float b, float c, float d);

class TweenSet {
public:
    TweenSet();
    TweenSet(const TweenSet& other);
    TweenSet& operator=(const TweenSet& other);
    ~TweenSet();
    /* Every live set drops its tweens on [p, p + size): memory about to be
     * freed (Node's operator delete, a CanvasItem's shader parameters), so
     * a node deleted mid-tween is not written to afterwards. */
    static void forget(const void* p, size_t size);
    /* Tweens *p to `to` over `seconds`, from its value when it starts
     * (after `delay` seconds: Godot's SetDelay, or intervals before it). */
    void tween(float* p, float to, float seconds, TweenTrans trans = TRANS_LINEAR, TweenEase ease = EASE_IN_OUT,
               float delay = 0);
    /* The same from a given start (Godot's .From(v)), set when it starts. */
    void tween_from(float* p, float from, float to, float seconds, TweenTrans trans = TRANS_LINEAR,
                    TweenEase ease = EASE_IN_OUT, float delay = 0);
    void tween(Vec2* p, const Vec2& to, float seconds, TweenTrans trans = TRANS_LINEAR, TweenEase ease = EASE_IN_OUT,
               float delay = 0);
    void tween_from(Vec2* p, const Vec2& from, const Vec2& to, float seconds, TweenTrans trans = TRANS_LINEAR,
                    TweenEase ease = EASE_IN_OUT, float delay = 0);
    /* Stops the tween on *p (the value stays where it is). */
    void kill(float* p);
    void kill(Vec2* p);
    void update(double dt);
    void clear() { items_.clear(); }
    bool running() const { return !items_.empty(); }

private:
    void add(float* p, float from, bool from_current, float to, float seconds, TweenTrans trans, TweenEase ease,
             float delay);
    struct Item {
        float* p;
        float from, to, seconds, t; /* t < 0: waiting out its delay */
        bool from_current;          /* from: the value when it starts */
        TweenTrans trans;
        TweenEase ease;
    };
    std::vector<Item> items_;
    /* the live sets (forget) */
    TweenSet* prev_;
    TweenSet* next_;
    static TweenSet* first_;
    void link();
};

} /* namespace spire */

#endif

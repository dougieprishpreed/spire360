#include "screen_shake.h"

#include "../scene/node.h"
#include "../scene/scene.h"
#include "../scene/script.h"
#include "ui_util.h"

#include <math.h>

namespace spire {

namespace {

float strength_of(const std::string& s) {
    if (s == "VeryWeak") return 2;
    if (s == "Weak") return 5;
    if (s == "Medium") return 20;
    if (s == "Strong") return 40;
    if (s == "TooMuch") return 80;
    return 0;
}

double duration_of(const std::string& d) {
    if (d == "Short") return 0.3;
    if (d == "Normal") return 0.8;
    if (d == "Long") return 1.2;
    if (d == "Forever") return 999999999.0;
    return 0;
}

float cubic_out(float p) {
    float n = p - 1;
    return n * n * n + 1;
}

unsigned hash(int i, int seed) {
    unsigned h = (unsigned)seed ^ ((unsigned)i * 501125321u);
    h *= 0x27d4eb2du;
    h ^= h >> 15;
    return h;
}

/* Godot's FastNoiseLite as the game creates it (Perlin, FBM of 5 octaves,
 * lacunarity 2, gain 0.5), one-dimensional: a gradient per lattice point,
 * quintic interpolation; about -1..1. */
float perlin(float x, int seed) {
    int i = (int)floorf(x);
    float d = x - (float)i;
    float g0 = (float)(hash(i, seed) & 0xffff) / 32767.5f - 1.0f;
    float g1 = (float)(hash(i + 1, seed) & 0xffff) / 32767.5f - 1.0f;
    float t = d * d * d * (d * (d * 6 - 15) + 10);
    float a = g0 * d, b = g1 * (d - 1);
    return (a + (b - a) * t) * 2.0f;
}

float rand01() { return (float)(chaotic_rand() % 1000001) / 1000000.0f; }

} /* namespace */

float fbm(float x, float frequency, int seed) {
    float sum = 0, amp = 1, bound = 0, f = frequency;
    for (int o = 0; o < 5; ++o) {
        sum += perlin(x * f, seed + o) * amp;
        bound += amp;
        amp *= 0.5f;
        f *= 2;
    }
    return sum / bound;
}

ScreenShake::ScreenShake()
    : target_(NULL), multiplier_(1), punch_(false), punch_strength_(0), punch_angle_(0), punch_t_(0), punch_len_(0),
      rumble_(false), drunk_(false), rumble_strength_(0), rumble_freq_(0), rumble_offset_(0), rumble_t_(0),
      rumble_len_(0), trauma_(0), trauma_t_(0), trauma_seed_((int)(chaotic_rand() % 999999)) {
    base_[0] = base_[1] = base_[2] = base_[3] = 0;
}

ScreenShake& ScreenShake::get() {
    static ScreenShake s;
    return s;
}

void ScreenShake::place_target(const Vec2& v) {
    if (!target_) return;
    target_->offset[SIDE_LEFT] = base_[SIDE_LEFT] + v.x;
    target_->offset[SIDE_RIGHT] = base_[SIDE_RIGHT] + v.x;
    target_->offset[SIDE_TOP] = base_[SIDE_TOP] + v.y;
    target_->offset[SIDE_BOTTOM] = base_[SIDE_BOTTOM] + v.y;
}

void ScreenShake::set_target(Control* target) {
    if (target_ == target) return;
    place_target(Vec2(0, 0));
    target_ = target;
    punch_ = rumble_ = false;
    if (target)
        for (int k = 0; k < 4; ++k) base_[k] = target->offset[k];
}

void ScreenShake::clear_target(Control* target) {
    if (target_ != target) return;
    target_ = NULL;
    punch_ = rumble_ = false;
}

/* ScreenPunchInstance */
void ScreenShake::shake(const std::string& strength, const std::string& duration, float deg) {
    if (!target_) return;
    if (deg < 0) deg = rand01() * 360;
    punch_ = true;
    punch_strength_ = strength_of(strength) * multiplier_;
    punch_len_ = punch_t_ = duration_of(duration);
    punch_angle_ = deg * 3.14159265f / 180;
}

/* ScreenRumbleInstance: Perlin at frequency 6 (Rumble) or 0.1 (Drunk),
 * strength x0.5 or x5. */
void ScreenShake::rumble(const std::string& strength, const std::string& duration, bool drunk) {
    if (!target_) return;
    rumble_ = true;
    drunk_ = drunk;
    rumble_freq_ = drunk ? 0.1f : 6.0f;
    rumble_strength_ = strength_of(strength) * multiplier_ * (drunk ? 5.0f : 0.5f);
    rumble_len_ = rumble_t_ = duration_of(duration);
    rumble_offset_ = rand01() * 99999;
    drunk_offset_ = Vec2();
}

/* ScreenTraumaRumble.AddTrauma */
void ScreenShake::trauma(const std::string& strength) {
    float add = strength == "VeryWeak" ? 0.07f : strength == "Weak" ? 0.2f : strength == "Medium" ? 0.4f
              : strength == "Strong" ? 0.6f : strength == "TooMuch" ? 1.0f : 0.0f;
    trauma_ = minf(trauma_ + add, 1);
}

void ScreenShake::update(double dt) {
    Vec2 v;
    if (rumble_) {
        rumble_t_ -= dt;
        float n = (float)rumble_t_;
        Vec2 noise(fbm(n + rumble_offset_, rumble_freq_, 0), fbm(n + rumble_offset_ * 2, rumble_freq_, 0));
        float ease = cubic_out((float)(rumble_t_ / rumble_len_));
        if (drunk_) {
            float k = clampf((float)dt * 10, 0, 1);
            drunk_offset_ = drunk_offset_ + (noise - drunk_offset_) * k;
            v = drunk_offset_ * (rumble_strength_ * ease);
        } else {
            v = noise * (rumble_strength_ * ease);
        }
        if (rumble_t_ < 0) rumble_ = false;
    }
    if (punch_) {
        /* the punch replaces the rumble's offset while it lasts */
        punch_t_ -= dt;
        float x = cosf((float)punch_t_ * 60.0f);
        float ease = cubic_out((float)(punch_t_ / punch_len_));
        v = Vec2(x * cosf(punch_angle_), x * sinf(punch_angle_)) * (punch_strength_ * ease);
        if (punch_t_ < 0) punch_ = false;
    }
    if (trauma_ > 0) {
        trauma_t_ += dt * 1000.0;
        float amount = powf(trauma_, 1.5f) * 200 * multiplier_;
        Vec2 t(fbm((float)trauma_t_ + 100, 0.01f, trauma_seed_), fbm((float)trauma_t_ + 300, 0.01f, trauma_seed_));
        t = t * amount;
        float len = sqrtf(t.x * t.x + t.y * t.y);
        if (len > 50) t = t * (50 / len);
        v = v + t;
        trauma_ = maxf(trauma_ - (float)(2.0 * dt), 0);
    }
    offset_ = v;
    place_target(v);
}

namespace {
class ShakeDriver : public ScriptBehavior {
public:
    explicit ShakeDriver(Control* target) : target_(target) { ScreenShake::get().set_target(target); }
    virtual ~ShakeDriver() { ScreenShake::get().clear_target(target_); }
    virtual void process(Node*, double dt) {
        ScreenShake& s = ScreenShake::get();
        if (s.target() != target_) return;
        s.update(dt);
        /* the top bar stays put (once laid out: a position set before
         * would keep its size 0) */
        Node* bar = target_->get_node("TopBarLayer");
        if (bar && bar->is_control() && ((Control*)bar)->size.x > 0) ((Control*)bar)->set_position(s.offset() * -1.0f);
    }

private:
    Control* target_;
};
} /* namespace */

Node* make_shake_driver(Control* target) {
    Node* n = SceneLibrary::create_node("Node");
    n->name = "ShakeDriver";
    n->behavior = new ShakeDriver(target);
    return n;
}

bool ScreenShake::cue(const std::string& call, const Json& args) {
    std::string a0 = args.size() > 0 && args[(size_t)0].is_string() ? args[(size_t)0].str() : std::string();
    std::string a1 = args.size() > 1 && args[(size_t)1].is_string() ? args[(size_t)1].str() : std::string();
    if (call == "NGame.ScreenShake") {
        float deg = args.size() > 2 && args[(size_t)2].is_number() ? (float)args[(size_t)2].num() : -1.0f;
        shake(a0, a1, deg);
        return true;
    }
    if (call == "NGame.ScreenRumble") {
        std::string style = args.size() > 2 && args[(size_t)2].is_string() ? args[(size_t)2].str() : std::string();
        rumble(a0, a1, style == "Drunk");
        return true;
    }
    if (call == "NGame.ScreenShakeTrauma") {
        trauma(a0);
        return true;
    }
    return false;
}

} /* namespace spire */

/* NGame's screen shake (NScreenShake): a punch (a cosine wiggle along an
 * angle, easing out), a rumble (noise) and trauma (noise that decays), added
 * together onto the shake target's position: the room's scene container
 * (NCombatRoom.SceneContainer, SetScreenShakeTarget). The settings'
 * multiplier scales them (Normal: 1). */
#ifndef SPIRE_SCREEN_SHAKE_H
#define SPIRE_SCREEN_SHAKE_H

#include "../core/base.h"
#include "../core/json.h"

namespace spire {

class Control;

class ScreenShake {
public:
    static ScreenShake& get();
    void set_target(Control* target);
    /* Only if it is the target (a screen going away). */
    void clear_target(Control* target);
    /* ShakeStrength / ShakeDuration by name ("Weak", "Short"); deg < 0:
     * a random angle. */
    void shake(const std::string& strength, const std::string& duration, float deg = -1);
    void rumble(const std::string& strength, const std::string& duration, bool drunk);
    void trauma(const std::string& strength);
    void set_multiplier(float m) { multiplier_ = m; }
    Control* target() const { return target_; }
    void update(double dt);
    /* A logic cue (NGame.ScreenShake / ScreenRumble / ScreenShakeTrauma);
     * false if it is none of them. */
    bool cue(const std::string& call, const Json& args);

private:
    ScreenShake();
    Control* target_;
    Vec2 origin_;
    float multiplier_;
    /* punch */
    bool punch_;
    float punch_strength_, punch_angle_;
    double punch_t_, punch_len_;
    /* rumble */
    bool rumble_, drunk_;
    float rumble_strength_, rumble_freq_, rumble_offset_;
    double rumble_t_, rumble_len_;
    Vec2 drunk_offset_;
    /* trauma */
    float trauma_;
    double trauma_t_;
    int trauma_seed_;
};

} /* namespace spire */

#endif

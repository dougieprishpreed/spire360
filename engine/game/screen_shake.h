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

/* Godot's FastNoiseLite as the game sets it up (Perlin FBM), one-dimensional;
 * about -1..1. */
float fbm(float x, float frequency, int seed);

class Node;

/* A node that makes target the shake's target (NGame.SetScreenShakeTarget)
 * and runs the shake while it is; going away it clears it
 * (ClearScreenShakeTarget). A top bar inside the target (the client puts
 * it in the room's tree; the game's is outside the room) stays put. */
Node* make_shake_driver(Control* target);

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
    /* What the shake adds to the target's position now. */
    Vec2 offset() const { return offset_; }
    void update(double dt);
    /* A logic cue (NGame.ScreenShake / ScreenRumble / ScreenShakeTrauma);
     * false if it is none of them. */
    bool cue(const std::string& call, const Json& args);

private:
    ScreenShake();
    Control* target_;
    Vec2 origin_, offset_;
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

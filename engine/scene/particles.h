/* CPUParticles2D and GPUParticles2D. The simulation is Godot 4.5.1's
 * CPUParticles2D (scene/2d/cpu_particles_2d.cpp, MIT, see THIRD_PARTY.md);
 * GPUParticles2D runs the same simulation with its ParticleProcessMaterial
 * mapped as Godot's CPUParticles2D::convert_from_particles does (plus the
 * material's alpha curve). GPU particles' random sequence differs from
 * Godot's shader one; the distributions are the same. Not ported:
 * turbulence, collision, sub-emitters, attractors, trails. */
#ifndef SPIRE_PARTICLES_H
#define SPIRE_PARTICLES_H

#include "node.h"

namespace spire {

class Texture2DRes;

/* Godot's Curve (sample, not baked) and Gradient, read from resources. */
struct CurveData {
    struct Point {
        Vec2 pos;
        float left, right;
    };
    std::vector<Point> points;
    bool valid() const { return !points.empty(); }
    float sample(float x) const;
    static bool from(Resource* r, CurveData& out); /* Curve or CurveTexture */
};

struct GradientData {
    std::vector<float> offsets;
    std::vector<Color> colors;
    int interpolation; /* 0 linear, 1 constant, 2 cubic */
    GradientData() : interpolation(0) {}
    bool valid() const { return !offsets.empty(); }
    Color at(float t) const;
    static bool from(Resource* r, GradientData& out); /* Gradient or GradientTexture1D/2D */
};

class Particles2D : public Node2D {
public:
    enum Param { P_VELOCITY, P_ANGULAR_VELOCITY, P_ORBIT_VELOCITY, P_LINEAR_ACCEL, P_RADIAL_ACCEL,
                 P_TANGENTIAL_ACCEL, P_DAMPING, P_ANGLE, P_SCALE, P_HUE_VARIATION, P_ANIM_SPEED, P_ANIM_OFFSET,
                 P_MAX };
    explicit Particles2D(bool gpu);
    bool gpu;
    bool emitting, one_shot, local_coords, fractional_delta, align_y, split_scale;
    int amount, fixed_fps, draw_order, emission_shape;
    double lifetime, preprocess, speed_scale, explosiveness, randomness, lifetime_randomness;
    u32 seed;
    Texture2DRes* texture;
    Vec2 direction, gravity, emission_rect_extents;
    float spread, emission_sphere_radius;
    Color color;
    float pmin[P_MAX], pmax[P_MAX];
    CurveData curves[P_MAX], scale_x, scale_y, alpha_curve;
    GradientData color_ramp, color_initial_ramp;
    std::vector<Vec2> emission_points;

    virtual bool set(const std::string& key, const Json& v, const PropCtx& ctx);
    virtual void process(double dt);
    virtual void draw(Canvas& c);
    void restart();
    size_t active_count() const;
    bool finished() const { return !emitting && active_count() == 0; }

private:
    struct Particle {
        Xform2D xform;
        Vec2 velocity;
        float rotation;
        Color color, base_color, start_color_rand;
        float custom[4];
        double time, lifetime;
        float angle_rand, scale_rand, hue_rot_rand, anim_offset_rand;
        u32 seed;
        bool active;
    };
    void step(double dt);
    void apply_process_material(Resource* m);
    std::vector<Particle> particles_;
    double time_, frame_remainder_;
    u32 cycle_;
    bool started_;
};

} /* namespace spire */

#endif

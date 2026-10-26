/* CPUParticles2D and GPUParticles2D, ported from Godot 4.5.1 (MIT, see
 * THIRD_PARTY.md). CPUParticles2D is scene/2d/cpu_particles_2d.cpp's
 * simulation. GPUParticles2D runs, on the CPU, the shader Godot generates
 * for its ParticleProcessMaterial (start() and process(): directional,
 * radial and orbit velocity, turbulence, the emission shapes with their
 * scale and offset, the emission curve, velocity limit, scale over
 * velocity) inside particles.glsl's frame loop (restart phases, the
 * particle's number and seed). Without local_coords the particles live in
 * the canvas: emitted through the node's global transform, drawn without
 * it (Godot's identity canvas item transform), so they stay behind a
 * moving emitter and ignore its scale. Collision: GPU particles whose
 * material has a collision mode bounce off (or vanish on) the
 * LightOccluder2D polygons of their tree, Godot's canvas SDF taken exactly
 * from the polygons. Sub-emitters: the parent's particles send emissions
 * (constant, at the end, at a collision, at the start) to the system its
 * sub_emitter names, which then emits only those. Trails: each particle
 * is GPUParticles2D's trail mesh, a ribbon of the texture's width through
 * its transforms over the last trail_lifetime (bone i of the sections at
 * i / sections of it). Not ported: attractors, the emission point
 * textures, fixed-fps interpolation. */
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
    /* A CurveXYZTexture's curve_x, curve_y or curve_z (channel 0..2). */
    static bool from_xyz(Resource* r, int channel, CurveData& out);
};

/* A curve texture as a shader samples it: a CurveTexture's value in every
 * channel (texture_mode RGB) or in red only (RED), a CurveXYZTexture's
 * curves per channel (0 without one). */
struct CurveTex {
    enum Kind { NONE, RGB, RED, XYZ };
    Kind kind;
    CurveData c[3];
    CurveTex() : kind(NONE) {}
    bool valid() const { return kind != NONE; }
    float channel(int i, float t) const;
    float r(float t) const { return channel(0, t); }
    static bool from(Resource* r, CurveTex& out);
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
    bool interpolate; /* GPUParticles2D: drawn ahead by VELOCITY * the fixed-fps remainder */
    int amount, fixed_fps, draw_order, emission_shape;
    double lifetime, preprocess, speed_scale, explosiveness, randomness, lifetime_randomness;
    u32 seed;
    Texture2DRes* texture;
    Vec2 direction, gravity, emission_rect_extents;
    float spread, emission_sphere_radius;
    /* ParticleProcessMaterial collision (0 none, 1 rigid, 2 hide on
     * contact) and GPUParticles2D's collision_base_size */
    int collision_mode;
    float collision_friction, collision_bounce, collision_base_size;
    bool collision_use_scale;
    /* GPUParticles2D's seed: random at creation and each restart unless
     * fixed */
    bool use_fixed_seed;
    /* GPUParticles2D trails: seconds, sections, subdivisions of each */
    bool trail_enabled;
    float trail_lifetime;
    int trail_sections, trail_subdivisions;
    Color color;
    float pmin[P_MAX], pmax[P_MAX];
    CurveData curves[P_MAX], scale_x, scale_y, alpha_curve;
    GradientData color_ramp, color_initial_ramp;
    std::vector<Vec2> emission_points;

    /* GPUParticles2D: the rest of its ParticleProcessMaterial (3D vectors
     * as x, y, z) and the node's amount_ratio / interp_to_end. */
    struct Material {
        float direction[3], emission_box[3], shape_offset[3], shape_scale[3], velocity_pivot[3];
        float ring_axis[3], ring_height, ring_radius, ring_inner_radius, ring_cone_angle;
        float directional_min, directional_max, radial_velocity_min, radial_velocity_max;
        float scale_over_velocity_min, scale_over_velocity_max, inherit_velocity_ratio;
        bool damping_as_friction, turbulence;
        float turb_strength, turb_scale, turb_speed[3], turb_speed_random;
        float turb_influence_min, turb_influence_max, turb_displacement_min, turb_displacement_max;
        CurveTex scale, orbit, directional, radial_velocity, velocity_limit, emission, scale_over_velocity,
            turb_influence_over_life;
        float amount_ratio, interp_to_end;
        /* sub_emitter_mode: 0 none, 1 constant (every sub_interval s), 2 at
         * the end, 3 at a collision, 4 at the start */
        int sub_mode, sub_at_end, sub_at_collision, sub_at_start;
        float sub_interval;
        bool sub_keep_velocity;
        /* EMISSION_SHAPE_POINTS / DIRECTED_POINTS: the point, normal and
         * colour textures' texels (4 floats each, row by row) */
        std::vector<float> points, normals, colors;
        int point_count;
        Material();
    };
    Material mat;
    std::string sub_emitter; /* GPUParticles2D.sub_emitter's path */

    virtual bool set(const std::string& key, const Json& v, const PropCtx& ctx);
    virtual void process(double dt);
    virtual void draw(Canvas& c);
    void restart();
    size_t active_count() const;
    /* Seconds it has run (stepped). */
    double clock() const { return clock_; }
    bool finished() const { return !emitting && active_count() == 0 && pending_.empty(); }

private:
    /* The shader's DisplayParameters / DynamicsParameters /
     * PhysicalParameters as drawn from a particle's seed at its start
     * (process() draws the same ones each frame); seed: the sequence
     * after them. */
    struct Drawn {
        float scale, hue_rotation, anim_speed, anim_offset, life, emission_texture_position;
        Color color;
        float angle, angular_velocity, initial_velocity, directional, radial, orbit, turb_influence;
        float linear_accel, radial_accel, tangent_accel, damping;
        u32 seed;
    };
    struct Particle {
        Xform2D xform;
        Vec2 velocity;
        float vz; /* GPU: VELOCITY.z (turbulence turns it out of the plane) */
        float rotation;
        Color color, base_color, start_color_rand;
        float custom[4];
        double time, lifetime;
        float angle_rand, scale_rand, hue_rot_rand, anim_offset_rand;
        u32 seed;
        u32 cycle; /* GPU: the cycle it started in (its NUMBER) */
        Drawn d;
        bool active;
        /* trails: its transforms since start (the system's clock) */
        std::vector<std::pair<double, Xform2D> > trail;
    };
    Xform2D trail_at(const Particle& p, double age) const;
    void step(double dt);
    void step_gpu(double dt);
    /* an emission from the parent system (emit_subparticle): its
     * transform, and its velocity if kept */
    struct Emission {
        Xform2D xform;
        Vec2 velocity;
        float vz;
        bool has_velocity;
    };
    void gpu_start(Particle& p, u32 number, const Emission* from);
    void gpu_process(Particle& p, double dt, bool collided, const Vec2& normal, float depth, Particles2D* sub);
    Particles2D* sub_target();
    void emit_to(Particles2D* sub, const Particle& p);
    void gpu_display(const Particle& p, float life, float scale[3], float* anim_offset, float* anim_speed,
                     Color* color) const;
    Vec2 noise_direction(const float pos[3], float* z) const;
    void apply_process_material(Resource* m);
    bool collision(const Particle& p, Vec2* normal, float* depth);
    std::vector<Particle> particles_;
    double time_, frame_remainder_;
    u32 cycle_;
    bool started_;
    /* the emission transform (identity with local_coords), the particles'
     * space to the canvas (the node's global transform with local_coords)
     * and the emitter's velocity (GPUParticles2D: its global position's
     * change) */
    Xform2D emission_, to_canvas_;
    Vec2 emitter_velocity_, last_origin_;
    bool has_last_origin_;
    double clock_; /* the time stepped (trail history) */
    /* as a sub-emitter: emits only what its parent sends */
    bool sub_mode_;
    std::vector<Emission> pending_;
};

/* LightOccluder2D: no lights in the game, so it draws nothing; its polygon
 * is what colliding GPU particles bounce off (Godot's canvas SDF). */
class LightOccluder2D : public Node2D {
public:
    LightOccluder2D();
    virtual ~LightOccluder2D();
    std::vector<Vec2> polygon;
    bool sdf_collision;
    virtual bool set(const std::string& key, const Json& v, const PropCtx& ctx);
    /* The canvas SDF at a global point of root's tree: the distance to the
     * nearest occluder (< 0 inside one), *normal the outward direction.
     * False when the tree has none. */
    static bool sdf(const Node* root, const Vec2& p, float* dist, Vec2* normal);
};

} /* namespace spire */

#endif

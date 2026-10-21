#include "particles.h"
#include "shaders.h"

#include "canvas.h"
#include "scene.h"
#include "value.h"
#include "viewport.h"

#include <math.h>
#include <string.h>

namespace spire {

static const float TAU_F = 6.28318530718f;
/* the ParticleProcessMaterial shader's pi */
static const float SHADER_PI = 3.14159f;
static float deg2rad(float d) { return d * 0.0174532925f; }
static float lerpf(float a, float b, float t) { return a + (b - a) * t; }
static float signf(float v) { return v > 0 ? 1.0f : (v < 0 ? -1.0f : 0.0f); }

/* ---- Godot's RandomPCG / pcg32 ---- */

struct Pcg {
    u64 state, inc;
    u32 next() {
        u64 old = state;
        state = old * 6364136223846793005ULL + (inc | 1);
        u32 xs = (u32)(((old >> 18u) ^ old) >> 27u);
        u32 rot = (u32)(old >> 59u);
        return (xs >> rot) | (xs << ((32 - rot) & 31));
    }
    void seed(u64 s) {
        state = 0;
        inc = (1442695040888963407ULL << 1u) | 1u;
        next();
        state += s;
        next();
    }
    float randf() {
        u32 e = next();
        if (e == 0) return 0;
        int clz = 0;
        while (!(e & 0x80000000u)) {
            e <<= 1;
            ++clz;
        }
        return (float)ldexp((double)(float)(next() | 0x80000001u), -32 - clz);
    }
};

/* particles.glsl's and the material shader's hash */
static u32 idhash(u32 x) {
    x = ((x >> 16u) ^ x) * 0x45d9f3bu;
    x = ((x >> 16u) ^ x) * 0x45d9f3bu;
    return (x >> 16u) ^ x;
}

static float rand_from_seed(u32& seed) {
    int s = (int)seed;
    if (s == 0) s = 305420679;
    int k = s / 127773;
    s = 16807 * (s - k * 127773) - 2836 * k;
    if (s < 0) s += 2147483647;
    seed = (u32)s;
    return (seed % 65536u) / 65535.0f;
}

/* ---- Curve / Gradient ---- */

static Resource* sub_of(Resource* r, const char* key) {
    if (!r || !r->props || !r->props->has(key)) return NULL;
    SceneLibrary* lib = SceneLibrary::current();
    return lib ? lib->resolve((*r->props)[key], r->file) : NULL;
}

bool CurveData::from(Resource* r, CurveData& out) {
    out.points.clear();
    if (!r || !r->props) return false;
    if (r->type == "CurveTexture") return from(sub_of(r, "curve"), out);
    if (r->type != "Curve") return false;
    const Json& d = (*r->props)["_data"];
    int n = r->props->has("point_count") ? (*r->props)["point_count"].i() : (int)(d.size() / 5);
    for (int i = 0; i < n && (size_t)(i * 5 + 2) < d.size(); ++i) {
        Point p;
        jv_vec2(d[(size_t)(i * 5)], p.pos);
        p.left = jv_float(d[(size_t)(i * 5 + 1)]);
        p.right = jv_float(d[(size_t)(i * 5 + 2)]);
        out.points.push_back(p);
    }
    return out.valid();
}

bool CurveData::from_xyz(Resource* r, int channel, CurveData& out) {
    static const char* const keys[3] = { "curve_x", "curve_y", "curve_z" };
    out.points.clear();
    if (!r || !r->props || r->type != "CurveXYZTexture" || channel < 0 || channel > 2) return false;
    return from(sub_of(r, keys[channel]), out);
}

float CurveData::sample(float x) const {
    if (points.empty()) return 0;
    if (points.size() == 1) return points[0].pos.y;
    /* get_index: last point with pos.x <= x */
    size_t i = 0;
    for (size_t k = 0; k < points.size(); ++k)
        if (points[k].pos.x <= x) i = k;
    if (i == points.size() - 1) return points[i].pos.y;
    float local = x - points[i].pos.x;
    if (i == 0 && local <= 0) return points[0].pos.y;
    const Point& a = points[i];
    const Point& b = points[i + 1];
    float d = b.pos.x - a.pos.x;
    if (fabsf(d) < 1e-6f) return b.pos.y;
    float t = local / d;
    d /= 3.0f;
    float yac = a.pos.y + d * a.right, ybc = b.pos.y - d * b.left;
    float omt = 1.0f - t;
    return omt * omt * omt * a.pos.y + 3 * omt * omt * t * yac + 3 * omt * t * t * ybc + t * t * t * b.pos.y;
}

bool CurveTex::from(Resource* r, CurveTex& out) {
    out.kind = NONE;
    for (int i = 0; i < 3; ++i) out.c[i].points.clear();
    if (!r || !r->props) return false;
    if (r->type == "CurveTexture") {
        out.kind = (*r->props)["texture_mode"].i() == 1 ? RED : RGB;
        CurveData::from(sub_of(r, "curve"), out.c[0]);
    } else if (r->type == "CurveXYZTexture") {
        out.kind = XYZ;
        for (int i = 0; i < 3; ++i) CurveData::from_xyz(r, i, out.c[i]);
    }
    return out.valid();
}

float CurveTex::channel(int i, float t) const {
    if (kind == RGB) return c[0].sample(t);
    if (kind == RED) return i == 0 ? c[0].sample(t) : 0.0f;
    return kind == XYZ && i >= 0 && i < 3 ? c[i].sample(t) : 0.0f;
}

bool GradientData::from(Resource* r, GradientData& out) {
    out.offsets.clear();
    out.colors.clear();
    if (!r || !r->props) return false;
    if (r->type == "GradientTexture1D" || r->type == "GradientTexture2D") return from(sub_of(r, "gradient"), out);
    if (r->type != "Gradient") return false;
    const Json& o = (*r->props)["offsets"];
    const Json& c = (*r->props)["colors"];
    if (!r->props->has("offsets")) {
        /* Gradient defaults: black at 0, white at 1 */
        out.offsets.push_back(0);
        out.offsets.push_back(1);
        out.colors.push_back(Color(0, 0, 0, 1));
        out.colors.push_back(Color(1, 1, 1, 1));
        const Json& cv = c["v"];
        for (size_t i = 0; i < out.colors.size() && i * 4 + 3 < cv.size(); ++i) {
            size_t k = i * 4;
            out.colors[i] = Color(jv_float(cv[k]), jv_float(cv[k + 1]), jv_float(cv[k + 2]), jv_float(cv[k + 3]));
        }
    } else {
        const Json& ov = o["v"];
        const Json& cv = c["v"];
        for (size_t i = 0; i < ov.size(); ++i) {
            out.offsets.push_back(jv_float(ov[i]));
            size_t k = i * 4;
            out.colors.push_back(k + 3 < cv.size() ? Color(jv_float(cv[k]), jv_float(cv[k + 1]), jv_float(cv[k + 2]),
                                                           jv_float(cv[k + 3]))
                                                   : Color());
        }
    }
    out.interpolation = (*r->props)["interpolation_mode"].i();
    return out.valid();
}

Color GradientData::at(float t) const {
    if (offsets.empty()) return Color();
    /* points are stored sorted by offset */
    if (t <= offsets[0]) return colors[0];
    if (t >= offsets.back()) return colors.back();
    size_t i = 0;
    while (i + 1 < offsets.size() && offsets[i + 1] < t) ++i;
    if (interpolation == 1) return colors[i];
    float d = offsets[i + 1] - offsets[i];
    float f = d > 0 ? (t - offsets[i]) / d : 0;
    const Color& a = colors[i];
    const Color& b = colors[i + 1];
    if (interpolation == 2) {
        /* Godot Gradient's cubic interpolation, using the adjacent stops;
         * endpoints duplicate the nearest colour. */
        const Color& before = colors[i > 0 ? i - 1 : 0];
        const Color& after = colors[i + 2 < colors.size() ? i + 2 : colors.size() - 1];
        Color result;
        float* dest[] = { &result.r, &result.g, &result.b, &result.a };
        float av[] = { a.r, a.g, a.b, a.a }, bv[] = { b.r, b.g, b.b, b.a };
        float pv[] = { before.r, before.g, before.b, before.a }, nv[] = { after.r, after.g, after.b, after.a };
        for (int k = 0; k < 4; ++k)
            *dest[k] = 0.5f * (2 * av[k] + (-pv[k] + bv[k]) * f +
                (2 * pv[k] - 5 * av[k] + 4 * bv[k] - nv[k]) * f * f +
                (-pv[k] + 3 * av[k] - 3 * bv[k] + nv[k]) * f * f * f);
        return result;
    }
    return Color(lerpf(a.r, b.r, f), lerpf(a.g, b.g, f), lerpf(a.b, b.b, f), lerpf(a.a, b.a, f));
}

/* ---- properties ---- */

static const char* const PARAM_NAMES_CPU[] = { "initial_velocity", "angular_velocity", "orbit_velocity",
                                                "linear_accel", "radial_accel", "tangential_accel", "damping",
                                                "angle", "scale_amount", "hue_variation", "anim_speed",
                                                "anim_offset" };
static const char* const PARAM_NAMES_GPU[] = { "initial_velocity", "angular_velocity", "orbit_velocity",
                                                "linear_accel", "radial_accel", "tangential_accel", "damping",
                                                "angle", "scale", "hue_variation", "anim_speed", "anim_offset" };
static const char* const CURVE_NAMES_GPU[] = { "velocity_curve", "angular_velocity_curve", "orbit_velocity_curve",
                                                "linear_accel_curve", "radial_accel_curve",
                                                "tangential_accel_curve", "damping_curve", "angle_curve",
                                                "scale_curve", "hue_variation_curve", "anim_speed_curve",
                                                "anim_offset_curve" };

/* GPUParticles2D's seed without use_fixed_seed: Math::rand() at creation
 * and each restart (a fixed sequence here, so runs repeat). */
static u32 next_seed() {
    static u32 n = 0;
    return idhash(++n * 2654435761u);
}

/* ParticleProcessMaterial::set_turbulence_noise_scale: the slider's value
 * to the shader's frequency. */
static float turbulence_scale(float v) {
    const float zero_frequency = 4.0f, max_slider = 10.0f, exponent = 0.25f;
    return powf(v, exponent) * (zero_frequency / powf(max_slider, exponent)) - zero_frequency;
}

Particles2D::Particles2D(bool is_gpu)
    : gpu(is_gpu), emitting(true), one_shot(false), local_coords(false), fractional_delta(true), align_y(false),
      split_scale(false), amount(8), fixed_fps(is_gpu ? 30 : 0), draw_order(0), emission_shape(0), lifetime(1),
      preprocess(0), speed_scale(1), explosiveness(0), randomness(0), lifetime_randomness(0), seed(0), texture(NULL),
      direction(1, 0), gravity(is_gpu ? Vec2(0, -9.8f) : Vec2(0, 980)), emission_rect_extents(1, 1), spread(45),
      emission_sphere_radius(1), collision_mode(0), collision_friction(0), collision_bounce(0), collision_base_size(1),
      collision_use_scale(false), use_fixed_seed(false), trail_enabled(false), trail_lifetime(0.3f), trail_sections(8),
      trail_subdivisions(4), time_(0), frame_remainder_(0), cycle_(0), started_(false), has_last_origin_(false),
      clock_(0), sub_mode_(false) {
    for (int i = 0; i < P_MAX; ++i) pmin[i] = pmax[i] = 0;
    pmin[P_SCALE] = pmax[P_SCALE] = 1;
    if (gpu) seed = next_seed();
}

Particles2D::Material::Material()
    : ring_height(1), ring_radius(1), ring_inner_radius(0), ring_cone_angle(90), directional_min(1),
      directional_max(1), radial_velocity_min(0), radial_velocity_max(0), scale_over_velocity_min(0),
      scale_over_velocity_max(5), inherit_velocity_ratio(0), damping_as_friction(false), turbulence(false),
      turb_strength(1), turb_scale(0), turb_speed_random(0.2f), turb_influence_min(0.1f), turb_influence_max(0.1f),
      turb_displacement_min(0), turb_displacement_max(0), amount_ratio(1), interp_to_end(0), sub_mode(0),
      sub_at_end(1), sub_at_collision(1), sub_at_start(1), sub_interval(0.25f), sub_keep_velocity(false) {
    for (int i = 0; i < 3; ++i) {
        direction[i] = i == 0 ? 1.0f : 0.0f;
        emission_box[i] = shape_scale[i] = 1;
        shape_offset[i] = velocity_pivot[i] = turb_speed[i] = 0;
        ring_axis[i] = i == 2 ? 1.0f : 0.0f;
    }
    turb_scale = turbulence_scale(9);
}

bool Particles2D::set(const std::string& key, const Json& v, const PropCtx& ctx) {
    if (key == "emitting") emitting = v.b(true);
    else if (key == "amount") amount = v.i(8);
    else if (key == "lifetime") lifetime = jv_float(v, 1);
    else if (key == "one_shot") one_shot = v.b();
    else if (key == "preprocess") preprocess = jv_float(v);
    else if (key == "speed_scale") speed_scale = jv_float(v, 1);
    else if (key == "explosiveness") explosiveness = jv_float(v);
    else if (key == "randomness") randomness = jv_float(v);
    else if (key == "lifetime_randomness") lifetime_randomness = jv_float(v);
    else if (key == "fixed_fps") fixed_fps = v.i();
    else if (key == "fract_delta") fractional_delta = v.b(true);
    else if (key == "local_coords") local_coords = v.b();
    else if (key == "collision_base_size") collision_base_size = jv_float(v, 1);
    else if (key == "draw_order") draw_order = v.i();
    else if (key == "seed") seed = (u32)v.num();
    else if (key == "use_fixed_seed") use_fixed_seed = v.b();
    else if (gpu && key == "amount_ratio") mat.amount_ratio = jv_float(v, 1);
    else if (gpu && key == "interp_to_end") mat.interp_to_end = jv_float(v);
    else if (gpu && key == "sub_emitter") sub_emitter = v["np"].str();
    else if (gpu && key == "trail_enabled") trail_enabled = v.b();
    else if (gpu && key == "trail_lifetime") trail_lifetime = jv_float(v, 0.3f);
    else if (gpu && key == "trail_sections") trail_sections = v.i(8) > 1 ? v.i(8) : 2;
    else if (gpu && key == "trail_section_subdivisions") trail_subdivisions = v.i(4) > 0 ? v.i(4) : 1;
    else if (key == "texture") {
        Resource* r = ctx.resource(v);
        texture = bind_texture(this, r);
    } else if (key == "process_material") {
        apply_process_material(ctx.resource(v));
    } else if (!gpu) {
        Vec2 tmp;
        if (key == "direction") jv_vec2(v, direction);
        else if (key == "spread") spread = jv_float(v, 45);
        else if (key == "gravity") jv_vec2(v, gravity);
        else if (key == "emission_shape") emission_shape = v.i();
        else if (key == "emission_sphere_radius") emission_sphere_radius = jv_float(v, 1);
        else if (key == "emission_rect_extents") jv_vec2(v, emission_rect_extents);
        else if (key == "emission_points") {
            emission_points.clear();
            const Json& a = v["v"];
            for (size_t i = 0; i + 1 < a.size(); i += 2) emission_points.push_back(Vec2(jv_float(a[i]), jv_float(a[i + 1])));
        } else if (key == "particle_flag_align_y") align_y = v.b();
        else if (key == "color") jv_color(v, color);
        else if (key == "color_ramp") GradientData::from(ctx.resource(v), color_ramp);
        else if (key == "color_initial_ramp") GradientData::from(ctx.resource(v), color_initial_ramp);
        else if (key == "split_scale") split_scale = v.b();
        else if (key == "scale_curve_x") CurveData::from(ctx.resource(v), scale_x);
        else if (key == "scale_curve_y") CurveData::from(ctx.resource(v), scale_y);
        else {
            for (int i = 0; i < P_MAX; ++i) {
                std::string n = PARAM_NAMES_CPU[i];
                if (key == n + "_min") pmin[i] = jv_float(v);
                else if (key == n + "_max") pmax[i] = jv_float(v);
                else if (key == n + "_curve") CurveData::from(ctx.resource(v), curves[i]);
                else continue;
                return true;
            }
            (void)tmp;
            return Node2D::set(key, v, ctx);
        }
    } else {
        return Node2D::set(key, v, ctx);
    }
    return true;
}

static void vec3_of(const Json& p, const char* key, float out[3]) {
    const Json& a = p[key]["v"];
    if (a.size() < 3) return;
    for (size_t i = 0; i < 3; ++i) out[i] = jv_float(a[i]);
}

/* The ParticleProcessMaterial's stored props (the rest keep its
 * defaults). */
void Particles2D::apply_process_material(Resource* m) {
    if (!m || !m->props || m->type != "ParticleProcessMaterial") return;
    const Json& p = *m->props;
    vec3_of(p, "direction", mat.direction);
    direction = Vec2(mat.direction[0], mat.direction[1]);
    if (p.has("spread")) spread = jv_float(p["spread"], 45);
    if (p.has("color")) jv_color(p["color"], color);
    GradientData::from(sub_of(m, "color_ramp"), color_ramp);
    GradientData::from(sub_of(m, "color_initial_ramp"), color_initial_ramp);
    CurveData::from(sub_of(m, "alpha_curve"), alpha_curve);
    align_y = p["particle_flag_align_y"].b();
    emission_shape = p["emission_shape"].i();
    if (p.has("emission_sphere_radius")) emission_sphere_radius = jv_float(p["emission_sphere_radius"], 1);
    vec3_of(p, "emission_box_extents", mat.emission_box);
    emission_rect_extents = Vec2(mat.emission_box[0], mat.emission_box[1]);
    if (p.has("gravity") && p["gravity"]["v"].size() >= 2)
        gravity = Vec2(jv_float(p["gravity"]["v"][(size_t)0]), jv_float(p["gravity"]["v"][1]));
    lifetime_randomness = jv_float(p["lifetime_randomness"]);
    collision_mode = p["collision_mode"].i();
    collision_friction = jv_float(p["collision_friction"]);
    collision_bounce = jv_float(p["collision_bounce"]);
    collision_use_scale = p["collision_use_scale"].b();
    for (int i = 0; i < P_MAX; ++i) {
        std::string n = PARAM_NAMES_GPU[i];
        if (p.has((n + "_min").c_str())) pmin[i] = jv_float(p[(n + "_min").c_str()]);
        if (p.has((n + "_max").c_str())) pmax[i] = jv_float(p[(n + "_max").c_str()]);
        Resource* c = sub_of(m, CURVE_NAMES_GPU[i]);
        if (i == P_SCALE) CurveTex::from(c, mat.scale);
        else if (i == P_ORBIT_VELOCITY) CurveTex::from(c, mat.orbit);
        else CurveData::from(c, curves[i]);
    }
    vec3_of(p, "emission_shape_offset", mat.shape_offset);
    vec3_of(p, "emission_shape_scale", mat.shape_scale);
    vec3_of(p, "velocity_pivot", mat.velocity_pivot);
    vec3_of(p, "emission_ring_axis", mat.ring_axis);
    if (p.has("emission_ring_height")) mat.ring_height = jv_float(p["emission_ring_height"], 1);
    if (p.has("emission_ring_radius")) mat.ring_radius = jv_float(p["emission_ring_radius"], 1);
    if (p.has("emission_ring_inner_radius")) mat.ring_inner_radius = jv_float(p["emission_ring_inner_radius"]);
    if (p.has("emission_ring_cone_angle")) mat.ring_cone_angle = jv_float(p["emission_ring_cone_angle"], 90);
    if (p.has("directional_velocity_min")) mat.directional_min = jv_float(p["directional_velocity_min"]);
    if (p.has("directional_velocity_max")) mat.directional_max = jv_float(p["directional_velocity_max"]);
    mat.radial_velocity_min = jv_float(p["radial_velocity_min"]);
    mat.radial_velocity_max = jv_float(p["radial_velocity_max"]);
    if (p.has("scale_over_velocity_min")) mat.scale_over_velocity_min = jv_float(p["scale_over_velocity_min"]);
    if (p.has("scale_over_velocity_max")) mat.scale_over_velocity_max = jv_float(p["scale_over_velocity_max"], 5);
    mat.inherit_velocity_ratio = jv_float(p["inherit_velocity_ratio"]);
    mat.damping_as_friction = p["particle_flag_damping_as_friction"].b();
    CurveTex::from(sub_of(m, "directional_velocity_curve"), mat.directional);
    CurveTex::from(sub_of(m, "radial_velocity_curve"), mat.radial_velocity);
    CurveTex::from(sub_of(m, "velocity_limit_curve"), mat.velocity_limit);
    CurveTex::from(sub_of(m, "emission_curve"), mat.emission);
    CurveTex::from(sub_of(m, "scale_over_velocity_curve"), mat.scale_over_velocity);
    mat.turbulence = p["turbulence_enabled"].b();
    if (p.has("turbulence_noise_strength")) mat.turb_strength = jv_float(p["turbulence_noise_strength"], 1);
    if (p.has("turbulence_noise_scale")) mat.turb_scale = turbulence_scale(jv_float(p["turbulence_noise_scale"], 9));
    vec3_of(p, "turbulence_noise_speed", mat.turb_speed);
    if (p.has("turbulence_noise_speed_random"))
        mat.turb_speed_random = jv_float(p["turbulence_noise_speed_random"], 0.2f);
    if (p.has("turbulence_influence_min")) mat.turb_influence_min = jv_float(p["turbulence_influence_min"], 0.1f);
    if (p.has("turbulence_influence_max")) mat.turb_influence_max = jv_float(p["turbulence_influence_max"], 0.1f);
    mat.turb_displacement_min = jv_float(p["turbulence_initial_displacement_min"]);
    mat.turb_displacement_max = jv_float(p["turbulence_initial_displacement_max"]);
    CurveTex::from(sub_of(m, "turbulence_influence_over_life"), mat.turb_influence_over_life);
    mat.sub_mode = p["sub_emitter_mode"].i();
    /* set_sub_emitter_frequency passes the interval ("delta instead") */
    float freq = p.has("sub_emitter_frequency") ? jv_float(p["sub_emitter_frequency"], 4) : 4.0f;
    mat.sub_interval = freq > 0 ? 1.0f / freq : 0.0f;
    if (p.has("sub_emitter_amount_at_end")) mat.sub_at_end = p["sub_emitter_amount_at_end"].i(1);
    if (p.has("sub_emitter_amount_at_collision")) mat.sub_at_collision = p["sub_emitter_amount_at_collision"].i(1);
    if (p.has("sub_emitter_amount_at_start")) mat.sub_at_start = p["sub_emitter_amount_at_start"].i(1);
    mat.sub_keep_velocity = p["sub_emitter_keep_velocity"].b();
}

/* ---- simulation ---- */

void Particles2D::restart() {
    time_ = frame_remainder_ = 0;
    cycle_ = 0;
    started_ = false;
    emitting = true;
    if (gpu && !use_fixed_seed) seed = next_seed();
    for (size_t i = 0; i < particles_.size(); ++i) particles_[i].active = false;
}

size_t Particles2D::active_count() const {
    size_t count = 0;
    for (size_t i = 0; i < particles_.size(); ++i) if (particles_[i].active) ++count;
    return count;
}

void Particles2D::process(double dt) {
    if ((int)particles_.size() != amount) {
        particles_.assign(amount > 0 ? amount : 0, Particle());
        for (size_t i = 0; i < particles_.size(); ++i) particles_[i].active = false;
    }
    Xform2D g = global_transform();
    emission_ = local_coords ? Xform2D() : g;
    to_canvas_ = local_coords ? g : Xform2D();
    if (gpu) {
        /* GPUParticles2D's internal process: the emitter's velocity */
        emitter_velocity_ = has_last_origin_ && dt > 0 ? (g.origin - last_origin_) * (float)(1.0 / dt) : Vec2();
        last_origin_ = g.origin;
        has_last_origin_ = true;
    }
    /* Stopping emission does not stop the particles already alive. In a
     * one-shot system those particles must keep aging until they finish. */
    if (particles_.empty() || (!emitting && active_count() == 0 && pending_.empty())) return;
    double frame_time = fixed_fps > 0 ? 1.0 / fixed_fps : 1.0 / 30.0;
    if (gpu) {
        /* ParticlesStorage::update_particles: the preprocess at the speed
         * of 1, then the frame's time scaled by speed_scale */
        if (!started_) {
            started_ = true;
            for (double todo = preprocess; preprocess > 0 && todo >= 0; todo -= frame_time) step_gpu(frame_time);
        }
        double time_scale = speed_scale > 0 ? speed_scale : 0;
        if (fixed_fps > 0) {
            double l = dt > 0.1 ? 0.1 : (dt <= 0 ? 0.001 : dt);
            double todo = frame_remainder_ + l * time_scale;
            while (todo >= frame_time) {
                step_gpu(frame_time);
                todo -= frame_time;
            }
            frame_remainder_ = todo;
        } else {
            step_gpu(dt * time_scale);
        }
        return;
    }
    if (!started_) {
        started_ = true;
        double todo = preprocess;
        double sp = speed_scale;
        speed_scale = 1.0;
        while (todo > 0) {
            step(frame_time);
            todo -= frame_time;
        }
        speed_scale = sp;
    }
    if (fixed_fps > 0) {
        double l = dt > 0.1 ? 0.1 : (dt <= 0 ? 0.001 : dt);
        double todo = frame_remainder_ + l;
        while (todo >= frame_time) {
            step(frame_time);
            todo -= frame_time;
        }
        frame_remainder_ = todo;
    } else {
        step(dt);
    }
}

/* CPUParticles2D::_particles_process */
void Particles2D::step(double p_delta) {
    p_delta *= speed_scale;
    int pcount = (int)particles_.size();
    double prev_time = time_;
    time_ += p_delta;
    if (time_ > lifetime) {
        time_ = fmod(time_, lifetime);
        ++cycle_;
        if (one_shot && cycle_ > 0) emitting = false;
    }
    double system_phase = time_ / lifetime;
    Pcg rng;
    for (int i = 0; i < pcount; ++i) {
        Particle& p = particles_[i];
        if (!emitting && !p.active) continue;
        double local_delta = p_delta;
        double restart_phase = double(i) / double(pcount);
        if (randomness > 0.0) {
            u32 s = cycle_;
            if (restart_phase >= system_phase) s -= 1u;
            s *= (u32)pcount;
            s += (u32)i;
            double r = double(idhash(s) % 65536u) / 65536.0;
            restart_phase += randomness * r * 1.0 / double(pcount);
        }
        restart_phase *= (1.0 - explosiveness);
        double restart_time = restart_phase * lifetime;
        bool restart = false;
        if (time_ > prev_time) {
            if (restart_time >= prev_time && restart_time < time_) {
                restart = true;
                if (fractional_delta) local_delta = time_ - restart_time;
            }
        } else if (local_delta > 0.0) {
            if (restart_time >= prev_time) {
                restart = true;
                if (fractional_delta) local_delta = lifetime - restart_time + time_;
            } else if (restart_time < time_) {
                restart = true;
                if (fractional_delta) local_delta = time_ - restart_time;
            }
        }
        if (p.active && p.time * (1.0 - explosiveness) > p.lifetime) restart = true;
        float tv = 0.0f;
        if (restart) {
            if (!emitting) {
                p.active = false;
                continue;
            }
            p.active = true;
            float tex_angle = curves[P_ANGLE].valid() ? curves[P_ANGLE].sample(tv) : 1.0f;
            float tex_anim_offset = curves[P_ANGLE].valid() ? curves[P_ANGLE].sample(tv) : 1.0f;
            p.seed = seed + (u32)i + (u32)i + cycle_;
            rng.seed(p.seed);
            p.angle_rand = rng.randf();
            p.scale_rand = rng.randf();
            p.hue_rot_rand = rng.randf();
            p.anim_offset_rand = rng.randf();
            p.start_color_rand = color_initial_ramp.valid() ? color_initial_ramp.at(rng.randf()) : Color();
            float a1 = atan2f(direction.y, direction.x) + deg2rad((rng.randf() * 2.0f - 1.0f) * spread);
            p.velocity = Vec2(cosf(a1), sinf(a1)) * lerpf(pmin[P_VELOCITY], pmax[P_VELOCITY], rng.randf());
            p.rotation = deg2rad(tex_angle * lerpf(pmin[P_ANGLE], pmax[P_ANGLE], p.angle_rand));
            p.custom[0] = 0;
            p.custom[1] = 0;
            p.custom[2] = tex_anim_offset * lerpf(pmin[P_ANIM_OFFSET], pmax[P_ANIM_OFFSET], p.anim_offset_rand);
            p.custom[3] = (float)(1.0 - rng.randf() * lifetime_randomness);
            p.xform = Xform2D();
            p.time = 0;
            p.lifetime = lifetime * p.custom[3];
            p.base_color = Color();
            if (emission_shape == 1) {
                float t = TAU_F * rng.randf(), r = emission_sphere_radius * rng.randf();
                p.xform.origin = Vec2(cosf(t), sinf(t)) * r;
            } else if (emission_shape == 2) {
                float s = rng.randf(), t = TAU_F * rng.randf();
                float r = emission_sphere_radius * sqrtf(1.0f - s * s);
                p.xform.origin = Vec2(cosf(t), sinf(t)) * r;
            } else if (emission_shape == 3) {
                float rx = rng.randf() * 2.0f - 1.0f, ry = rng.randf() * 2.0f - 1.0f;
                p.xform.origin = Vec2(rx * emission_rect_extents.x, ry * emission_rect_extents.y);
            } else if ((emission_shape == 4 || emission_shape == 5) && !emission_points.empty()) {
                p.xform.origin = emission_points[rng.next() % emission_points.size()];
            }
            /* not local: emitted through the global transform */
            p.velocity = emission_.basis_xform(p.velocity);
            p.xform = emission_ * p.xform;
        } else if (!p.active) {
            continue;
        } else if (p.time > p.lifetime) {
            p.active = false;
            tv = 1.0f;
        } else {
            u32 s = p.seed;
            p.time += local_delta;
            p.custom[1] = (float)(p.time / lifetime);
            tv = (float)(p.time / p.lifetime);
            float tc[P_MAX];
            for (int k = 0; k < P_MAX; ++k) tc[k] = curves[k].valid() ? curves[k].sample(tv) : 1.0f;
            Vec2 force = gravity;
            Vec2 pos = p.xform.origin;
            float vlen = sqrtf(p.velocity.x * p.velocity.x + p.velocity.y * p.velocity.y);
            float la = tc[P_LINEAR_ACCEL] * lerpf(pmin[P_LINEAR_ACCEL], pmax[P_LINEAR_ACCEL], rand_from_seed(s));
            if (vlen > 0) force = force + p.velocity * (la / vlen);
            Vec2 diff = pos - emission_.origin;
            float dlen = sqrtf(diff.x * diff.x + diff.y * diff.y);
            float ra = tc[P_RADIAL_ACCEL] * lerpf(pmin[P_RADIAL_ACCEL], pmax[P_RADIAL_ACCEL], rand_from_seed(s));
            if (dlen > 0) force = force + diff * (ra / dlen);
            float ta = tc[P_TANGENTIAL_ACCEL] * lerpf(pmin[P_TANGENTIAL_ACCEL], pmax[P_TANGENTIAL_ACCEL], rand_from_seed(s));
            Vec2 yx(-diff.y, diff.x);
            if (dlen > 0) force = force + yx * (ta / dlen);
            p.velocity = p.velocity + force * (float)local_delta;
            float orbit = tc[P_ORBIT_VELOCITY] * lerpf(pmin[P_ORBIT_VELOCITY], pmax[P_ORBIT_VELOCITY], rand_from_seed(s));
            if (orbit != 0) {
                float ang = -orbit * (float)local_delta * TAU_F;
                Vec2 r(diff.x * cosf(ang) - diff.y * sinf(ang), diff.x * sinf(ang) + diff.y * cosf(ang));
                p.xform.origin = p.xform.origin - diff + r;
            }
            if (curves[P_VELOCITY].valid()) {
                float l = sqrtf(p.velocity.x * p.velocity.x + p.velocity.y * p.velocity.y);
                if (l > 0) p.velocity = p.velocity * (tc[P_VELOCITY] / l);
            }
            if (pmax[P_DAMPING] + tc[P_DAMPING] > 0) {
                float l = sqrtf(p.velocity.x * p.velocity.x + p.velocity.y * p.velocity.y);
                float damp = tc[P_DAMPING] * lerpf(pmin[P_DAMPING], pmax[P_DAMPING], rand_from_seed(s));
                float nl = l - damp * (float)local_delta;
                p.velocity = nl < 0 || l == 0 ? Vec2() : p.velocity * (nl / l);
            }
            float base_angle = tc[P_ANGLE] * lerpf(pmin[P_ANGLE], pmax[P_ANGLE], p.angle_rand);
            base_angle += p.custom[1] * (float)lifetime * tc[P_ANGULAR_VELOCITY] *
                          lerpf(pmin[P_ANGULAR_VELOCITY], pmax[P_ANGULAR_VELOCITY], rand_from_seed(s));
            p.rotation = deg2rad(base_angle);
            p.custom[0] = p.rotation; /* CUSTOM.x: the angle */
            p.custom[2] = tc[P_ANIM_OFFSET] * lerpf(pmin[P_ANIM_OFFSET], pmax[P_ANIM_OFFSET], p.anim_offset_rand) +
                          tv * tc[P_ANIM_SPEED] * lerpf(pmin[P_ANIM_SPEED], pmax[P_ANIM_SPEED], rand_from_seed(s));
        }
        Vec2 tex_scale(1, 1);
        if (split_scale) {
            tex_scale.x = scale_x.valid() ? scale_x.sample(tv) : 1.0f;
            tex_scale.y = scale_y.valid() ? scale_y.sample(tv) : 1.0f;
        } else if (curves[P_SCALE].valid()) {
            float t = curves[P_SCALE].sample(tv);
            tex_scale = Vec2(t, t);
        }
        float thv = curves[P_HUE_VARIATION].valid() ? curves[P_HUE_VARIATION].sample(tv) : 0.0f;
        float hra = thv * TAU_F * lerpf(pmin[P_HUE_VARIATION], pmax[P_HUE_VARIATION], p.hue_rot_rand);
        float hc = cosf(hra), hs = sinf(hra);
        static const float m1[9] = { 0.299f, 0.587f, 0.114f, 0.299f, 0.587f, 0.114f, 0.299f, 0.587f, 0.114f };
        static const float m2[9] = { 0.701f, -0.587f, -0.114f, -0.299f, 0.413f, -0.114f, -0.300f, -0.588f, 0.886f };
        static const float m3[9] = { 0.168f, 0.330f, -0.497f, -0.328f, 0.035f, 0.292f, 1.250f, -1.050f, -0.203f };
        float hm[9];
        for (int k = 0; k < 9; ++k) hm[k] = m1[k] + m2[k] * hc + m3[k] * hs;
        p.color = color_ramp.valid() ? color_ramp.at(tv) * color : color;
        /* Basis::xform_inv: transpose multiply */
        float r = hm[0] * p.color.r + hm[3] * p.color.g + hm[6] * p.color.b;
        float g = hm[1] * p.color.r + hm[4] * p.color.g + hm[7] * p.color.b;
        float b = hm[2] * p.color.r + hm[5] * p.color.g + hm[8] * p.color.b;
        p.color = Color(r, g, b, p.color.a) * p.base_color * p.start_color_rand;
        if (alpha_curve.valid()) p.color.a *= alpha_curve.sample(tv);
        if (align_y) {
            float l = sqrtf(p.velocity.x * p.velocity.x + p.velocity.y * p.velocity.y);
            if (l > 0) p.xform.y = p.velocity;
            float yl = sqrtf(p.xform.y.x * p.xform.y.x + p.xform.y.y * p.xform.y.y);
            if (yl > 0) p.xform.y = p.xform.y / yl;
            p.xform.x = Vec2(p.xform.y.y, -p.xform.y.x);
        } else {
            p.xform.x = Vec2(cosf(p.rotation), -sinf(p.rotation));
            p.xform.y = Vec2(sinf(p.rotation), cosf(p.rotation));
        }
        float bs = lerpf(pmin[P_SCALE], pmax[P_SCALE], p.scale_rand);
        Vec2 base_scale(maxf(tex_scale.x * bs, 0.00001f), maxf(tex_scale.y * bs, 0.00001f));
        p.xform.x = p.xform.x * base_scale.x;
        p.xform.y = p.xform.y * base_scale.y;
        p.xform.origin = p.xform.origin + p.velocity * (float)local_delta;
    }
}

/* ---- GPUParticles2D: the ParticleProcessMaterial shader ---- */

/* The shader's 4D noise and curl (its "Functions for 3D noise /
 * turbulence"). */
/* floorf, without the library call (the noise takes ~300 per particle
 * and step); floats past 2^23 are whole already */
static float fast_floor(float x) {
    if (x >= 8388608.0f || x <= -8388608.0f) return x;
    int i = (int)x;
    return (float)(i - (x < (float)i ? 1 : 0));
}

static float fractf(float x) { return x - fast_floor(x); }

static void noise_grad(const float in[4], float out[4]) {
    static const float M[4][4] = { { 0.143081f, 0.001724f, 0.280166f, 0.262771f },
                                   { 0.645401f, -0.047791f, -0.146698f, 0.595016f },
                                   { -0.499665f, -0.095734f, 0.425674f, -0.207367f },
                                   { -0.013596f, -0.848588f, 0.423736f, 0.17044f } };
    float p[4];
    for (int i = 0; i < 4; ++i) p[i] = fractf(M[i][0] * in[0] + M[i][1] * in[1] + M[i][2] * in[2] + M[i][3] * in[3]);
    for (int i = 0; i < 4; ++i) out[i] = fractf(p[i] * p[(i + 1) & 3] * 2365.952041f) * 2.0f - 1.0f;
}

static float noise4(const float in[4]) {
    /* domain rotation */
    float d = (in[0] + in[1] + in[2]) * -0.1666667f + in[3] * -0.5f;
    float c[4] = { in[0] + d, in[1] + d, in[2] + d, (in[0] + in[1] + in[2] + in[3]) * 0.5f };
    float base[4], delta[4], fade[4];
    for (int i = 0; i < 4; ++i) {
        base[i] = fast_floor(c[i]);
        delta[i] = c[i] - base[i];
        fade[i] = delta[i] * delta[i] * delta[i] * (10.0f + delta[i] * (-15.0f + delta[i] * 6.0f));
    }
    /* the corners' gradients dotted with the offsets (bit 0 x .. bit 3 w) */
    float r[16];
    for (int k = 0; k < 16; ++k) {
        float off[4] = { (float)(k & 1), (float)((k >> 1) & 1), (float)((k >> 2) & 1), (float)((k >> 3) & 1) };
        float bp[4] = { base[0] + off[0], base[1] + off[1], base[2] + off[2], base[3] + off[3] };
        float g[4];
        noise_grad(bp, g);
        r[k] = (delta[0] - off[0]) * g[0] + (delta[1] - off[1]) * g[1] + (delta[2] - off[2]) * g[2] +
               (delta[3] - off[3]) * g[3];
    }
    float wz[4];
    for (int j = 0; j < 4; ++j)
        wz[j] = lerpf(lerpf(r[j], r[8 + j], fade[3]), lerpf(r[4 + j], r[12 + j], fade[3]), fade[2]);
    return lerpf(lerpf(wz[0], wz[2], fade[1]), lerpf(wz[1], wz[3], fade[1]), fade[0]);
}

static void noise_3x(const float p[4], float out[3]) {
    const float w = 1.7320508f * 2048.333333f;
    float a[4] = { p[0], p[1], p[2], p[3] + w }, b[4] = { p[0], p[1], p[2], p[3] - w };
    out[0] = noise4(p);
    out[1] = noise4(a);
    out[2] = noise4(b);
}

static void curl_3d(const float p[4], float c, float out[3]) {
    float eps = 0.001f + c;
    float n[3][2][3]; /* axis, -/+, the three noises */
    for (int axis = 0; axis < 3; ++axis)
        for (int side = 0; side < 2; ++side) {
            float q[4] = { p[0], p[1], p[2], p[3] };
            q[axis] += side ? eps : -eps;
            noise_3x(q, n[axis][side]);
        }
    float x = (n[1][1][2] - n[1][0][2]) - (n[2][1][1] - n[2][0][1]);
    float y = (n[2][1][0] - n[2][0][0]) - (n[0][1][2] - n[0][0][2]);
    float z = (n[0][1][1] - n[0][0][1]) - (n[1][1][0] - n[1][0][0]);
    float l = sqrtf(x * x + y * y + z * z);
    float k = l > 0 ? 1.0f / l : 0.0f;
    out[0] = x * k;
    out[1] = y * k;
    out[2] = z * k;
}

/* get_noise_direction (TIME: the shaders' time) */
Vec2 Particles2D::noise_direction(const float pos[3], float* z) const {
    float adj_contrast = maxf(mat.turb_strength - 1.0f, 0.0f) * 70.0f;
    float t = (float)g_shader_time;
    float p[4] = { pos[0] * mat.turb_scale + t * mat.turb_speed[0], pos[1] * mat.turb_scale + t * mat.turb_speed[1],
                   pos[2] * mat.turb_scale + t * mat.turb_speed[2], t * mat.turb_speed_random };
    float d[3];
    curl_3d(p, adj_contrast, d);
    /* mix(0.9 * d, d, strength - 9) */
    float k = 0.9f + 0.1f * (mat.turb_strength - 9.0f);
    *z = d[2] * k;
    return Vec2(d[0] * k, d[1] * k);
}

/* get_random_direction_from_spread (2D) */
static Vec2 spread_direction(u32& s, const float direction[3], float spread_deg) {
    float spread_rad = spread_deg * (SHADER_PI / 180.0f);
    float a = (rand_from_seed(s) * 2.0f - 1.0f) * spread_rad;
    a += direction[0] != 0.0f ? atan2f(direction[1], direction[0]) : signf(direction[1]) * (SHADER_PI / 2.0f);
    return Vec2(cosf(a), sinf(a));
}

/* start() */
void Particles2D::gpu_start(Particle& p, u32 number, const Emission* from) {
    u32 s = idhash(number + 1u + seed);
    Drawn& d = p.d;
    /* calculate_initial_display_params */
    d.scale = lerpf(pmin[P_SCALE], pmax[P_SCALE], rand_from_seed(s));
    d.scale = signf(d.scale) * maxf(fabsf(d.scale), 0.001f);
    d.hue_rotation = SHADER_PI * 2.0f * lerpf(pmin[P_HUE_VARIATION], pmax[P_HUE_VARIATION], rand_from_seed(s));
    d.anim_speed = lerpf(pmin[P_ANIM_SPEED], pmax[P_ANIM_SPEED], rand_from_seed(s));
    d.anim_offset = lerpf(pmin[P_ANIM_OFFSET], pmax[P_ANIM_OFFSET], rand_from_seed(s));
    d.life = (float)(1.0 - lifetime_randomness * rand_from_seed(s));
    d.color = color;
    if (color_initial_ramp.valid()) d.color = d.color * color_initial_ramp.at(rand_from_seed(s));
    if (emission_shape == 4 || emission_shape == 5) rand_from_seed(s); /* emission_texture_position */
    /* calculate_initial_dynamics_params */
    d.angle = lerpf(pmin[P_ANGLE], pmax[P_ANGLE], rand_from_seed(s));
    d.angular_velocity = lerpf(pmin[P_ANGULAR_VELOCITY], pmax[P_ANGULAR_VELOCITY], rand_from_seed(s));
    d.initial_velocity = lerpf(pmin[P_VELOCITY], pmax[P_VELOCITY], rand_from_seed(s));
    d.directional = lerpf(mat.directional_min, mat.directional_max, rand_from_seed(s));
    d.radial = lerpf(mat.radial_velocity_min, mat.radial_velocity_max, rand_from_seed(s));
    d.orbit = lerpf(pmin[P_ORBIT_VELOCITY], pmax[P_ORBIT_VELOCITY], rand_from_seed(s));
    d.turb_influence = lerpf(mat.turb_influence_min, mat.turb_influence_max, rand_from_seed(s));
    /* calculate_initial_physical_params */
    d.linear_accel = lerpf(pmin[P_LINEAR_ACCEL], pmax[P_LINEAR_ACCEL], rand_from_seed(s));
    d.radial_accel = lerpf(pmin[P_RADIAL_ACCEL], pmax[P_RADIAL_ACCEL], rand_from_seed(s));
    d.tangent_accel = lerpf(pmin[P_TANGENTIAL_ACCEL], pmax[P_TANGENTIAL_ACCEL], rand_from_seed(s));
    d.damping = lerpf(pmin[P_DAMPING], pmax[P_DAMPING], rand_from_seed(s));
    d.seed = s;
    if (rand_from_seed(s) > mat.amount_ratio) {
        p.active = false;
        return;
    }
    p.custom[0] = d.angle;
    p.custom[1] = p.custom[2] = 0;
    p.custom[3] = d.life;
    p.time = 0;
    p.trail.clear();
    if (from) {
        /* a sub-emission: its transform kept (no RESTART_POSITION /
         * ROT_SCALE), its velocity if sent; VELOCITY then goes through the
         * emission transform like any other */
        p.xform = from->xform;
        Vec2 v = from->has_velocity ? from->velocity : spread_direction(s, mat.direction, spread) * d.initial_velocity;
        p.velocity = emission_.basis_xform(v) + emitter_velocity_ * mat.inherit_velocity_ratio;
        p.vz = from->has_velocity ? from->vz : 0.0f;
        return;
    }
    /* calculate_initial_position */
    float pos[3] = { 0, 0, 0 };
    if (emission_shape == 1 || emission_shape == 2) {
        float sz = rand_from_seed(s) * 2.0f - 1.0f;
        float t = rand_from_seed(s) * 2.0f * SHADER_PI;
        float f = emission_shape == 1 ? rand_from_seed(s) : 1.0f;
        float radius = emission_sphere_radius * sqrtf(1.0f - sz * sz);
        pos[0] = radius * cosf(t) * f;
        pos[1] = radius * sinf(t) * f;
        pos[2] = emission_sphere_radius * sz * f;
    } else if (emission_shape == 3) {
        for (int k = 0; k < 3; ++k) pos[k] = (rand_from_seed(s) * 2.0f - 1.0f) * mat.emission_box[k];
    } else if (emission_shape == 6) {
        float rc = maxf(0.001f, mat.ring_radius);
        float top = maxf(rc - tanf((90.0f - mat.ring_cone_angle) * 0.017453293f) * mat.ring_height, 0.0f);
        float y_pos = rand_from_seed(s);
        float skew = maxf(minf(rc, top) / maxf(rc, top), 0.5f);
        y_pos = rc < top ? powf(y_pos, skew) : 1.0f - powf(y_pos, skew);
        float angle = rand_from_seed(s) * 2.0f * SHADER_PI;
        float ri = mat.ring_inner_radius;
        float rr = sqrtf(rand_from_seed(s) * (rc * rc - ri * ri) + ri * ri);
        rr = lerpf(rr, rr * (top / rc), y_pos);
        float ax[3] = { mat.ring_axis[0], mat.ring_axis[1], mat.ring_axis[2] };
        float al = sqrtf(ax[0] * ax[0] + ax[1] * ax[1] + ax[2] * ax[2]);
        if (al == 0) {
            ax[0] = ax[1] = 0;
            ax[2] = 1;
        } else {
            for (int k = 0; k < 3; ++k) ax[k] /= al;
        }
        /* an axis across the ring: axis x (0, 1, 0) along x, else axis x (1, 0, 0) */
        float o[3];
        if (fabsf(ax[0]) == 1.0f && ax[1] == 0.0f && ax[2] == 0.0f) {
            o[0] = -ax[2];
            o[1] = 0;
            o[2] = ax[0];
        } else {
            o[0] = 0;
            o[1] = ax[2];
            o[2] = -ax[1];
        }
        float ol = sqrtf(o[0] * o[0] + o[1] * o[1] + o[2] * o[2]);
        if (ol > 0)
            for (int k = 0; k < 3; ++k) o[k] /= ol;
        /* rotated about the axis by the spawn angle (the shader's mat3,
         * columns) */
        float sn = sinf(angle), cs = cosf(angle), oc = 1.0f - cs;
        float c0[3] = { cs + ax[0] * ax[0] * oc, ax[0] * ax[1] * oc - ax[2] * sn, ax[0] * ax[2] * oc + ax[1] * sn };
        float c1[3] = { ax[0] * ax[1] * oc + sn * ax[2], cs + ax[1] * ax[1] * oc, ax[1] * ax[2] * oc - ax[0] * sn };
        float c2[3] = { ax[2] * ax[0] * oc - ax[1] * sn, ax[2] * ax[1] * oc + ax[0] * sn, cs + ax[2] * ax[2] * oc };
        float h = y_pos * mat.ring_height - mat.ring_height / 2.0f;
        for (int k = 0; k < 3; ++k) pos[k] = (c0[k] * o[0] + c1[k] * o[1] + c2[k] * o[2]) * rr + h * ax[k];
    }
    for (int k = 0; k < 3; ++k) pos[k] = pos[k] * mat.shape_scale[k] + mat.shape_offset[k];
    if (mat.turbulence) {
        float disp = lerpf(mat.turb_displacement_min, mat.turb_displacement_max, rand_from_seed(s));
        float nz;
        Vec2 n = noise_direction(pos, &nz);
        pos[0] += n.x * disp;
        pos[1] += n.y * disp;
    }
    /* TRANSFORM = EMISSION_TRANSFORM * TRANSFORM, z dropped */
    p.xform = emission_;
    p.xform.origin = emission_.xform(Vec2(pos[0], pos[1]));
    Vec2 v = spread_direction(s, mat.direction, spread) * d.initial_velocity;
    p.velocity = emission_.basis_xform(v) + emitter_velocity_ * mat.inherit_velocity_ratio;
    p.vz = 0;
}

/* process_display_param at a life: the scale, animation and colour */
void Particles2D::gpu_display(const Particle& p, float life, float scale[3], float* anim_offset, float* anim_speed,
                              Color* color) const {
    const Drawn& d = p.d;
    for (int k = 0; k < 3; ++k) scale[k] = d.scale * (mat.scale.valid() ? mat.scale.channel(k, life) : 1.0f);
    float hue = d.hue_rotation * (curves[P_HUE_VARIATION].valid() ? curves[P_HUE_VARIATION].sample(life) : 1.0f);
    *anim_offset = d.anim_offset + (curves[P_ANIM_OFFSET].valid() ? curves[P_ANIM_OFFSET].sample(life) : 0.0f);
    *anim_speed = d.anim_speed * (curves[P_ANIM_SPEED].valid() ? curves[P_ANIM_SPEED].sample(life) : 1.0f);
    Color c = d.color;
    if (color_ramp.valid()) c = c * color_ramp.at(life);
    if (alpha_curve.valid()) c.a *= alpha_curve.sample(life);
    if (hue != 0) {
        /* rotate_hue: the luma-preserving rotation, transposed */
        static const float m1[9] = { 0.299f, 0.587f, 0.114f, 0.299f, 0.587f, 0.114f, 0.299f, 0.587f, 0.114f };
        static const float m2[9] = { 0.701f, -0.587f, -0.114f, -0.299f, 0.413f, -0.114f, -0.300f, -0.588f, 0.886f };
        static const float m3[9] = { 0.168f, 0.330f, -0.497f, -0.328f, 0.035f, 0.292f, 1.250f, -1.050f, -0.203f };
        float hc = cosf(hue), hs = sinf(hue), hm[9];
        for (int k = 0; k < 9; ++k) hm[k] = m1[k] + m2[k] * hc + m3[k] * hs;
        c = Color(hm[0] * c.r + hm[3] * c.g + hm[6] * c.b, hm[1] * c.r + hm[4] * c.g + hm[7] * c.b,
                  hm[2] * c.r + hm[5] * c.g + hm[8] * c.b, c.a);
    }
    if (mat.emission.valid()) {
        float e = 1.0f + mat.emission.r(life);
        c = Color(c.r * e, c.g * e, c.b * e, c.a);
    }
    *color = c;
}

/* process() (disable_z: the particles stay in the plane) */
void Particles2D::gpu_process(Particle& p, double dt_d, bool collided, const Vec2& cn, float cdepth,
                              Particles2D* sub) {
    float dt = (float)dt_d;
    const Drawn& d = p.d;
    u32 s = d.seed;
    int sub_mode = sub ? mat.sub_mode : 0;
    bool just_spawned = p.custom[1] == 0.0f;
    p.custom[1] += (float)(dt_d / lifetime);
    p.custom[1] = lerpf(p.custom[1], 1.0f, mat.interp_to_end);
    float lp = p.custom[1] / d.life;
    /* ACTIVE = false; the rest still runs (an emission at the end) */
    if (p.custom[1] > p.custom[3]) {
        p.active = false;
        if (sub_mode != 2) return;
    }
    Vec2 pos = p.xform.origin;
    /* the controlled displacement: orbit, radial, directional */
    Vec2 ctrl(0, 0);
    float ctrl_z = 0;
    if (!(fabsf(d.orbit) < 0.01f || dt < 0.001f)) {
        float amount = d.orbit;
        if (mat.orbit.valid()) amount *= mat.orbit.channel(mat.orbit.kind == CurveTex::XYZ ? 2 : 0, lp);
        Vec2 disp(0, 0);
        if (amount != 0) {
            Vec2 diff = pos - emission_.origin;
            float ang = amount * SHADER_PI * 2.0f * dt;
            float c = cosf(ang), sn = sinf(ang);
            disp = Vec2(c * diff.x + sn * diff.y, -sn * diff.x + c * diff.y) - diff;
        }
        ctrl = ctrl + emission_.basis_xform(disp * (1.0f / dt));
    }
    if (dt >= 0.001f) {
        float mult = mat.radial_velocity.valid() ? mat.radial_velocity.r(lp) : 1.0f;
        Vec2 pivot = emission_.xform(Vec2(mat.velocity_pivot[0], mat.velocity_pivot[1]));
        float rel[3] = { pos.x - pivot.x, pos.y - pivot.y, -mat.velocity_pivot[2] };
        float len = sqrtf(rel[0] * rel[0] + rel[1] * rel[1] + rel[2] * rel[2]);
        float disp[3];
        if (len > 0.01f) {
            for (int k = 0; k < 3; ++k) disp[k] = rel[k] / len * mult * d.radial;
        } else {
            Vec2 r = spread_direction(s, mat.direction, 360.0f) * d.radial;
            disp[0] = r.x;
            disp[1] = r.y;
            disp[2] = 0;
        }
        float rv = mult * d.radial;
        if (rv < 0) {
            /* no flicker once an inward particle reaches the pivot */
            float dl = sqrtf(disp[0] * disp[0] + disp[1] * disp[1] + disp[2] * disp[2]);
            float want = minf(fabsf(rv), len / dt);
            for (int k = 0; k < 3; ++k) disp[k] = dl > 0 ? disp[k] / dl * want : 0.0f;
        }
        ctrl = ctrl + Vec2(disp[0], disp[1]);
        ctrl_z += disp[2];
    }
    if (mat.directional.valid()) {
        Vec2 v(mat.directional.channel(0, lp) * d.directional, mat.directional.channel(1, lp) * d.directional);
        ctrl = ctrl + emission_.basis_xform(v);
        ctrl_z += mat.directional.channel(2, lp) * d.directional;
    }
    /* process_physical_parameters and the forces */
    float linear = d.linear_accel * (curves[P_LINEAR_ACCEL].valid() ? curves[P_LINEAR_ACCEL].sample(lp) : 1.0f);
    float radial = d.radial_accel * (curves[P_RADIAL_ACCEL].valid() ? curves[P_RADIAL_ACCEL].sample(lp) : 1.0f);
    float tangent =
        d.tangent_accel * (curves[P_TANGENTIAL_ACCEL].valid() ? curves[P_TANGENTIAL_ACCEL].sample(lp) : 1.0f);
    float damping = d.damping * (curves[P_DAMPING].valid() ? curves[P_DAMPING].sample(lp) : 1.0f);
    Vec2 force = gravity;
    float vlen = sqrtf(p.velocity.x * p.velocity.x + p.velocity.y * p.velocity.y + p.vz * p.vz);
    if (vlen > 0) force = force + p.velocity * (linear / vlen);
    Vec2 diff = pos - emission_.origin;
    float dlen = sqrtf(diff.x * diff.x + diff.y * diff.y);
    if (dlen > 0) {
        force = force + diff * (radial / dlen);
        force = force + Vec2(-diff.y, diff.x) * (tangent / dlen);
    }
    p.velocity = p.velocity + force * dt;
    if (damping > 0) {
        float v = sqrtf(p.velocity.x * p.velocity.x + p.velocity.y * p.velocity.y + p.vz * p.vz);
        float nv = v;
        if (!mat.damping_as_friction) nv -= damping * dt;
        else if (nv > 0.001f) nv -= nv * nv * damping * 0.05f * dt; /* a particle of 0.05 kg */
        if (nv < 0 || v == 0) {
            p.velocity = Vec2();
            p.vz = 0;
        } else {
            p.velocity = p.velocity * (nv / v);
            p.vz *= nv / v;
        }
    }
    if (collided && collision_mode == 2) {
        p.active = false;
        if (sub_mode == 3)
            for (int k = 0; k < mat.sub_at_collision; ++k) emit_to(sub, p);
        return;
    }
    if (collided) {
        /* the rigid response, in the canvas */
        Xform2D back = to_canvas_.affine_inverse();
        Vec2 at = to_canvas_.xform(p.xform.origin) + cn * cdepth;
        Vec2 v = to_canvas_.basis_xform(p.velocity);
        float response = cn.x * v.x + cn.y * v.y;
        float bounce_edge = 2.0f / clampf(collision_bounce + 1.0f, 1.0f, 2.0f);
        float trigger = fabsf(response) >= bounce_edge ? 1.0f : 0.0f;
        v = v - cn * response;
        float keep = 1.0f - clampf(collision_friction, 0.0f, 1.0f);
        v = v * keep;
        p.vz *= keep;
        v = v - cn * (response * collision_bounce * trigger);
        p.xform.origin = back.xform(at);
        p.velocity = back.basis_xform(v);
        pos = p.xform.origin;
    }
    if (mat.turbulence) {
        float life_infl = mat.turb_influence_over_life.valid() ? mat.turb_influence_over_life.r(lp) : 1.0f;
        float at[3] = { pos.x, pos.y, 0 };
        float nz;
        Vec2 n = noise_direction(at, &nz);
        float nl = sqrtf(n.x * n.x + n.y * n.y + nz * nz);
        if (!collided && nl > 0) {
            n = n * (1.0f / nl);
            nz /= nl;
            float infl = clampf(d.turb_influence * life_infl, 0.0f, 1.0f) * (dt <= 0 ? 0.0f : 1.0f);
            float k = 1.0f + (1.0f - infl) * 0.2f;
            float vm = sqrtf(p.velocity.x * p.velocity.x + p.velocity.y * p.velocity.y + p.vz * p.vz) * k;
            p.velocity = Vec2(lerpf(p.velocity.x, n.x * vm, infl), lerpf(p.velocity.y, n.y * vm, infl));
            p.vz = lerpf(p.vz, nz * vm, infl);
            float cm = sqrtf(ctrl.x * ctrl.x + ctrl.y * ctrl.y + ctrl_z * ctrl_z) * k;
            ctrl = Vec2(lerpf(ctrl.x, n.x * cm, infl), lerpf(ctrl.y, n.y * cm, infl));
            ctrl_z = lerpf(ctrl_z, nz * cm, infl);
        }
    }
    Vec2 fv = ctrl + p.velocity;
    float fz = ctrl_z + p.vz;
    if (mat.velocity_limit.valid()) {
        float fl = sqrtf(fv.x * fv.x + fv.y * fv.y + fz * fz);
        if (fl > 0.001f) {
            float k = minf(fl, fabsf(mat.velocity_limit.r(lp))) / fl;
            fv = fv * k;
        }
    }
    p.xform.origin = pos + fv * dt;
    float scale[3], anim_offset, anim_speed;
    gpu_display(p, lp, scale, &anim_offset, &anim_speed, &p.color);
    float base_angle = d.angle;
    if (curves[P_ANGLE].valid()) base_angle *= curves[P_ANGLE].sample(lp);
    float spin = d.angular_velocity;
    if (curves[P_ANGULAR_VELOCITY].valid()) spin *= curves[P_ANGULAR_VELOCITY].sample(lp);
    base_angle += p.custom[1] * (float)lifetime * spin;
    p.custom[0] = base_angle * (SHADER_PI / 180.0f);
    float fl = sqrtf(fv.x * fv.x + fv.y * fv.y);
    if (align_y) {
        Vec2 y = fl > 0 ? fv * (1.0f / fl) : p.xform.y;
        float yl = sqrtf(y.x * y.x + y.y * y.y);
        if (yl > 0) y = y * (1.0f / yl);
        p.xform.y = y;
        p.xform.x = Vec2(y.y, -y.x);
    } else {
        p.xform.x = Vec2(cosf(p.custom[0]), -sinf(p.custom[0]));
        p.xform.y = Vec2(sinf(p.custom[0]), cosf(p.custom[0]));
    }
    if (mat.scale_over_velocity.valid()) {
        float range = mat.scale_over_velocity_max - mat.scale_over_velocity_min;
        float t = range != 0 ? clampf(fl / range, 0.0f, 1.0f) : 0.0f;
        for (int k = 0; k < 3; ++k) scale[k] *= mat.scale_over_velocity.channel(k, t);
    }
    p.xform.x = p.xform.x * (signf(scale[0]) < 0 ? -1.0f : 1.0f) * maxf(fabsf(scale[0]), 0.001f);
    p.xform.y = p.xform.y * (signf(scale[1]) < 0 ? -1.0f : 1.0f) * maxf(fabsf(scale[1]), 0.001f);
    p.custom[2] = anim_offset + lp * anim_speed;
    /* the sub-emitter's emissions */
    int emit_count = 0;
    if (sub_mode == 1 && mat.sub_interval > 0) {
        float from = p.custom[1] * (float)lifetime - dt;
        float rem = mat.sub_interval - (from - mat.sub_interval * floorf(from / mat.sub_interval));
        if (dt >= rem) emit_count = 1;
    } else if (sub_mode == 2) {
        if (p.custom[1] / p.custom[3] * (float)lifetime > (float)lifetime - dt) emit_count = mat.sub_at_end;
    } else if (sub_mode == 3) {
        if (collided) emit_count = mat.sub_at_collision;
    } else if (sub_mode == 4) {
        if (just_spawned) emit_count = mat.sub_at_start;
    }
    for (int k = 0; k < emit_count; ++k) emit_to(sub, p);
}

/* emit_subparticle: into the sub-emitter's buffer (its amount at most) */
void Particles2D::emit_to(Particles2D* sub, const Particle& p) {
    if ((int)sub->pending_.size() >= sub->amount) return;
    Emission e;
    e.xform = p.xform;
    e.velocity = p.velocity;
    e.vz = p.vz;
    e.has_velocity = mat.sub_keep_velocity;
    sub->pending_.push_back(e);
}

/* the system sub_emitter names (it then emits only for this one) */
Particles2D* Particles2D::sub_target() {
    if (sub_emitter.empty() || !mat.sub_mode) return NULL;
    Node* n = get_node(sub_emitter);
    if (!n || n == this || n->type != "GPUParticles2D") return NULL;
    Particles2D* s = (Particles2D*)n;
    s->sub_mode_ = true;
    return s;
}

/* particles.glsl's frame: each particle restarts at its phase of the
 * system's cycle (its number: the cycle it started in), then the
 * material's process() */
void Particles2D::step_gpu(double delta) {
    int total = (int)particles_.size();
    double prev_phase = time_ / lifetime;
    time_ += delta;
    clock_ += delta;
    if (time_ > lifetime) {
        time_ = fmod(time_, lifetime);
        ++cycle_;
        if (one_shot) emitting = false;
    }
    double phase = time_ / lifetime;
    Particles2D* sub = sub_target();
    for (int i = 0; i < total; ++i) {
        Particle& p = particles_[i];
        double local_delta = delta;
        bool restart = false;
        Emission from;
        bool emitted = false;
        if (sub_mode_) {
            /* a sub-emitter's free slot takes a sent emission */
            if (!p.active && !pending_.empty()) {
                from = pending_.back();
                pending_.pop_back();
                restart = emitted = true;
            }
        } else if (emitting) {
            double restart_phase = double(i) / double(total);
            if (randomness > 0.0) {
                u32 s = cycle_;
                if (restart_phase >= phase) s -= 1u;
                s *= (u32)total;
                s += (u32)i;
                double r = double(idhash(s) % 65536u) / 65536.0;
                restart_phase += randomness * r * 1.0 / double(total);
            }
            restart_phase *= (1.0 - explosiveness);
            if (phase > prev_phase) {
                if (restart_phase >= prev_phase && restart_phase < phase) {
                    restart = true;
                    if (fractional_delta) local_delta = (phase - restart_phase) * lifetime;
                }
            } else if (delta > 0.0) {
                if (restart_phase >= prev_phase) {
                    restart = true;
                    if (fractional_delta) local_delta = (1.0 - restart_phase + phase) * lifetime;
                } else if (restart_phase < phase) {
                    restart = true;
                    if (fractional_delta) local_delta = (phase - restart_phase) * lifetime;
                }
            }
        }
        if (restart) {
            p.active = true;
            p.cycle = cycle_;
            gpu_start(p, p.cycle * (u32)total + (u32)i, emitted ? &from : NULL);
        }
        if (!p.active) continue;
        Vec2 n;
        float depth = 0;
        bool collided = collision_mode != 0 && collision(p, &n, &depth);
        gpu_process(p, local_delta, collided, n, depth, sub);
        if (trail_enabled && p.active) {
            /* the history the trail needs (one point before its window) */
            p.trail.push_back(std::make_pair(clock_, p.xform));
            while (p.trail.size() > 2 && p.trail[1].first <= clock_ - trail_lifetime) p.trail.erase(p.trail.begin());
        }
    }
    /* what no free slot took is dropped with the frame's buffer */
    pending_.clear();
}

/* particles.glsl's 2D SDF collision: the canvas SDF of the
 * LightOccluder2D polygons at the particle, less its size. */
bool Particles2D::collision(const Particle& p, Vec2* normal, float* depth) {
    const Node* root = this;
    while (root->parent) root = root->parent;
    Vec2 pos = to_canvas_.xform(p.xform.origin);
    float d;
    Vec2 n;
    if (!LightOccluder2D::sdf(root, pos, &d, &n)) return false;
    float size = collision_base_size;
    if (collision_use_scale) {
        Vec2 sx = to_canvas_.basis_xform(p.xform.x), sy = to_canvas_.basis_xform(p.xform.y);
        size *= (sqrtf(sx.x * sx.x + sx.y * sx.y) + sqrtf(sy.x * sy.x + sy.y * sy.y) + 1.0f) / 3.0f;
    } else {
        size *= sqrtf(to_canvas_.y.x * to_canvas_.y.x + to_canvas_.y.y * to_canvas_.y.y);
    }
    d -= size;
    if (d >= 0) return false;
    *normal = n;
    *depth = -d;
    return true;
}

/* A trailed particle's transform age seconds ago, between the recorded
 * ones (its first before it started). */
Xform2D Particles2D::trail_at(const Particle& p, double age) const {
    const std::vector<std::pair<double, Xform2D> >& h = p.trail;
    if (h.empty()) return p.xform;
    double t = clock_ - age;
    if (t <= h[0].first) return h[0].second;
    for (size_t i = 1; i < h.size(); ++i) {
        if (t > h[i].first) continue;
        double span = h[i].first - h[i - 1].first;
        float f = span > 0 ? (float)((t - h[i - 1].first) / span) : 1.0f;
        const Xform2D& a = h[i - 1].second;
        const Xform2D& b = h[i].second;
        return Xform2D(a.x + (b.x - a.x) * f, a.y + (b.y - a.y) * f, a.origin + (b.origin - a.origin) * f);
    }
    return h.back().second;
}

/* ---- draw: a texture-sized quad per particle, CanvasItemMaterial
 * particle animation frames ---- */

void Particles2D::draw(Canvas& c) {
    if (particles_.empty()) return;
    /* not local: drawn in the canvas (Godot's identity item transform) */
    Xform2D view = local_coords ? Xform2D() : global_transform().affine_inverse();
    Vec2 ts = texture ? texture->size() : Vec2(1, 1);
    int hf = 1, vf = 1;
    bool loop = false, anim = false;
    if (material && material->props) {
        const Json& mp = *material->props;
        anim = mp["particles_animation"].b();
        hf = mp.has("particles_anim_h_frames") ? mp["particles_anim_h_frames"].i(1) : 1;
        vf = mp.has("particles_anim_v_frames") ? mp["particles_anim_v_frames"].i(1) : 1;
        loop = mp["particles_anim_loop"].b();
        if (hf < 1) hf = 1;
        if (vf < 1) vf = 1;
    }
    /* The game's flipbook VFX shaders: the frame from the particle's life
     * (vfx_flipbook_shader / vfx_row_flipbook_shader's vertex()), the quad
     * moved by pivot_offset texture sizes. */
    int fb = 0; /* 1 flipbook, 2 row flipbook */
    float fb_frames = 1;
    Vec2 fb_size(1, 1), pivot(0, 0);
    CurveData fb_curve;
    const MaterialRes* sm = material && material->kind == Resource::RES_SHADER_MATERIAL ? material : NULL;
    if (sm) {
        if (sm->shader == "shaders/vfx/common/vfx_flipbook_shader.gdshader") fb = 1;
        if (sm->shader == "shaders/vfx/common/vfx_row_flipbook_shader.gdshader") fb = 2;
        if (sm->shader == "shaders/vfx/common/vfx_common_particle_shader.gdshader" ||
            sm->shader == "shaders/vfx/common/vfx_round_smoke_shader.gdshader" ||
            sm->shader == "shaders/vfx/ui/card/exhaust/vfx_card_exhaust_quick_erosion_shader.gdshader")
            fb = 3;
        pivot = shader_param_vec2(sm, "pivot_offset", Vec2(0, 0));
        if (fb) {
            /* the shader's own defaults */
            bool quick = sm->shader == "shaders/vfx/ui/card/exhaust/vfx_card_exhaust_quick_erosion_shader.gdshader";
            fb_frames = shader_param(sm, "frame_count", quick ? 4.0f : 9.0f);
            fb_size = shader_param_vec2(sm, "flipbook_size", quick ? Vec2(2, 2) : Vec2(3, 3));
            if (fb_size.x < 1) fb_size.x = 1;
            if (fb_size.y < 1) fb_size.y = 1;
            CurveData::from(shader_param_resource(sm, "flipbook_curve"), fb_curve);
        }
    }
    /* the image and uv rect behind the texture */
    ImageTextureRes* img = NULL;
    Rect2 dst, src;
    if (texture) texture->get_rect_region(Rect2(Vec2(), ts), Rect2(Vec2(), ts), dst, src, &img);
    if (texture && !img) return;
    Vec2 is = img ? img->size() : Vec2(1, 1);
    /* The erosion shaders: each particle drawn on its own, eroded by the
     * curve at its life. */
    const VfxParams* vfx = shader_vfx();
    EffectParams effect;
    bool per_effect = shader_effect_particle(0, 0, &effect);
    bool ray = sm && sm->shader == "shaders/vfx/common/vfx_ray_shader.gdshader";
    /* vfx_sweeping_beam_flipbook_shader's vertex(): the quad grows by
     * tan(fov / 2) * (1 - inset) of its size */
    float grow = 1;
    if (sm && sm->shader == "shaders/vfx/sweeping_beam/vfx_sweeping_beam_flipbook_shader.gdshader")
        grow = 1 + tanf(shader_param(sm, "fov", 90.0f) * 0.017453293f * 0.5f) * (1 - shader_param(sm, "inset", 0.0f));
    /* a screen shader per particle (vfx_screen_chromatic_aberration) */
    ScreenParams screen;
    bool per_screen = shader_screen_particle(0, &screen);
    CurveData erosion;
    if (vfx && sm) CurveData::from(shader_param_resource(sm, "erosion_curve"), erosion);
    std::vector<Vec2> v, uv;
    std::vector<Color> col;
    std::vector<int> idx;
    for (size_t i = 0; i < particles_.size(); ++i) {
        const Particle& p = particles_[i];
        if (!p.active) continue;
        Xform2D px = view * p.xform;
        Vec2 h = Vec2(ts.x / (anim ? hf : 1), ts.y / (anim ? vf : 1)) * 0.5f;
        float fu0 = 0, fv0 = 0, fw = 1, fh = 1;
        if (anim) {
            float total = (float)(hf * vf);
            float frame = floorf(p.custom[2] * total);
            frame = loop ? fmodf(frame, total) : clampf(frame, 0, total - 1);
            if (frame < 0) frame += total;
            fw = 1.0f / hf;
            fh = 1.0f / vf;
            fu0 = fmodf(frame, (float)hf) / hf;
            fv0 = floorf((frame + 0.5f) / hf) / vf;
        }
        if (fb) {
            float life = p.custom[3] > 0 ? p.custom[1] / p.custom[3] : 0.0f;
            float prog = fb_curve.valid() ? fb_curve.sample(life) : 0.0f; /* hint_default_black */
            float frame;
            if (fb == 3) {
                frame = floorf(fb_frames * p.custom[2] * 0.99f);
            } else if (fb == 1) {
                frame = floorf((fb_frames * p.custom[2] + fb_frames * prog) * 0.99f);
            } else {
                float fx = floorf(p.custom[2] * fb_size.y) / fb_size.y;
                float fy = floorf(prog * fb_size.x) / fb_size.x / fb_size.y;
                frame = floorf((fx + fy) * fb_frames);
            }
            if (frame < 0) frame = 0;
            fw = 1.0f / fb_size.x;
            fh = 1.0f / fb_size.y;
            fu0 = fmodf(frame, fb_size.x) / fb_size.x;
            fv0 = floorf(frame / fb_size.x) / fb_size.y;
        }
        Vec2 po(pivot.x * ts.x, pivot.y * ts.y);
        Vec2 hg = h * grow;
        Vec2 corners[4] = { Vec2(-hg.x, -hg.y) + po, Vec2(hg.x, -hg.y) + po, Vec2(hg.x, hg.y) + po, Vec2(-hg.x, hg.y) + po };
        if (ray) /* vfx_ray_shader: VERTEX.x *= CUSTOM.z */
            for (int k = 0; k < 4; ++k) corners[k].x *= p.custom[2];
        float cu[4] = { 0, 1, 1, 0 }, cv[4] = { 0, 0, 1, 1 };
        int base = (int)v.size();
        if (trail_enabled) {
            /* GPUParticles2D's trail mesh: rows of two vertices at
             * (+-width / 2, 0), each row skinned between two bones (the
             * particle's transform i / sections of the trail ago), v down
             * the trail */
            int sections = trail_sections, sub = trail_subdivisions, segments = sections * sub;
            std::vector<Xform2D> bones((size_t)sections + 1);
            for (int b = 0; b <= sections; ++b) bones[(size_t)b] = view * trail_at(p, trail_lifetime * b / sections);
            float half = ts.x * 0.5f;
            for (int j = 0; j <= segments; ++j) {
                int b = j / sub;
                float blend = 1.0f - (float)(j % sub) / (float)sub;
                const Xform2D& b0 = bones[(size_t)b];
                const Xform2D& b1 = bones[(size_t)(b + 1 < sections ? b + 1 : sections)];
                float vv = (float)j / (float)segments;
                for (int side = 0; side < 2; ++side) {
                    Vec2 at(side ? half : -half, 0);
                    v.push_back(b0.xform(at) * blend + b1.xform(at) * (1.0f - blend));
                    float u = (float)side, w = vv;
                    uv.push_back(img ? Vec2((src.pos.x + u * src.size.x) / is.x, (src.pos.y + w * src.size.y) / is.y)
                                     : Vec2(u, w));
                    col.push_back(p.color);
                }
                if (j > 0) {
                    int r = base + j * 2 - 2;
                    int q[6] = { r, r + 1, r + 2, r + 1, r + 3, r + 2 };
                    for (int k = 0; k < 6; ++k) idx.push_back(q[k]);
                }
            }
        } else {
            for (int k = 0; k < 4; ++k) {
                v.push_back(px.xform(corners[k]));
                float u = fu0 + cu[k] * fw, w = fv0 + cv[k] * fh;
                uv.push_back(img ? Vec2((src.pos.x + u * src.size.x) / is.x, (src.pos.y + w * src.size.y) / is.y)
                                 : Vec2(u, w));
                col.push_back(p.color);
            }
            int q[6] = { 0, 1, 2, 2, 3, 0 };
            for (int k = 0; k < 6; ++k) idx.push_back(base + q[k]);
        }
        if (per_screen && img) {
            float life = p.custom[3] > 0 ? p.custom[1] / p.custom[3] : 0.0f;
            shader_screen_particle(life, &screen);
            c.renderer()->set_screen(&screen);
            c.draw_triangles(img, v, uv, col, idx);
            v.clear();
            uv.clear();
            col.clear();
            idx.clear();
        } else if (per_effect && img) {
            float life = p.custom[3] > 0 ? p.custom[1] / p.custom[3] : 0.0f;
            shader_effect_particle(life, p.custom[2], &effect, p.custom[0]);
            c.renderer()->set_effect(&effect);
            c.draw_triangles(img, v, uv, col, idx);
            v.clear();
            uv.clear();
            col.clear();
            idx.clear();
        } else if (vfx && img) {
            VfxParams one = *vfx;
            float life = p.custom[3] > 0 ? p.custom[1] / p.custom[3] : 0.0f;
            if (erosion.valid()) one.erosion[0] = erosion.sample(life);
            c.renderer()->set_vfx(&one);
            c.draw_triangles(img, v, uv, col, idx);
            v.clear();
            uv.clear();
            col.clear();
            idx.clear();
        }
    }
    if (vfx && img) {
        c.renderer()->set_vfx(vfx);
        return;
    }
    if (per_effect && img) {
        shader_effect_particle(0, 0, &effect);
        c.renderer()->set_effect(&effect);
        return;
    }
    if (img) c.draw_triangles(img, v, uv, col, idx);
    else c.draw_colored_triangles(v, idx, col);
}

/* ---- LightOccluder2D ---- */

static std::vector<LightOccluder2D*>& occluders() {
    static std::vector<LightOccluder2D*> all;
    return all;
}

LightOccluder2D::LightOccluder2D() : sdf_collision(true) { occluders().push_back(this); }

LightOccluder2D::~LightOccluder2D() {
    std::vector<LightOccluder2D*>& all = occluders();
    for (size_t i = 0; i < all.size(); ++i)
        if (all[i] == this) {
            all.erase(all.begin() + (long)i);
            break;
        }
}

bool LightOccluder2D::set(const std::string& key, const Json& v, const PropCtx& ctx) {
    if (key == "occluder") {
        polygon.clear();
        Resource* r = ctx.resource(v);
        if (r && r->props) {
            const Json& a = (*r->props)["polygon"]["v"];
            for (size_t i = 0; i + 1 < a.size(); i += 2) polygon.push_back(Vec2(jv_float(a[i]), jv_float(a[i + 1])));
        }
    } else if (key == "sdf_collision") {
        sdf_collision = v.b(true);
    } else {
        return Node2D::set(key, v, ctx);
    }
    return true;
}

bool LightOccluder2D::sdf(const Node* root, const Vec2& p, float* dist, Vec2* normal) {
    std::vector<LightOccluder2D*>& all = occluders();
    bool found = false;
    float best = 0;
    Vec2 best_n;
    for (size_t o = 0; o < all.size(); ++o) {
        LightOccluder2D* oc = all[o];
        size_t n = oc->polygon.size();
        if (!oc->sdf_collision || n < 3 || !oc->is_visible_in_tree()) continue;
        const Node* r = oc;
        while (r->parent) r = r->parent;
        if (r != root) continue;
        /* the polygon filled (crossings), the distance to its nearest edge */
        Xform2D g = oc->global_transform();
        bool inside = false;
        float near2 = -1;
        Vec2 near;
        for (size_t i = 0, j = n - 1; i < n; j = i++) {
            Vec2 a = g.xform(oc->polygon[j]), b = g.xform(oc->polygon[i]);
            if ((b.y > p.y) != (a.y > p.y) && p.x < (a.x - b.x) * (p.y - b.y) / (a.y - b.y) + b.x) inside = !inside;
            Vec2 ab = b - a, ap = p - a;
            float l2 = ab.x * ab.x + ab.y * ab.y;
            float t = l2 > 0 ? clampf((ap.x * ab.x + ap.y * ab.y) / l2, 0.0f, 1.0f) : 0.0f;
            Vec2 q = a + ab * t;
            float d2 = (p.x - q.x) * (p.x - q.x) + (p.y - q.y) * (p.y - q.y);
            if (near2 < 0 || d2 < near2) {
                near2 = d2;
                near = q;
            }
        }
        float d = sqrtf(near2);
        Vec2 dn = d > 0 ? (p - near) / d : Vec2(0, -1);
        if (inside) {
            d = -d;
            dn = Vec2(-dn.x, -dn.y);
        }
        if (!found || d < best) {
            found = true;
            best = d;
            best_n = dn;
        }
    }
    if (found) {
        *dist = best;
        *normal = best_n;
    }
    return found;
}

} /* namespace spire */

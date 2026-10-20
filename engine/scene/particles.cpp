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
static float deg2rad(float d) { return d * 0.0174532925f; }
static float lerpf(float a, float b, float t) { return a + (b - a) * t; }

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

Particles2D::Particles2D(bool is_gpu)
    : gpu(is_gpu), emitting(true), one_shot(false), local_coords(false), fractional_delta(true), align_y(false),
      split_scale(false), amount(8), fixed_fps(is_gpu ? 30 : 0), draw_order(0), emission_shape(0), lifetime(1),
      preprocess(0), speed_scale(1), explosiveness(0), randomness(0), lifetime_randomness(0), seed(0), texture(NULL),
      direction(1, 0), gravity(is_gpu ? Vec2(0, -9.8f) : Vec2(0, 980)), emission_rect_extents(1, 1), spread(45),
      emission_sphere_radius(1), time_(0), frame_remainder_(0), cycle_(0), started_(false) {
    for (int i = 0; i < P_MAX; ++i) pmin[i] = pmax[i] = 0;
    pmin[P_SCALE] = pmax[P_SCALE] = 1;
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
    else if (key == "draw_order") draw_order = v.i();
    else if (key == "seed") seed = (u32)v.num();
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

/* convert_from_particles, reading the ParticleProcessMaterial's stored props. */
void Particles2D::apply_process_material(Resource* m) {
    if (!m || !m->props || m->type != "ParticleProcessMaterial") return;
    const Json& p = *m->props;
    Vec2 d;
    if (p.has("direction") && p["direction"]["v"].size() >= 2)
        direction = Vec2(jv_float(p["direction"]["v"][(size_t)0]), jv_float(p["direction"]["v"][1]));
    if (p.has("spread")) spread = jv_float(p["spread"], 45);
    if (p.has("color")) jv_color(p["color"], color);
    GradientData::from(sub_of(m, "color_ramp"), color_ramp);
    GradientData::from(sub_of(m, "color_initial_ramp"), color_initial_ramp);
    CurveData::from(sub_of(m, "alpha_curve"), alpha_curve);
    align_y = p["particle_flag_align_y"].b();
    emission_shape = p["emission_shape"].i();
    if (emission_shape == 6) emission_shape = 1; /* ring: as a sphere (2D) */
    if (p.has("emission_sphere_radius")) emission_sphere_radius = jv_float(p["emission_sphere_radius"], 1);
    if (p.has("emission_box_extents") && p["emission_box_extents"]["v"].size() >= 2)
        emission_rect_extents = Vec2(jv_float(p["emission_box_extents"]["v"][(size_t)0]),
                                     jv_float(p["emission_box_extents"]["v"][1]));
    if (p.has("gravity") && p["gravity"]["v"].size() >= 2)
        gravity = Vec2(jv_float(p["gravity"]["v"][(size_t)0]), jv_float(p["gravity"]["v"][1]));
    lifetime_randomness = jv_float(p["lifetime_randomness"]);
    for (int i = 0; i < P_MAX; ++i) {
        std::string n = PARAM_NAMES_GPU[i];
        if (p.has((n + "_min").c_str())) pmin[i] = jv_float(p[(n + "_min").c_str()]);
        if (p.has((n + "_max").c_str())) pmax[i] = jv_float(p[(n + "_max").c_str()]);
        Resource* c = sub_of(m, CURVE_NAMES_GPU[i]);
        if (i == P_SCALE && c && c->type == "CurveXYZTexture") {
            split_scale = true;
            CurveData::from(sub_of(c, "curve_x"), scale_x);
            CurveData::from(sub_of(c, "curve_y"), scale_y);
        } else {
            CurveData::from(c, curves[i]);
        }
    }
}

/* ---- simulation (_update_internal / _particles_process) ---- */

void Particles2D::restart() {
    time_ = frame_remainder_ = 0;
    cycle_ = 0;
    started_ = false;
    emitting = true;
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
    /* Stopping emission does not stop the particles already alive. In a
     * one-shot system those particles must keep aging until they finish. */
    if (particles_.empty() || (!emitting && active_count() == 0)) return;
    double frame_time = fixed_fps > 0 ? 1.0 / fixed_fps : 1.0 / 30.0;
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
            Vec2 diff = pos; /* emission origin is the node (local coordinates) */
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

/* ---- draw: a texture-sized quad per particle, CanvasItemMaterial
 * particle animation frames ---- */

void Particles2D::draw(Canvas& c) {
    if (particles_.empty()) return;
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
    CurveData erosion;
    if (vfx && sm) CurveData::from(shader_param_resource(sm, "erosion_curve"), erosion);
    std::vector<Vec2> v, uv;
    std::vector<Color> col;
    std::vector<int> idx;
    for (size_t i = 0; i < particles_.size(); ++i) {
        const Particle& p = particles_[i];
        if (!p.active) continue;
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
        Vec2 corners[4] = { Vec2(-h.x, -h.y) + po, Vec2(h.x, -h.y) + po, Vec2(h.x, h.y) + po, Vec2(-h.x, h.y) + po };
        if (ray) /* vfx_ray_shader: VERTEX.x *= CUSTOM.z */
            for (int k = 0; k < 4; ++k) corners[k].x *= p.custom[2];
        float cu[4] = { 0, 1, 1, 0 }, cv[4] = { 0, 0, 1, 1 };
        int base = (int)v.size();
        for (int k = 0; k < 4; ++k) {
            v.push_back(p.xform.xform(corners[k]));
            float u = fu0 + cu[k] * fw, w = fv0 + cv[k] * fh;
            uv.push_back(img ? Vec2((src.pos.x + u * src.size.x) / is.x, (src.pos.y + w * src.size.y) / is.y)
                             : Vec2(u, w));
            col.push_back(p.color);
        }
        int q[6] = { 0, 1, 2, 2, 3, 0 };
        for (int k = 0; k < 6; ++k) idx.push_back(base + q[k]);
        if (per_effect && img) {
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

} /* namespace spire */

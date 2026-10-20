/* Spine animation: timeline evaluation and the animation state. */
#include "spine.h"

#include <math.h>

namespace spire {

/* ---- curves ---- */

static float polyline(const float* p, float t0, float v0, float t1, float v1, float time) {
    if (p[0] > time) return p[0] > t0 ? v0 + (time - t0) / (p[0] - t0) * (p[1] - v0) : v0;
    for (int i = 2; i < 18; i += 2) {
        if (p[i] >= time) {
            float x0 = p[i - 2], y0 = p[i - 1];
            return p[i] > x0 ? y0 + (time - x0) / (p[i] - x0) * (p[i + 1] - y0) : y0;
        }
    }
    return t1 > p[16] ? p[17] + (time - p[16]) / (t1 - p[16]) * (v1 - p[17]) : p[17];
}

/* Value i of key k at time (key k <= time < key k + 1). */
static float curve_value(const SpTimeline& tl, int k, int i, float time) {
    int stride = tl.values + 1;
    const float* key = &tl.keys[k * stride];
    float v0 = key[1 + i];
    int kind = k < (int)tl.curves.kind.size() ? tl.curves.kind[k] : SpCurves::CURVE_STEPPED;
    if (kind == SpCurves::CURVE_STEPPED || (k + 1) * stride >= (int)tl.keys.size()) return v0;
    float t0 = key[0], t1 = key[stride], v1 = key[stride + 1 + i];
    if (kind == SpCurves::CURVE_LINEAR) return t1 > t0 ? v0 + (time - t0) / (t1 - t0) * (v1 - v0) : v0;
    if (kind + i * 18 + 18 > (int)tl.curves.points.size()) return v0;
    return polyline(&tl.curves.points[kind + i * 18], t0, v0, t1, v1, time);
}

static float curve_percent(const SpTimeline& tl, int k, float time) {
    int kind = k < (int)tl.curves.kind.size() ? tl.curves.kind[k] : SpCurves::CURVE_STEPPED;
    if (kind == SpCurves::CURVE_STEPPED || k + 1 >= tl.key_count()) return 0;
    float t0 = tl.time(k), t1 = tl.time(k + 1);
    if (kind == SpCurves::CURVE_LINEAR) return t1 > t0 ? (time - t0) / (t1 - t0) : 0;
    if (kind + 18 > (int)tl.curves.points.size()) return 0;
    return polyline(&tl.curves.points[kind], t0, 0, t1, 1, time);
}

/* Index of the last key at or before time; -1 before the first. */
static int key_index(const std::vector<float>& keys, int stride, float time) {
    int n = stride ? (int)keys.size() / stride : 0;
    if (n == 0 || time < keys[0]) return -1;
    int lo = 0, hi = n - 1;
    while (lo < hi) {
        int mid = (lo + hi + 1) / 2;
        if (keys[mid * stride] <= time) lo = mid;
        else hi = mid - 1;
    }
    return lo;
}

static int key_index(const SpTimeline& tl, float time) {
    return key_index(tl.keys, tl.values ? tl.values + 1 : 1, time);
}

static float mixf(float from, float to, float alpha) { return alpha >= 1.0f ? to : from + (to - from) * alpha; }

/* Rotations mix the short way round. */
static float mix_angle(float from, float to, float alpha) {
    if (alpha >= 1.0f) return to;
    float r = to - from;
    r -= floorf(r / 360.0f + 0.5f) * 360.0f;
    return from + r * alpha;
}

/* ---- timelines ---- */

static void set_attachment(SpSkeleton& skel, int slot, const std::string& key) {
    SpSlot& s = skel.slots[slot];
    SpAttachment* a = key.empty() ? NULL : skel.attachment(slot, key);
    if (a == s.attachment) return;
    if (!a || !s.attachment || a->timeline_key != s.attachment->timeline_key) s.deform.clear();
    s.attachment = a;
    s.sequence_index = -1;
}

static void apply_bone(const SpTimeline& tl, SpSkeleton& skel, float time, float alpha) {
    if (tl.target < 0 || tl.target >= (int)skel.bones.size()) return;
    SpBone& b = skel.bones[tl.target];
    if (!b.active) return;
    const SpBoneData& d = *b.data;
    int k = key_index(tl, time);
    if (tl.type == SPT_INHERIT) {
        b.inherit = k < 0 || k >= (int)tl.ints.size() ? d.inherit : tl.ints[k];
        return;
    }
    float v0 = k < 0 ? 0 : curve_value(tl, k, 0, time);
    float v1 = k < 0 || tl.values < 2 ? 0 : curve_value(tl, k, 1, time);
    switch (tl.type) {
    case SPT_ROTATE: b.rotation = mix_angle(b.rotation, d.rotation + v0, alpha); break;
    case SPT_TRANSLATE:
        b.x = mixf(b.x, d.x + v0, alpha);
        b.y = mixf(b.y, d.y + v1, alpha);
        break;
    case SPT_TRANSLATE_X: b.x = mixf(b.x, d.x + v0, alpha); break;
    case SPT_TRANSLATE_Y: b.y = mixf(b.y, d.y + v0, alpha); break;
    case SPT_SCALE:
        b.scale_x = mixf(b.scale_x, k < 0 ? d.scale_x : d.scale_x * v0, alpha);
        b.scale_y = mixf(b.scale_y, k < 0 ? d.scale_y : d.scale_y * v1, alpha);
        break;
    case SPT_SCALE_X: b.scale_x = mixf(b.scale_x, k < 0 ? d.scale_x : d.scale_x * v0, alpha); break;
    case SPT_SCALE_Y: b.scale_y = mixf(b.scale_y, k < 0 ? d.scale_y : d.scale_y * v0, alpha); break;
    case SPT_SHEAR:
        b.shear_x = mixf(b.shear_x, d.shear_x + v0, alpha);
        b.shear_y = mixf(b.shear_y, d.shear_y + v1, alpha);
        break;
    case SPT_SHEAR_X: b.shear_x = mixf(b.shear_x, d.shear_x + v0, alpha); break;
    case SPT_SHEAR_Y: b.shear_y = mixf(b.shear_y, d.shear_y + v0, alpha); break;
    default: break;
    }
}

/* Sequence modes: hold, once, loop, pingpong, onceReverse, loopReverse,
 * pingpongReverse. */
static int sequence_index(int mode, int index, float elapsed, float delay, int count) {
    if (mode == 0 || count <= 0) return index;
    if (delay > 0) index += (int)(elapsed / delay + 0.0001f);
    int n = (count << 1) - 2;
    switch (mode) {
    case 1: return index < count - 1 ? index : count - 1;
    case 2: return index % count;
    case 3:
        index = n == 0 ? 0 : index % n;
        return index >= count ? n - index : index;
    case 4: return count - 1 - index > 0 ? count - 1 - index : 0;
    case 5: return count - 1 - (index % count);
    case 6:
        index = n == 0 ? 0 : (index + count - 1) % n;
        return index >= count ? n - index : index;
    default: return index;
    }
}

static void apply_slot(const SpTimeline& tl, SpSkeleton& skel, float time, float alpha) {
    if (tl.target < 0 || tl.target >= (int)skel.slots.size()) return;
    SpSlot& s = skel.slots[tl.target];
    if (!skel.bones[s.data->bone].active) return;
    int k = key_index(tl, time);
    switch (tl.type) {
    case SPT_ATTACHMENT:
        if (alpha < 0.5f) return; /* attachments switch over at the middle of a crossfade */
        set_attachment(skel, tl.target, k < 0 || k >= (int)tl.names.size() ? s.data->attachment : tl.names[k]);
        return;
    case SPT_RGBA:
    case SPT_RGBA2: {
        const Color& c0 = s.data->color;
        Color c = k < 0 ? c0 : Color(curve_value(tl, k, 0, time), curve_value(tl, k, 1, time),
                                     curve_value(tl, k, 2, time), curve_value(tl, k, 3, time));
        s.color = Color(mixf(s.color.r, c.r, alpha), mixf(s.color.g, c.g, alpha), mixf(s.color.b, c.b, alpha),
                        mixf(s.color.a, c.a, alpha));
        return;
    }
    case SPT_RGB:
    case SPT_RGB2: {
        const Color& c0 = s.data->color;
        float r = k < 0 ? c0.r : curve_value(tl, k, 0, time), g = k < 0 ? c0.g : curve_value(tl, k, 1, time);
        float bl = k < 0 ? c0.b : curve_value(tl, k, 2, time);
        s.color.r = mixf(s.color.r, r, alpha);
        s.color.g = mixf(s.color.g, g, alpha);
        s.color.b = mixf(s.color.b, bl, alpha);
        return;
    }
    case SPT_ALPHA:
        s.color.a = mixf(s.color.a, k < 0 ? s.data->color.a : curve_value(tl, k, 0, time), alpha);
        return;
    case SPT_SEQUENCE: {
        SpAttachment* a = s.attachment;
        if (!a || a->timeline_key != tl.attachment || a->seq_count <= 0) return;
        int k4 = key_index(tl.keys, 4, time);
        if (k4 < 0) {
            if (alpha >= 1.0f) s.sequence_index = -1;
            return;
        }
        const float* f = &tl.keys[k4 * 4];
        int mode = (int)f[1], index = (int)f[2];
        s.sequence_index = sequence_index(mode, index, time - f[0], f[3], a->seq_count);
        return;
    }
    case SPT_DEFORM: {
        SpAttachment* a = s.attachment;
        if (!a || a->timeline_key != tl.attachment || (a->type != SP_ATT_MESH && a->type != SP_ATT_BOUNDING_BOX &&
                                                       a->type != SP_ATT_PATH && a->type != SP_ATT_CLIPPING))
            return;
        size_t n = (size_t)a->deform_length();
        if (k < 0) {
            if (alpha >= 1.0f) s.deform.clear();
            return;
        }
        const std::vector<float>& f0 = tl.deforms[k];
        if (f0.size() != n) return;
        if (s.deform.size() != n) {
            if (a->weighted()) s.deform.assign(n, 0.0f);
            else s.deform = a->vertices;
        }
        if (k + 1 >= (int)tl.deforms.size()) {
            for (size_t i = 0; i < n; ++i) s.deform[i] = mixf(s.deform[i], f0[i], alpha);
            return;
        }
        const std::vector<float>& f1 = tl.deforms[k + 1];
        float pct = curve_percent(tl, k, time);
        for (size_t i = 0; i < n; ++i) s.deform[i] = mixf(s.deform[i], f0[i] + (f1[i] - f0[i]) * pct, alpha);
        return;
    }
    default: return;
    }
}

static void apply_constraint(const SpTimeline& tl, SpSkeleton& skel, float last_time, float time, float alpha) {
    int k = key_index(tl, time);
    switch (tl.type) {
    case SPT_IK: {
        if (tl.target < 0 || tl.target >= (int)skel.ik.size()) return;
        SpIk& c = skel.ik[tl.target];
        if (!c.active) return;
        const SpIkData& d = *c.data;
        if (k < 0) {
            c.mix = mixf(c.mix, d.mix, alpha);
            c.softness = mixf(c.softness, d.softness, alpha);
            if (alpha >= 1.0f) {
                c.bend = d.bend;
                c.compress = d.compress;
                c.stretch = d.stretch;
            }
            return;
        }
        c.mix = mixf(c.mix, curve_value(tl, k, 0, time), alpha);
        c.softness = mixf(c.softness, curve_value(tl, k, 1, time), alpha);
        if (alpha >= 0.5f && k < (int)tl.ints.size()) {
            c.bend = tl.ints[k];
            c.compress = (tl.flags[k] & 1) != 0;
            c.stretch = (tl.flags[k] & 2) != 0;
        }
        return;
    }
    case SPT_TRANSFORM: {
        if (tl.target < 0 || tl.target >= (int)skel.transform.size()) return;
        SpTransform& c = skel.transform[tl.target];
        if (!c.active) return;
        const SpTransformData& d = *c.data;
        float v[6];
        if (k < 0) {
            v[0] = d.mix_rotate;
            v[1] = d.mix_x;
            v[2] = d.mix_y;
            v[3] = d.mix_scale_x;
            v[4] = d.mix_scale_y;
            v[5] = d.mix_shear_y;
        } else {
            for (int i = 0; i < 6; ++i) v[i] = curve_value(tl, k, i, time);
        }
        c.mix_rotate = mixf(c.mix_rotate, v[0], alpha);
        c.mix_x = mixf(c.mix_x, v[1], alpha);
        c.mix_y = mixf(c.mix_y, v[2], alpha);
        c.mix_scale_x = mixf(c.mix_scale_x, v[3], alpha);
        c.mix_scale_y = mixf(c.mix_scale_y, v[4], alpha);
        c.mix_shear_y = mixf(c.mix_shear_y, v[5], alpha);
        return;
    }
    case SPT_PATH_POSITION:
    case SPT_PATH_SPACING:
    case SPT_PATH_MIX: {
        if (tl.target < 0 || tl.target >= (int)skel.path.size()) return;
        SpPath& c = skel.path[tl.target];
        if (!c.active) return;
        const SpPathData& d = *c.data;
        if (tl.type == SPT_PATH_POSITION) {
            c.position = mixf(c.position, k < 0 ? d.position : curve_value(tl, k, 0, time), alpha);
        } else if (tl.type == SPT_PATH_SPACING) {
            c.spacing = mixf(c.spacing, k < 0 ? d.spacing : curve_value(tl, k, 0, time), alpha);
        } else {
            c.mix_rotate = mixf(c.mix_rotate, k < 0 ? d.mix_rotate : curve_value(tl, k, 0, time), alpha);
            c.mix_x = mixf(c.mix_x, k < 0 ? d.mix_x : curve_value(tl, k, 1, time), alpha);
            c.mix_y = mixf(c.mix_y, k < 0 ? d.mix_y : curve_value(tl, k, 2, time), alpha);
        }
        return;
    }
    case SPT_PHYSICS_RESET: {
        /* Keys crossed since the last apply reset the constraint(s). */
        bool fire = false;
        for (size_t i = 0; i < tl.keys.size(); ++i) {
            float t = tl.keys[i];
            if ((last_time < 0 && t <= time && t == 0) || (last_time >= 0 && last_time < time && t > last_time &&
                                                           t <= time))
                fire = true;
        }
        if (fire) skel.reset_physics(tl.target);
        return;
    }
    default: break;
    }
    if (tl.type < SPT_PHYSICS_INERTIA || tl.type > SPT_PHYSICS_MIX) return;
    /* Physics properties: one constraint, or (-1) every constraint whose
     * data marks the property global. */
    static const int GLOBAL_FLAG[7] = { SP_PHYS_GLOBAL_INERTIA, SP_PHYS_GLOBAL_STRENGTH, SP_PHYS_GLOBAL_DAMPING,
                                        SP_PHYS_GLOBAL_MASS, SP_PHYS_GLOBAL_WIND, SP_PHYS_GLOBAL_GRAVITY,
                                        SP_PHYS_GLOBAL_MIX };
    int prop = tl.type - SPT_PHYSICS_INERTIA;
    for (size_t i = 0; i < skel.physics.size(); ++i) {
        if (tl.target >= 0 && (int)i != tl.target) continue;
        SpPhysicsC& c = skel.physics[i];
        if (!c.active) continue;
        const SpPhysicsData& d = *c.data;
        if (tl.target < 0 && !(d.global & GLOBAL_FLAG[prop])) continue;
        float* field = NULL;
        float setup = 0;
        switch (tl.type) {
        case SPT_PHYSICS_INERTIA: field = &c.inertia; setup = d.inertia; break;
        case SPT_PHYSICS_STRENGTH: field = &c.strength; setup = d.strength; break;
        case SPT_PHYSICS_DAMPING: field = &c.damping; setup = d.damping; break;
        case SPT_PHYSICS_WIND: field = &c.wind; setup = d.wind; break;
        case SPT_PHYSICS_GRAVITY: field = &c.gravity; setup = d.gravity; break;
        case SPT_PHYSICS_MIX: field = &c.mix; setup = d.mix; break;
        case SPT_PHYSICS_MASS: {
            /* Keys hold the mass; the constraint keeps its inverse. */
            float mass = c.mass_inverse != 0 ? 1.0f / c.mass_inverse : 0;
            float target = k < 0 ? (d.mass_inverse != 0 ? 1.0f / d.mass_inverse : 0) : curve_value(tl, k, 0, time);
            mass = mixf(mass, target, alpha);
            c.mass_inverse = mass != 0 ? 1.0f / mass : 0;
            continue;
        }
        default: break;
        }
        if (field) *field = mixf(*field, k < 0 ? setup : curve_value(tl, k, 0, time), alpha);
    }
}

static void apply_timeline(const SpTimeline& tl, SpSkeleton& skel, float last_time, float time, float alpha) {
    if (tl.type <= SPT_INHERIT) {
        apply_bone(tl, skel, time, alpha);
        return;
    }
    if (tl.type == SPT_DRAW_ORDER) {
        if (alpha < 0.5f) return;
        int k = key_index(tl, time);
        if (k < 0 || tl.orders[k].size() != skel.draw_order.size()) {
            for (size_t i = 0; i < skel.draw_order.size(); ++i) skel.draw_order[i] = (int)i;
        } else {
            skel.draw_order = tl.orders[k];
        }
        return;
    }
    if (tl.type >= SPT_IK) {
        apply_constraint(tl, skel, last_time, time, alpha);
        return;
    }
    apply_slot(tl, skel, time, alpha);
}

void sp_apply_animation(const SpAnimation& anim, SpSkeleton& skel, float last_time, float time, bool loop,
                        float alpha) {
    if (loop && anim.duration > 0) {
        time = fmodf(time, anim.duration);
        if (time < 0) time += anim.duration;
        if (last_time >= 0) {
            last_time = fmodf(last_time, anim.duration);
            if (last_time > time) last_time = -1; /* wrapped: keys from the start fire again */
        }
    } else if (time > anim.duration) {
        time = anim.duration;
    }
    for (size_t i = 0; i < anim.timelines.size(); ++i) apply_timeline(anim.timelines[i], skel, last_time, time, alpha);
}

/* ---- animation state ---- */

SpAnimationState::SpAnimationState() : default_mix(0), time_scale(1) {}

SpAnimationState::Track& SpAnimationState::track(int i) {
    if (i < 0) i = 0;
    if ((size_t)i >= tracks_.size()) tracks_.resize(i + 1);
    return tracks_[i];
}

/* The event keys of anim in (from, to], seconds into it; from < 0: from the
 * start, keys at 0 included. Looping: across the wraps. */
static void fire_events(std::vector<SpFired>& out, int track, const SpAnimation* anim, float from, float to, bool loop) {
    if (!anim || to <= from) return;
    float d = anim->duration;
    for (size_t i = 0; i < anim->timelines.size(); ++i) {
        const SpTimeline& tl = anim->timelines[i];
        if (tl.type != SPT_EVENT) continue;
        for (int k = 0; k < tl.key_count(); ++k) {
            float kt = tl.time(k);
            bool hit = false;
            if (!loop || d <= 0) {
                hit = kt > from && kt <= to && kt <= d;
                if (from < 0 && kt == 0) hit = true;
            } else {
                /* every pass of the loop between from and to */
                float base = from < 0 ? 0 : floorf(from / d) * d;
                for (float c = base; c <= to && !hit; c += d) {
                    float at = c + kt;
                    if ((at > from || (from < 0 && at == 0)) && at <= to) hit = true;
                }
            }
            if (!hit) continue;
            SpFired f;
            f.start = false;
            f.track = track;
            f.anim = anim;
            f.name = k < (int)tl.names.size() ? tl.names[(size_t)k] : std::string();
            f.ival = k < (int)tl.ints.size() ? tl.ints[(size_t)k] : 0;
            f.fval = k < (int)tl.fvals.size() ? tl.fvals[(size_t)k] : 0;
            f.sval = k < (int)tl.strs.size() ? tl.strs[(size_t)k] : std::string();
            out.push_back(f);
        }
    }
}

void SpAnimationState::start(Track& t, const Entry& e) {
    if (t.has_cur && t.cur.anim && default_mix > 0) {
        t.from = t.cur;
        t.has_from = true;
    } else {
        t.has_from = false;
    }
    t.cur = e;
    if (e.anim) {
        SpFired f;
        f.start = true;
        f.track = (int)(&t - &tracks_[0]);
        f.anim = e.anim;
        f.ival = 0;
        f.fval = 0;
        fired.push_back(f);
    }
    t.cur.mix_time = 0;
    t.cur.mix_duration = t.has_from ? default_mix : 0;
    t.has_cur = true;
}

void SpAnimationState::set_animation(int ti, const SpAnimation* anim, bool loop) {
    Track& t = track(ti);
    t.queue.clear();
    Entry e;
    e.anim = anim;
    e.loop = loop;
    start(t, e);
}

void SpAnimationState::set_empty_animation(int ti) {
    Track& t = track(ti);
    t.queue.clear();
    t.has_cur = false;
    t.has_from = false;
}

void SpAnimationState::add_animation(int ti, const SpAnimation* anim, bool loop, float delay) {
    Track& t = track(ti);
    Entry e;
    e.anim = anim;
    e.loop = loop;
    if (!t.has_cur) {
        e.delay = delay;
        start(t, e);
        return;
    }
    /* Delay <= 0: from the end of the entry before it (less the crossfade). */
    const Entry& prev = t.queue.empty() ? t.cur : t.queue.back();
    if (delay <= 0) {
        float d = prev.anim ? prev.anim->duration : 0;
        delay = maxf(0.0f, d - default_mix + delay);
    }
    e.delay = delay;
    t.queue.push_back(e);
}

void SpAnimationState::add_empty_animation(int ti) {
    Track& t = track(ti);
    if (!t.has_cur) return;
    const Entry& prev = t.queue.empty() ? t.cur : t.queue.back();
    Entry e; /* no animation */
    e.delay = maxf(0.0f, (prev.anim ? prev.anim->duration : 0) - default_mix);
    t.queue.push_back(e);
}

void SpAnimationState::update(float dt) {
    dt *= time_scale;
    for (size_t i = 0; i < tracks_.size(); ++i) {
        Track& t = tracks_[i];
        if (!t.has_cur) continue;
        float d = dt;
        if (t.cur.delay > 0) {
            t.cur.delay -= d;
            if (t.cur.delay > 0) continue;
            d = -t.cur.delay;
            t.cur.delay = 0;
        }
        float before = t.cur.events_from_start ? -1.0f : t.cur.time;
        t.cur.events_from_start = false;
        t.cur.time += d * t.cur.time_scale;
        fire_events(fired, (int)i, t.cur.anim, before, t.cur.time, t.cur.loop);
        if (t.has_from) {
            t.from.time += d * t.from.time_scale;
            t.cur.mix_time += d;
            if (t.cur.mix_time >= t.cur.mix_duration) t.has_from = false;
        }
        if (!t.queue.empty() && t.cur.time >= t.queue[0].delay) {
            Entry next = t.queue[0];
            t.queue.erase(t.queue.begin());
            next.time = t.cur.time - next.delay;
            next.delay = 0;
            if (!next.anim) {
                /* the empty animation: the track lets go */
                t.has_cur = false;
                t.has_from = false;
                continue;
            }
            start(t, next);
            /* its keys from the start to where it begins */
            fire_events(fired, (int)i, t.cur.anim, -1.0f, t.cur.time, t.cur.loop);
            t.cur.events_from_start = false;
        }
    }
}

void SpAnimationState::apply(SpSkeleton& skel) {
    for (size_t i = 0; i < tracks_.size(); ++i) {
        Track& t = tracks_[i];
        if (!t.has_cur || t.cur.delay > 0 || !t.cur.anim) continue;
        if (t.has_from && t.from.anim && t.cur.mix_duration > 0) {
            sp_apply_animation(*t.from.anim, skel, t.from.last_time, t.from.time, t.from.loop, 1.0f);
            sp_apply_animation(*t.cur.anim, skel, t.cur.last_time, t.cur.time, t.cur.loop,
                               t.cur.mix_time / t.cur.mix_duration);
            t.from.last_time = t.from.time;
        } else {
            sp_apply_animation(*t.cur.anim, skel, t.cur.last_time, t.cur.time, t.cur.loop, 1.0f);
        }
        t.cur.last_time = t.cur.time;
    }
}

const SpAnimation* SpAnimationState::current(int ti) const {
    if (ti < 0 || (size_t)ti >= tracks_.size() || !tracks_[ti].has_cur) return NULL;
    return tracks_[ti].cur.anim;
}

float SpAnimationState::track_time(int ti) const {
    if (ti < 0 || (size_t)ti >= tracks_.size() || !tracks_[ti].has_cur) return -1;
    return tracks_[ti].cur.time;
}

void SpAnimationState::set_track_time(int ti, float t) {
    if (ti < 0 || (size_t)ti >= tracks_.size() || !tracks_[ti].has_cur) return;
    tracks_[ti].cur.time = t;
    tracks_[ti].cur.last_time = -1;
}

bool SpAnimationState::track_complete(int ti) const {
    if (ti < 0 || (size_t)ti >= tracks_.size() || !tracks_[ti].has_cur) return true;
    const Entry& e = tracks_[ti].cur;
    return e.anim && e.time >= e.anim->duration;
}

} /* namespace spire */

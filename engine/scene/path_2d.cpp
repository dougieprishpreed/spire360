#include "path_2d.h"

#include "value.h"

#include <math.h>

namespace spire {

Path2D::Path2D() : dirty_(true), bake_interval_(5) {}

/* Curve2D's _data: per point its in handle, its out handle, its position. */
bool Path2D::set(const std::string& key, const Json& v, const PropCtx& ctx) {
    if (key == "curve") {
        pos_.clear();
        in_.clear();
        out_.clear();
        Resource* r = ctx.resource(v);
        if (r && r->props) {
            const Json& d = (*r->props)["_data"];
            const Json& a = d["points"]["v"];
            for (size_t i = 0; i + 5 < a.size(); i += 6) {
                in_.push_back(Vec2(jv_float(a[i]), jv_float(a[i + 1])));
                out_.push_back(Vec2(jv_float(a[i + 2]), jv_float(a[i + 3])));
                pos_.push_back(Vec2(jv_float(a[i + 4]), jv_float(a[i + 5])));
            }
            if (r->props->has("bake_interval")) bake_interval_ = jv_float((*r->props)["bake_interval"], 5);
            if (bake_interval_ <= 0) bake_interval_ = 5;
        }
        dirty_ = true;
        return true;
    }
    return Node2D::set(key, v, ctx);
}

void Path2D::set_point_position(int i, const Vec2& p) {
    if (i < 0 || i >= (int)pos_.size()) return;
    pos_[(size_t)i] = p;
    dirty_ = true;
}

static Vec2 bezier(const Vec2& a, const Vec2& b, const Vec2& c, const Vec2& d, float t) {
    float u = 1 - t;
    return a * (u * u * u) + b * (3 * u * u * t) + c * (3 * u * t * t) + d * (t * t * t);
}

/* Godot's _bake: the segments tessellated finely, then points every
 * bake_interval along the result (the last point at the end). */
void Path2D::bake() {
    dirty_ = false;
    baked_.clear();
    dist_.clear();
    if (pos_.empty()) return;
    std::vector<Vec2> fine;
    fine.push_back(pos_[0]);
    for (size_t i = 0; i + 1 < pos_.size(); ++i) {
        Vec2 a = pos_[i], b = pos_[i] + out_[i], c = pos_[i + 1] + in_[i + 1], d = pos_[i + 1];
        for (int k = 1; k <= 64; ++k) fine.push_back(bezier(a, b, c, d, (float)k / 64.0f));
    }
    baked_.push_back(fine[0]);
    dist_.push_back(0);
    float total = 0, next = bake_interval_;
    for (size_t i = 1; i < fine.size(); ++i) {
        Vec2 s = fine[i] - fine[i - 1];
        float len = sqrtf(s.x * s.x + s.y * s.y);
        while (len > 0 && total + len >= next) {
            float k = (next - total) / len;
            baked_.push_back(fine[i - 1] + s * k);
            dist_.push_back(next);
            next += bake_interval_;
        }
        total += len;
    }
    if (total > dist_.back()) {
        baked_.push_back(fine.back());
        dist_.push_back(total);
    }
}

float Path2D::baked_length() {
    if (dirty_) bake();
    return dist_.empty() ? 0.0f : dist_.back();
}

Vec2 Path2D::sample_baked(float offset, float* rotation) {
    if (dirty_) bake();
    if (rotation) *rotation = 0;
    if (baked_.empty()) return Vec2(0, 0);
    if (baked_.size() == 1) return baked_[0];
    float len = dist_.back();
    if (offset < 0) offset = 0;
    if (offset > len) offset = len;
    size_t i = 1;
    while (i + 1 < dist_.size() && dist_[i] < offset) ++i;
    float seg = dist_[i] - dist_[i - 1];
    float k = seg > 0 ? (offset - dist_[i - 1]) / seg : 0.0f;
    Vec2 a = baked_[i - 1], b = baked_[i];
    if (rotation) {
        Vec2 f = b - a;
        *rotation = atan2f(f.y, f.x);
    }
    return a + (b - a) * k;
}

PathFollow2D::PathFollow2D()
    : progress(0), progress_ratio(0), h_offset(0), v_offset(0), rotates(true), loop(true), use_ratio(false) {}

bool PathFollow2D::set(const std::string& key, const Json& v, const PropCtx& ctx) {
    if (key == "progress") {
        progress = jv_float(v);
        use_ratio = false;
    }
    else if (key == "progress_ratio") {
        progress_ratio = jv_float(v);
        use_ratio = true;
    } else if (key == "h_offset") h_offset = jv_float(v);
    else if (key == "v_offset") v_offset = jv_float(v);
    else if (key == "rotates") rotates = v.b(true);
    else if (key == "loop") loop = v.b(true);
    else return Node2D::set(key, v, ctx);
    return true;
}

void PathFollow2D::process(double) { update_transform(); }

/* PathFollow2D._update_transform */
void PathFollow2D::update_transform() {
    if (!parent || parent->type != "Path2D") return;
    Path2D* p = (Path2D*)parent;
    float len = p->baked_length();
    if (len <= 0) return;
    float off = use_ratio ? progress_ratio * len : progress;
    if (loop) {
        off = fmodf(off, len);
        if (off < 0) off += len;
    }
    float rot = 0;
    Vec2 at = p->sample_baked(off, &rot);
    if (rotates) {
        rotation = rot;
        at = at + Vec2(cosf(rot), sinf(rot)) * h_offset + Vec2(-sinf(rot), cosf(rot)) * v_offset;
    } else {
        at = at + Vec2(h_offset, v_offset);
    }
    position = at;
}

} /* namespace spire */

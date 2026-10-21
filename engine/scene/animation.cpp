#include "animation.h"

#include "controls.h"
#include "path_2d.h"
#include "scene.h"
#include "value.h"

#include <math.h>
#include <stdlib.h>
#include <string.h>

namespace spire {

AnimationPlayer::AnimationPlayer()
    : lib_(NULL), file_(NULL), libraries_(NULL), anim_(NULL), time_(0), length_(1), loop_(false), playing_(false),
      started_(false), active_(true) {}

bool AnimationPlayer::set(const std::string& key, const Json& v, const PropCtx& ctx) {
    if (key == "libraries") {
        libraries_ = &v;
        lib_ = ctx.lib;
        file_ = ctx.file;
        return true;
    }
    if (key == "autoplay" || key == "current_animation") {
        autoplay_ = v.str();
        return true;
    }
    return Node::set(key, v, ctx);
}

/* The props of a sub resource of the player's scene ({"sub": id}). */
static const Json* sub_props(const SceneFile* f, const Json& ref) {
    if (!f || !jv_is_sub(ref)) return NULL;
    const Json& s = f->doc["sub"][ref["sub"].str().c_str()];
    return s.is_object() ? &s["props"] : NULL;
}

/* "library/name", or a name of the default library "". */
const Json* AnimationPlayer::animation(const std::string& name) const {
    if (!libraries_) return NULL;
    std::string lib_name, anim = name;
    size_t slash = name.find('/');
    if (slash != std::string::npos) {
        lib_name = name.substr(0, slash);
        anim = name.substr(slash + 1);
    }
    const Json* lib = sub_props(file_, (*libraries_)[lib_name.c_str()]);
    if (!lib) return NULL;
    return sub_props(file_, (*lib)["_data"][anim.c_str()]);
}

bool AnimationPlayer::play(const std::string& name) {
    const std::string& n = name.empty() ? autoplay_ : name;
    const Json* a = n.empty() ? NULL : animation(n);
    if (!a) return false;
    anim_ = a;
    current_ = n;
    finished_.clear();
    length_ = (*a).has("length") ? jv_float((*a)["length"], 1.0f) : 1.0;
    loop_ = (*a)["loop_mode"].i() != 0;
    time_ = 0;
    playing_ = true;
    started_ = false;
    discrete_.clear();
    return true;
}

void AnimationPlayer::process(double dt) {
    if (!active_) return;
    if (!playing_ && !started_ && !autoplay_.empty() && current_.empty()) play("");
    if (!playing_ || !anim_) return;
    if (!started_) {
        /* The first frame: the keys at the start (Godot seeks to 0). */
        started_ = true;
        apply(0, 0, true);
    }
    double from = time_, to = time_ + dt;
    if (to >= length_) {
        if (loop_ && length_ > 0) {
            apply(from, length_, false);
            to = fmod(to, length_);
            apply(0, to, true);
            time_ = to;
            return;
        }
        to = length_;
        apply(from, to, false);
        time_ = to;
        playing_ = false;
        finished_ = current_;
        return;
    }
    apply(from, to, false);
    time_ = to;
}

/* Godot's Math::ease (a value key's transition). */
static double ease(double x, double c) {
    if (x < 0) x = 0;
    if (x > 1) x = 1;
    if (c > 0) return c < 1 ? 1.0 - pow(1.0 - x, 1.0 / c) : pow(x, c);
    if (c < 0) return x < 0.5 ? pow(x * 2.0, -c) * 0.5 : (1.0 - pow(1.0 - (x - 0.5) * 2.0, -c)) * 0.5 + 0.5;
    return 0;
}

static float key_time(const Json& times, size_t i) { return jv_float(times["v"][i]); }

/* The last key at or before t (0 before the first). */
static size_t key_at(const Json& times, double t) {
    size_t n = times["v"].size(), k = 0;
    while (k + 1 < n && key_time(times, k + 1) <= t) ++k;
    return k;
}

/* Animation::bezier_track_interpolate: a key's points are value, in handle
 * (time, value), out handle (time, value); the curve is solved for time. */
static float bezier_at(const Json& keys, double t) {
    const Json& times = keys["times"];
    const Json& pts = keys["points"]["v"];
    size_t n = times["v"].size();
    if (!n || pts.size() < n * 5) return 0;
    if (t <= key_time(times, 0)) return jv_float(pts[(size_t)0]);
    size_t k = key_at(times, t);
    if (k + 1 >= n) return jv_float(pts[k * 5]);
    float t0 = key_time(times, k), t1 = key_time(times, k + 1);
    float duration = t1 - t0, x = (float)t - t0;
    Vec2 start(0, jv_float(pts[k * 5]));
    Vec2 start_out = start + Vec2(jv_float(pts[k * 5 + 3]), jv_float(pts[k * 5 + 4]));
    Vec2 end(duration, jv_float(pts[(k + 1) * 5]));
    Vec2 end_in = end + Vec2(jv_float(pts[(k + 1) * 5 + 1]), jv_float(pts[(k + 1) * 5 + 2]));
    float low = 0, high = 1;
    Vec2 lo, hi;
    for (int i = 0; i < 10; ++i) {
        float m = (low + high) * 0.5f, u = 1 - m;
        Vec2 p = start * (u * u * u) + start_out * (3 * u * u * m) + end_in * (3 * u * m * m) + end * (m * m * m);
        if (p.x < x) low = m;
        else high = m;
    }
    for (int i = 0; i < 2; ++i) {
        float m = i ? high : low, u = 1 - m;
        Vec2 p = start * (u * u * u) + start_out * (3 * u * u * m) + end_in * (3 * u * m * m) + end * (m * m * m);
        if (i) hi = p;
        else lo = p;
    }
    float c = hi.x - lo.x > 1e-6f ? (x - lo.x) / (hi.x - lo.x) : 0;
    return lo.y + (hi.y - lo.y) * c;
}

/* A number, Vector2 or Color value as components (0 if it is none). */
static int components(const Json& v, float* out) {
    if (v.is_number()) {
        out[0] = jv_float(v);
        return 1;
    }
    Vec2 p;
    Color c;
    if (v["t"].str() == "Color" && jv_color(v, c)) {
        out[0] = c.r;
        out[1] = c.g;
        out[2] = c.b;
        out[3] = c.a;
        return 4;
    }
    if (jv_vec2(v, p)) {
        out[0] = p.x;
        out[1] = p.y;
        return 2;
    }
    return 0;
}

/* The float fields of a node's property (all components, or the one a
 * subproperty names); 0 if it has none. */
static int slots(Node* n, const std::string& prop, const std::string& sub, float** out) {
    int count = 0;
    if (n->is_canvas_item() && (prop == "modulate" || prop == "self_modulate")) {
        Color& c = prop == "modulate" ? ((CanvasItem*)n)->modulate : ((CanvasItem*)n)->self_modulate;
        out[0] = &c.r;
        out[1] = &c.g;
        out[2] = &c.b;
        out[3] = &c.a;
        count = 4;
    } else if (n->is_node2d()) {
        Node2D* d = (Node2D*)n;
        if (prop == "position" || prop == "scale") {
            Vec2& v = prop == "position" ? d->position : d->scale;
            out[0] = &v.x;
            out[1] = &v.y;
            count = 2;
        } else if (prop == "rotation" || prop == "skew") {
            out[0] = prop == "rotation" ? &d->rotation : &d->skew;
            count = 1;
        } else if (n->type == "PathFollow2D" && (prop == "progress" || prop == "progress_ratio")) {
            PathFollow2D* f = (PathFollow2D*)n;
            f->use_ratio = prop == "progress_ratio";
            out[0] = prop == "progress" ? &f->progress : &f->progress_ratio;
            count = 1;
        }
    } else if (n->is_control()) {
        Control* c = (Control*)n;
        if (prop == "scale" || prop == "pivot_offset") {
            Vec2& v = prop == "scale" ? c->scale : c->pivot_offset;
            out[0] = &v.x;
            out[1] = &v.y;
            count = 2;
        } else if (prop == "rotation") {
            out[0] = &c->rotation;
            count = 1;
        }
    }
    if (!count || sub.empty()) return count;
    static const char* const names = "xyzw";
    static const char* const colors = "rgba";
    for (int i = 0; i < count; ++i)
        if (sub.size() == 1 && (sub[0] == names[i] || sub[0] == colors[i])) {
            out[0] = out[i];
            return 1;
        }
    return 0;
}

void AnimationPlayer::apply(double from, double to, bool seeked) {
    const Json& a = *anim_;
    Node* root = parent ? parent : this; /* root_node ".." */
    PropCtx ctx(lib_, file_);
    for (int i = 0;; ++i) {
        std::string base = format("tracks/%d/", i);
        const Json& type = a[(base + "type").c_str()];
        if (!type.is_string()) break;
        if (!a[(base + "enabled").c_str()].b(true)) continue;
        const Json& path_v = a[(base + "path").c_str()];
        std::string path = path_v.is_string() ? path_v.str() : path_v["np"].str();
        const Json& keys = a[(base + "keys").c_str()];
        size_t colon = path.find(':');
        std::string node_path = path.substr(0, colon), prop, sub;
        if (colon != std::string::npos) {
            prop = path.substr(colon + 1);
            size_t c2 = prop.find(':');
            if (c2 != std::string::npos) {
                sub = prop.substr(c2 + 1);
                prop = prop.substr(0, c2);
            }
        }
        Node* target = root->get_node(node_path.empty() ? std::string(".") : node_path);
        if (!target) continue;
        if (type.str() == "method") {
            /* Keys in (from, to], and the start's on seeking to 0. */
            const Json& times = keys["times"];
            for (size_t k = 0; k < times["v"].size(); ++k) {
                double kt = key_time(times, k);
                if (!(kt > from && kt <= to) && !(seeked && kt == from && kt == to)) continue;
                const std::string& m = keys["values"][k]["method"].str();
                if (m == "queue_free") target->queue_free();
                else if (SceneLibrary::current()) SceneLibrary::current()->note("animation method " + m);
            }
            continue;
        }
        float* out[4];
        if (type.str() == "bezier") {
            if (slots(target, prop, sub, out) == 1) *out[0] = bezier_at(keys, to);
            continue;
        }
        if (type.str() != "value") continue;
        const Json& times = keys["times"];
        const Json& values = keys["values"];
        size_t n = times["v"].size();
        if (!n || values.size() < n) continue;
        size_t k = key_at(times, to);
        float va[4], vb[4];
        int count = components(values[k], va);
        bool discrete = keys["update"].i() == 1 || a[(base + "interp").c_str()].i(1) == 0;
        int have = count ? slots(target, prop, sub, out) : 0;
        if (count && have == count) {
            if (!discrete && k + 1 < n && to > key_time(times, k) && components(values[k + 1], vb) == count) {
                double t0 = key_time(times, k), t1 = key_time(times, k + 1);
                double w = t1 > t0 ? (to - t0) / (t1 - t0) : 0;
                float tr = jv_float(keys["transitions"]["v"][k], 1.0f);
                if (tr != 1.0f) w = ease(w, tr);
                for (int c = 0; c < count; ++c) va[c] += (vb[c] - va[c]) * (float)w;
            }
            for (int c = 0; c < count; ++c) *out[c] = va[c];
            continue;
        }
        /* Other values (textures, ...): set when the key changes. */
        std::map<int, int>::iterator it = discrete_.find(i);
        if (it != discrete_.end() && it->second == (int)k) continue;
        discrete_[i] = (int)k;
        std::string key = sub.empty() ? prop : prop + ":" + sub;
        if (!target->set(key, values[k], ctx) && SceneLibrary::current())
            SceneLibrary::current()->note("animation property " + key);
    }
}

} /* namespace spire */

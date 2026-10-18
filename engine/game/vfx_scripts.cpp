/* Effect scripts: ports of the game's generic VFX helpers, which most of
 * the combat effects (scenes/vfx/vfx_*.tscn) are built on. The game frees
 * an effect when it is over; deferred deletion follows the frame traversal. */
#include "../scene/controls.h"
#include "../scene/node.h"
#include "../scene/particles.h"
#include "../scene/resource.h"
#include "../scene/scene.h"
#include "../scene/script.h"
#include "../scene/spine_sprite.h"
#include "../scene/tween.h"
#include "../scene/value.h"
#include "game.h"
#include "ui_util.h"

#include <math.h>

namespace spire {

namespace {

const Json* export_value(Node* n, const char* key) {
    std::map<std::string, const Json*>::const_iterator it = n->extra.find(key);
    return it == n->extra.end() ? NULL : it->second;
}

/* NSpriteAnimator (src/Core/Nodes/Vfx/Utilities/NSpriteAnimator.cs): the
 * Sprite2D shows _frames one after another at _fps (looping with _loop),
 * then goes; _randomizeRotation turns it by a random angle in
 * _rotationRange (degrees). */
class SpriteAnimator : public ScriptBehavior {
public:
    SpriteAnimator(Node* n) : fps_(15), loop_(false), t_(0), done_(false) {
        const Json* frames = export_value(n, "_frames");
        for (size_t i = 0; frames && i < frames->size(); ++i) {
            Resource* r = script_resource((*frames)[i]);
            if (r && r->is_texture()) frames_.push_back((Texture2DRes*)r);
        }
        const Json* fps = export_value(n, "_fps");
        if (fps && fps->num(0) > 0) fps_ = (float)fps->num(15);
        const Json* loop = export_value(n, "_loop");
        loop_ = loop && loop->b();
        const Json* rnd = export_value(n, "_randomizeRotation");
        const Json* range = export_value(n, "_rotationRange");
        if (rnd && rnd->b() && range && n->is_node2d()) {
            const Json& v = (*range)["v"];
            int lo = (int)v[(size_t)0].num(0), hi = (int)v[(size_t)1].num(0);
            int deg = hi > lo ? lo + (int)(chaotic_rand() % (unsigned)(hi - lo)) : lo;
            ((Node2D*)n)->rotation = (float)deg * 3.14159265f / 180.0f;
        }
    }
    virtual void ready(Node* n) { show(n, 0); }
    virtual void process(Node* n, double dt) {
        if (done_ || frames_.empty()) return;
        t_ += dt;
        /* Task.Delay of 1000 / fps ms (rounded) between frames. */
        double interval = floor(1000.0 / fps_ + 0.5) / 1000.0;
        int i = (int)(t_ / interval);
        if (!loop_ && i >= (int)frames_.size()) {
            done_ = true;
            n->queue_free();
            return;
        }
        show(n, i % (int)frames_.size());
    }

private:
    void show(Node* n, int i) {
        if (n->type == "Sprite2D" && !frames_.empty()) ((Sprite2D*)n)->texture = frames_[(size_t)i];
    }
    std::vector<Texture2DRes*> frames_;
    float fps_;
    bool loop_;
    double t_;
    bool done_;
};

void emit_all(Node* n) {
    if (n->type == "GPUParticles2D" || n->type == "CPUParticles2D") ((Particles2D*)n)->emitting = true;
    for (size_t i = 0; i < n->children.size(); ++i) emit_all(n->children[i]);
}

/* NVfxParticleSystem (NVfxParticleSystem.cs): every particle system under
 * it starts emitting; it goes after _lifetime seconds. */
class VfxParticleSystem : public ScriptBehavior {
public:
    VfxParticleSystem(Node* n) : lifetime_(1), t_(0) {
        const Json* l = export_value(n, "_lifetime");
        if (l) lifetime_ = l->num(1);
    }
    virtual void ready(Node* n) { emit_all(n); }
    virtual void process(Node* n, double dt) {
        if (t_ > lifetime_) return;
        t_ += dt;
        if (t_ >= lifetime_) n->queue_free();
    }

private:
    double lifetime_, t_;
};

/* NVfxSpine (NVfxSpine.cs): its SpineSprite plays _animation once, then it
 * goes. */
class VfxSpine : public ScriptBehavior {
public:
    VfxSpine(Node* n) : started_(false), done_(false) {
        const Json* a = export_value(n, "_animation");
        if (a) anim_ = a->str();
    }
    virtual void ready(Node* n) {
        Node* s = n->get_node("SpineSprite");
        if (s && s->type == "SpineSprite") started_ = ((SpineSprite*)s)->set_animation(anim_, false, 0);
    }
    virtual void process(Node* n, double) {
        if (!started_ || done_) return;
        Node* s = n->get_node("SpineSprite");
        SpAnimationState* st = s && s->type == "SpineSprite" ? ((SpineSprite*)s)->animation_state() : NULL;
        if (st && st->track_complete(0)) {
            done_ = true;
            n->queue_free();
        }
    }

private:
    std::string anim_;
    bool started_, done_;
};

static void exported_particles(Node* n, const char* key, std::vector<Particles2D*>& out) {
    const Json* paths = n->get_extra(key);
    for (size_t i = 0; paths && i < paths->size(); ++i) {
        Node* child = n->get_node((*paths)[i]["np"].str());
        if (child && (child->type == "GPUParticles2D" || child->type == "CPUParticles2D"))
            out.push_back((Particles2D*)child);
    }
}

/* NBigSlashVfx.PlaySequence: restart the slash and free it after 1 s.
 * NBigSlashImpactVfx: anticipation, 0.1 s, impact, then 2 s to finish. */
class BigSlash : public ScriptBehavior {
public:
    explicit BigSlash(bool impact) : impact_(impact), t_(0), fired_(false) {}
    virtual void ready(Node* n) {
        exported_particles(n, impact_ ? "_anticipationParticles" : "_slashParticles", first_);
        exported_particles(n, "_impactParticles", impact_particles_);
        for (size_t i = 0; i < first_.size(); ++i) first_[i]->restart();
    }
    virtual void process(Node* n, double dt) {
        t_ += dt;
        if (impact_ && !fired_ && t_ >= 0.1) {
            fired_ = true;
            for (size_t i = 0; i < impact_particles_.size(); ++i) impact_particles_[i]->restart();
        }
        if (t_ >= (impact_ ? 2.1 : 1.0)) n->queue_free();
    }
private:
    bool impact_;
    double t_;
    bool fired_;
    std::vector<Particles2D*> first_, impact_particles_;
};

/* NHitSparkVfx.FlashAndFree waits for its specks' Finished signal. */
class HitSpark : public ScriptBehavior {
public:
    HitSpark() : specks_(NULL), started_(false) {}
    virtual void ready(Node* n) {
        std::vector<Particles2D*> particles;
        exported_particles(n, "_particles", particles);
        for (size_t i = 0; i < particles.size(); ++i) particles[i]->restart();
        const Json* p = n->get_extra("_specks");
        Node* specks = p ? n->get_node((*p)["np"].str()) : NULL;
        if (specks && (specks->type == "GPUParticles2D" || specks->type == "CPUParticles2D"))
            specks_ = (Particles2D*)specks;
    }
    virtual void process(Node* n, double) {
        if (started_ && (!specks_ || specks_->finished())) n->queue_free();
        started_ = true;
    }
private:
    Particles2D* specks_;
    bool started_;
};

static float random_unit() { return (float)(chaotic_rand() % 1000000) / 999999.0f; }

static Vec2 local_point(Node* parent, const Vec2& global) {
    if (!parent || !parent->is_canvas_item()) return global;
    Xform2D xf = ((CanvasItem*)parent)->global_transform();
    Vec2 v = global - xf.origin;
    float determinant = xf.x.x * xf.y.y - xf.x.y * xf.y.x;
    if (fabsf(determinant) < 0.000001f) return Vec2();
    return Vec2((v.x * xf.y.y - v.y * xf.y.x) / determinant,
                (v.y * xf.x.x - v.x * xf.x.y) / determinant);
}

static float exported_number(Node* n, const char* key, float fallback = 0) {
    const Json* p = n->get_extra(key);
    return p ? p->f(fallback) : fallback;
}

/* NShaker interpolates between chaotic offsets, with its frequency in
 * seconds. NOrbVfx reduces Strength with a sine OutIn tween. */
class Shaker : public ScriptBehavior {
public:
    explicit Shaker(Node* n) : target_(NULL), timer_(0), previous_rot_(0), rotation_(0),
        strength_(exported_number(n, "_strength")), initial_(0), time_(0), duration_(0),
        offset_(exported_number(n, "_maxPosOffset")), max_rotation_(exported_number(n, "_maxRotOffset")),
        frequency_(exported_number(n, "_frequency", 0.02f)) {}
    void start(float strength, double duration) { strength_ = initial_ = strength; time_ = 0; duration_ = duration; }
    virtual void ready(Node* n) {
        const Json* p = n->get_extra("_target");
        Node* target = p ? n->get_node(jv_nodepath(*p)) : NULL;
        if (target && target->is_node2d()) target_ = (Node2D*)target;
    }
    virtual void process(Node*, double dt) {
        if (!target_) return;
        if (duration_ > 0) {
            time_ += dt;
            float k = clampf((float)(time_ / duration_), 0, 1);
            float eased = k < 0.5f ? tween_interpolate(TRANS_SINE, EASE_OUT, k * 2, 0, 0.5f, 1)
                                  : tween_interpolate(TRANS_SINE, EASE_IN, (k - 0.5f) * 2, 0.5f, 0.5f, 1);
            strength_ = initial_ * (1 - eased);
        }
        if (strength_ == 0) { target_->position = Vec2(); target_->rotation = 0; return; }
        if (timer_ > 0) {
            timer_ = maxf(0, timer_ - (float)dt);
            float k = frequency_ > 0 ? 1 - timer_ / frequency_ : 1;
            target_->position = previous_ + (offset_to_ - previous_) * k;
            target_->rotation = (previous_rot_ + (rotation_ - previous_rot_) * k) * 3.14159265f / 180;
        } else {
            timer_ = frequency_;
            previous_ = offset_to_; previous_rot_ = rotation_;
            float a = random_unit() * 6.2831853f, radius = random_unit() * offset_ * strength_;
            offset_to_ = Vec2(cosf(a), sinf(a)) * radius;
            rotation_ = (random_unit() * 2 - 1) * max_rotation_;
        }
    }
private:
    Node2D* target_;
    float timer_, previous_rot_, rotation_, strength_, initial_;
    double time_, duration_;
    float offset_, max_rotation_, frequency_;
    Vec2 previous_, offset_to_;
};

static void projectile_emitting(Node* n, bool enabled) {
    if (!n) return;
    const Json* head = n->get_extra("_projectileHead");
    if (head) set_visible(n->get_node(jv_nodepath(*head)), enabled);
    std::vector<Particles2D*> particles;
    exported_particles(n, "_particles", particles);
    for (size_t i = 0; i < particles.size(); ++i) particles[i]->emitting = enabled;
}

/* NVfxProjectileHandler: sample the exported movement and height curves,
 * stop emission at the target, spawn its impact and keep trails for 2 s. */
class ProjectileHandler : public ScriptBehavior {
public:
    explicit ProjectileHandler(Node* n) : projectile_(NULL), time_(0), duration_(0.125f), height_(0),
        delay_(0), configured_(false), impacted_(false), align_(false) {
        const Json* time = n->get_extra("_travelTimeRange");
        const Json* height = n->get_extra("_heightOffsetRange");
        Vec2 tr(0.125f, 0.125f), hr;
        if (time) jv_vec2(*time, tr);
        if (height) jv_vec2(*height, hr);
        duration_ = tr.x + (tr.y - tr.x) * random_unit();
        height_ = hr.x + (hr.y - hr.x) * random_unit();
        read_curve(n, "_movementCurves", movement_);
        read_curve(n, "_pathHeightOffsets", height_curve_);
        const Json* impact = n->get_extra("_impactParticlesScenePath");
        if (impact) impact_ = impact->str();
    }
    void configure(const std::string& path, const Vec2& from, const Vec2& to, double delay) {
        path_ = path; from_ = from; to_ = to; delay_ = delay; configured_ = true;
    }
    virtual void process(Node* n, double dt) {
        if (!configured_) return;
        time_ += dt;
        if (time_ < delay_) return;
        if (!projectile_) {
            Node* p = instance(SceneLibrary::current(), "scenes/" + path_ + ".tscn");
            if (!p || !p->is_node2d()) { delete p; n->queue_free(); return; }
            projectile_ = (Node2D*)p;
            n->add_child(p);
            const Json* align = p->get_extra("_alignToVelocity");
            align_ = align && align->b();
            projectile_emitting(p, true);
            set_position(n, from_);
        }
        double elapsed = time_ - delay_;
        if (!impacted_ && elapsed < duration_) {
            float t = duration_ > 0 ? (float)(elapsed / duration_) : 1;
            float weight = movement_.valid() ? movement_.sample(t) : t;
            float height = height_curve_.valid() ? height_curve_.sample(t) : 0;
            Vec2 dir = to_ - from_;
            float length = sqrtf(dir.x * dir.x + dir.y * dir.y);
            Vec2 normal = length > 0 ? Vec2(dir.y, -dir.x) * (1 / length) : Vec2();
            if (normal.y < 0) normal = normal * -1;
            set_position(n, from_ + dir * weight + normal * (height_ * height));
        } else if (!impacted_) {
            impacted_ = true;
            set_position(n, to_);
            projectile_emitting(projectile_, false);
            if (!impact_.empty()) {
                Node* fx = instance(SceneLibrary::current(), "scenes/" + impact_ + ".tscn");
                if (fx) {
                    n->parent->add_child(fx);
                    if (fx->is_node2d()) ((Node2D*)fx)->position = local_point(n->parent, to_);
                }
            }
        }
        if (elapsed >= duration_ + 2) n->queue_free();
    }
private:
    static void read_curve(Node* n, const char* key, CurveData& out) {
        const Json* curves = n->get_extra(key);
        if (!curves || !curves->size()) return;
        size_t index = (size_t)floorf(random_unit() * (float)(curves->size() - 1) + 0.5f);
        CurveData::from(script_resource((*curves)[index]), out);
    }
    void set_position(Node* n, const Vec2& global) {
        Vec2 local = local_point(n, global);
        Vec2 direction = local - projectile_->position;
        if (align_ && (direction.x != 0 || direction.y != 0)) projectile_->rotation = atan2f(direction.y, direction.x);
        projectile_->position = local;
    }
    Node2D* projectile_;
    double time_;
    float duration_, height_;
    double delay_;
    bool configured_, impacted_, align_;
    Vec2 from_, to_;
    std::string path_, impact_;
    CurveData movement_, height_curve_;
};

ScriptBehavior* make_sprite_animator(Node* n) { return new SpriteAnimator(n); }
ScriptBehavior* make_particle_system(Node* n) { return new VfxParticleSystem(n); }
ScriptBehavior* make_vfx_spine(Node* n) { return new VfxSpine(n); }
ScriptBehavior* make_big_slash(Node*) { return new BigSlash(false); }
ScriptBehavior* make_big_slash_impact(Node*) { return new BigSlash(true); }
ScriptBehavior* make_hit_spark(Node*) { return new HitSpark(); }
ScriptBehavior* make_shaker(Node* n) { return new Shaker(n); }
ScriptBehavior* make_projectile_handler(Node* n) { return new ProjectileHandler(n); }

} /* namespace */

bool shake_vfx(Node* shaker, float strength, double duration) {
    if (!shaker || !shaker->behavior || shaker->script != "res://src/Core/Nodes/Vfx/Utilities/NShaker.cs") return false;
    ((Shaker*)shaker->behavior)->start(strength, duration);
    return true;
}

Node* make_vfx_projectile(SceneLibrary* lib, const std::string& handler, const std::string& projectile,
                          const Vec2& from, const Vec2& to, double delay) {
    Node* node = instance(lib, "scenes/" + handler + ".tscn");
    if (!node || !node->behavior || node->script != "res://src/Core/Nodes/Vfx/Utilities/NVfxProjectileHandler.cs") {
        delete node; return NULL;
    }
    ((ProjectileHandler*)node->behavior)->configure(projectile, from, to, delay);
    return node;
}

void install_vfx_scripts() {
    register_script("res://src/Core/Nodes/Vfx/Utilities/NSpriteAnimator.cs", make_sprite_animator);
    register_script("res://src/Core/Nodes/Vfx/Utilities/NVfxParticleSystem.cs", make_particle_system);
    register_script("res://src/Core/Nodes/Vfx/Utilities/NVfxSpine.cs", make_vfx_spine);
    register_script("res://src/Core/Nodes/Vfx/NBigSlashVfx.cs", make_big_slash);
    register_script("res://src/Core/Nodes/Vfx/NBigSlashImpactVfx.cs", make_big_slash_impact);
    register_script("res://src/Core/Nodes/Vfx/NHitSparkVfx.cs", make_hit_spark);
    register_script("res://src/Core/Nodes/Vfx/Utilities/NShaker.cs", make_shaker);
    register_script("res://src/Core/Nodes/Vfx/Utilities/NVfxProjectileHandler.cs", make_projectile_handler);
}

} /* namespace spire */

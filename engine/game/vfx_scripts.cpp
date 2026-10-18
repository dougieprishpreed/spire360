/* Effect scripts: ports of the game's generic VFX helpers, which most of
 * the combat effects (scenes/vfx/vfx_*.tscn) are built on. The game frees
 * an effect when it is over; deferred deletion follows the frame traversal. */
#include "../scene/controls.h"
#include "../scene/node.h"
#include "../scene/particles.h"
#include "../scene/resource.h"
#include "../scene/script.h"
#include "../scene/spine_sprite.h"
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

ScriptBehavior* make_sprite_animator(Node* n) { return new SpriteAnimator(n); }
ScriptBehavior* make_particle_system(Node* n) { return new VfxParticleSystem(n); }
ScriptBehavior* make_vfx_spine(Node* n) { return new VfxSpine(n); }
ScriptBehavior* make_big_slash(Node*) { return new BigSlash(false); }
ScriptBehavior* make_big_slash_impact(Node*) { return new BigSlash(true); }
ScriptBehavior* make_hit_spark(Node*) { return new HitSpark(); }

} /* namespace */

void install_vfx_scripts() {
    register_script("res://src/Core/Nodes/Vfx/Utilities/NSpriteAnimator.cs", make_sprite_animator);
    register_script("res://src/Core/Nodes/Vfx/Utilities/NVfxParticleSystem.cs", make_particle_system);
    register_script("res://src/Core/Nodes/Vfx/Utilities/NVfxSpine.cs", make_vfx_spine);
    register_script("res://src/Core/Nodes/Vfx/NBigSlashVfx.cs", make_big_slash);
    register_script("res://src/Core/Nodes/Vfx/NBigSlashImpactVfx.cs", make_big_slash_impact);
    register_script("res://src/Core/Nodes/Vfx/NHitSparkVfx.cs", make_hit_spark);
}

} /* namespace spire */

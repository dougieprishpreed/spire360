/* Effect scripts: ports of the game's generic VFX helpers, which most of
 * the combat effects (scenes/vfx/vfx_*.tscn) are built on. The game frees
 * an effect when it is over; here it is hidden (the screen it is on is
 * rebuilt with the next decision). */
#include "../scene/controls.h"
#include "../scene/node.h"
#include "../scene/particles.h"
#include "../scene/resource.h"
#include "../scene/script.h"
#include "../scene/spine_sprite.h"
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
            set_visible(n, false);
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
        if (t_ > lifetime_) set_visible(n, false);
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
            set_visible(n, false);
        }
    }

private:
    std::string anim_;
    bool started_, done_;
};

ScriptBehavior* make_sprite_animator(Node* n) { return new SpriteAnimator(n); }
ScriptBehavior* make_particle_system(Node* n) { return new VfxParticleSystem(n); }
ScriptBehavior* make_vfx_spine(Node* n) { return new VfxSpine(n); }

} /* namespace */

void install_vfx_scripts() {
    register_script("res://src/Core/Nodes/Vfx/Utilities/NSpriteAnimator.cs", make_sprite_animator);
    register_script("res://src/Core/Nodes/Vfx/Utilities/NVfxParticleSystem.cs", make_particle_system);
    register_script("res://src/Core/Nodes/Vfx/Utilities/NVfxSpine.cs", make_vfx_spine);
}

} /* namespace spire */

/* The creatures' visual scripts (MegaCrit.Sts2.Core.Nodes.Vfx.N*Vfx on a
 * creature's spine sprite): each listens to the sprite's animation events
 * and starts (spine-godot's animation_event / animation_started signals,
 * SpineSprite::fired) and drives particles, nodes and slot materials from
 * them. SpineScript is the common part; each port below follows its C#. */
#include "../core/json.h"
#include "../scene/controls.h"
#include "../scene/node.h"
#include "../scene/particles.h"
#include "../scene/script.h"
#include "../scene/spine_sprite.h"
#include "../scene/tween.h"
#include "../spine/spine.h"
#include "game.h"
#include "ui_util.h"

namespace spire {

class SpineScript : public ScriptBehavior {
public:
    SpineScript() : self_(NULL), sprite_(NULL) {}
    virtual void ready(Node* n) {
        self_ = n;
        sprite_ = n->parent && n->parent->type == "SpineSprite" ? (SpineSprite*)n->parent : NULL;
        if (sprite_) on_ready();
    }
    virtual void process(Node*, double dt) {
        if (!sprite_) return;
        const std::vector<SpFired>& f = sprite_->fired();
        for (size_t i = 0; i < f.size(); ++i) {
            if (f[i].start) on_start(current_animation(f[i]));
            else on_event(f[i].name);
        }
        tweens_.update(dt);
        tick(dt);
    }

protected:
    virtual void on_ready() {}
    virtual void on_event(const std::string&) {}
    /* animation_started: MegaAnimationState.GetCurrentAnimationName (track 0) */
    virtual void on_start(const std::string&) {}
    virtual void tick(double) {}

    std::string current_animation(const SpFired& f) {
        if (f.track == 0 && f.anim) return f.anim->name;
        SpAnimationState* st = sprite_->animation_state();
        const SpAnimation* a = st ? st->current(0) : NULL;
        return a ? a->name : std::string();
    }
    /* _parent.GetNode(path) */
    Node* node(const char* path) { return sprite_ ? sprite_->get_node(path) : NULL; }
    static Particles2D* particles(Node* n) {
        return n && (n->type == "GPUParticles2D" || n->type == "CPUParticles2D") ? (Particles2D*)n : NULL;
    }
    /* OneShot = one_shot, Emitting = false */
    static void idle(Particles2D* p, bool one_shot) {
        if (!p) return;
        p->one_shot = one_shot;
        p->emitting = false;
    }
    static void restart(Particles2D* p) {
        if (p) p->restart();
    }
    static void emit(Particles2D* p, bool on) {
        if (p) p->emitting = on;
    }
    /* a slot node's shader parameter (MegaSlotNode.GetNormalMaterial()'s) */
    static float* param(Node* slot, const char* name) { return slot && slot->is_canvas_item() ? &((CanvasItem*)slot)->shader_params[name] : NULL; }

    Node* self_;
    SpineSprite* sprite_;
    TweenSet tweens_;
};

/* NIroncladVfx: the slash's step swept from its base when the slash
 * starts; the eyes' fire while casting. */
class IroncladVfx : public SpineScript {
    Node* slash_;
    Node* eye_fire_;
    void on_ready() {
        slash_ = node("SlashVfxSlot");
        eye_fire_ = node("EyeSlot/EyeFire");
        clear();
    }
    void sweep(float wait, float seconds, TweenTrans trans) {
        float* x = param(slash_, "step.x");
        float* y = param(slash_, "step.y");
        if (!x || !y) return;
        tweens_.kill(x);
        tweens_.kill(y);
        *x = 0; /* _slashStepBase */
        *y = 0.02f;
        tweens_.tween(x, 1.0f, seconds, trans, EASE_IN, wait);
        tweens_.tween(y, 1.02f, seconds, trans, EASE_IN, wait);
    }
    void clear() { set_visible(eye_fire_, false); }
    void on_event(const std::string& e) {
        if (e == "heavy_slash_start") sweep(0, 0.35f, TRANS_CUBIC);
        else if (e == "attack_slash_start") sweep(0.15f, 0.2f, TRANS_QUAD);
        else if (e == "cast_eyes_start") set_visible(eye_fire_, true);
        else if (e == "clear_vfx") clear();
    }
    void on_start(const std::string& anim) {
        if (anim != "cast") clear();
    }

public:
    IroncladVfx() : slash_(NULL), eye_fire_(NULL) {}
};

/* NDefectVfx: sparks from either hand. */
class DefectVfx : public SpineScript {
    Particles2D* left_;
    Particles2D* right_;
    void on_ready() {
        left_ = particles(node("SparkSlot/SparkLeftParticles"));
        right_ = particles(node("SparkSlot/SparkRightParticles"));
        idle(left_, true);
        idle(right_, true);
    }
    void on_event(const std::string& e) {
        if (e == "fire_sparks_left") restart(left_);
        else if (e == "fire_sparks_right") restart(right_);
    }

public:
    DefectVfx() : left_(NULL), right_(NULL) {}
};

/* NNecrobinderVfx: the scythe's flames, the slash (master_step and opacity
 * swept), hurt and low-health particles; the head's flame hidden while
 * dying. */
class NecrobinderVfx : public SpineScript {
    Node* head_;
    Node* slash_;
    Particles2D* scythe1_;
    Particles2D* scythe2_;
    Particles2D* hurt_;
    Particles2D* low_health_;
    void on_ready() {
        head_ = node("HeadBoneNode");
        slash_ = node("SlashVfxSlot");
        scythe1_ = particles(node("ScytheVfxSlot1/ScytheParticles"));
        scythe2_ = particles(node("ScytheVfxSlot2/ScytheParticles"));
        hurt_ = particles(node("HeadBoneNode/HurtParticles"));
        low_health_ = particles(node("HeadBoneNode/LowHealthParticles"));
        idle(scythe1_, true);
        idle(scythe2_, true);
        idle(hurt_, true);
        emit(low_health_, false);
    }
    void on_event(const std::string& e) {
        if (e == "scythe_fx1") restart(scythe1_);
        else if (e == "scythe_fx2") restart(scythe2_);
        else if (e == "attack_slash_start") {
            float* step = param(slash_, "master_step");
            float* opacity = param(slash_, "opacity");
            if (!step || !opacity) return;
            tweens_.kill(step);
            tweens_.kill(opacity);
            *step = 0.04f;
            tweens_.tween(step, 0.235f, 0.4f, TRANS_QUAD, EASE_IN);
            *opacity = 0.4f;
            tweens_.tween(opacity, 0.0f, 0.45f, TRANS_QUAD, EASE_IN_OUT);
        } else if (e == "low_health_start") emit(low_health_, true);
        else if (e == "low_health_end") emit(low_health_, false);
        else if (e == "hurt_particles_start") restart(hurt_);
    }
    void on_start(const std::string& anim) { set_visible(head_, anim != "die"); }

public:
    NecrobinderVfx() : head_(NULL), slash_(NULL), scythe1_(NULL), scythe2_(NULL), hurt_(NULL), low_health_(NULL) {}
};

/* NRegentVfx: the death's particles and explosion, the two weapons'
 * attacks in turn. */
class RegentVfx : public SpineScript {
    Particles2D* arm_;
    Particles2D* chest_;
    Particles2D* back_;
    Particles2D* leg_;
    Particles2D* leg_l_;
    Particles2D* explosion_;
    SpineSprite* weapon1_;
    SpineSprite* weapon2_;
    int weapon_;
    static SpineSprite* spine(Node* n) { return n && n->type == "SpineSprite" ? (SpineSprite*)n : NULL; }
    void on_ready() {
        arm_ = particles(node("SpineArmBone/Particles"));
        chest_ = particles(node("SpineChestBone/Particles"));
        back_ = particles(node("SpineChestBone/ParticlesBack"));
        leg_ = particles(node("SpineLegBone/Particles"));
        leg_l_ = particles(node("SpineLegBoneL/Particles"));
        explosion_ = particles(node("Explosion"));
        weapon1_ = spine(node("Weapons/WeaponAnim1"));
        weapon2_ = spine(node("Weapons/WeaponAnim2"));
        dying_off();
        emit(explosion_, false);
    }
    void dying_off() {
        emit(arm_, false);
        emit(chest_, false);
        emit(back_, false);
        emit(leg_, false);
        emit(leg_l_, false);
    }
    void on_event(const std::string& e) {
        if (e == "death_particles_start") {
            restart(arm_);
            restart(leg_);
            restart(leg_l_);
        } else if (e == "death_particles_start2") {
            restart(chest_);
            restart(back_);
        } else if (e == "death_particles_end") dying_off();
        else if (e == "explode_dead") restart(explosion_);
        else if (e == "explode_end") emit(explosion_, false);
        else if (e == "attack1") {
            if (weapon_ == 1) {
                if (weapon1_) weapon1_->set_animation("attack", false, 0);
                weapon_ = 2;
            } else {
                if (weapon2_) weapon2_->set_animation("attack2", false, 0);
                weapon_ = 1;
            }
        }
    }
    void on_start(const std::string& anim) {
        if (anim == "die") return;
        emit(explosion_, false);
        dying_off();
    }

public:
    RegentVfx()
        : arm_(NULL), chest_(NULL), back_(NULL), leg_(NULL), leg_l_(NULL), explosion_(NULL), weapon1_(NULL), weapon2_(NULL),
          weapon_(1) {}
};

template <class T> static ScriptBehavior* make(Node*) { return new T(); }

void install_creature_vfx_scripts() {
    register_script("res://src/Core/Nodes/Vfx/NIroncladVfx.cs", make<IroncladVfx>);
    register_script("res://src/Core/Nodes/Vfx/NDefectVfx.cs", make<DefectVfx>);
    register_script("res://src/Core/Nodes/Vfx/NNecrobinderVfx.cs", make<NecrobinderVfx>);
    register_script("res://src/Core/Nodes/Vfx/NRegentVfx.cs", make<RegentVfx>);
}

} /* namespace spire */

/* The creatures' visual scripts (MegaCrit.Sts2.Core.Nodes.Vfx.N*Vfx on a
 * creature's spine sprite): each listens to the sprite's animation events
 * and starts (spine-godot's animation_event / animation_started signals,
 * SpineSprite::fired) and drives particles, nodes and slot materials from
 * them. SpineScript is the common part; each port below follows its C#. */
#include "../core/json.h"
#include "form_vfx.h"
#include "../scene/controls.h"
#include "../scene/line_2d.h"
#include "../scene/node.h"
#include "../scene/particles.h"
#include "../scene/script.h"
#include "../scene/spine_sprite.h"
#include "../scene/tween.h"
#include "../spine/spine.h"
#include "../scene/scene.h"
#include "../scene/shaders.h"
#include "combat_vfx.h"
#include "game.h"
#include "screen_shake.h"
#include "ui_util.h"

#include <stdlib.h>
#include <string.h>

namespace spire {

namespace SpineScriptHelpers {
/* Node2D.GlobalPosition = g */
inline void set_global(Node2D* n, const Vec2& g) {
    Xform2D pt = n->parent && n->parent->is_canvas_item() ? ((CanvasItem*)n->parent)->global_transform() : Xform2D();
    Vec2 d = g - pt.origin;
    float det = pt.x.x * pt.y.y - pt.y.x * pt.x.y;
    if (det == 0) return;
    n->position = Vec2((d.x * pt.y.y - d.y * pt.y.x) / det, (pt.x.x * d.y - pt.x.y * d.x) / det);
}
}

class SpineScript : public ScriptBehavior {
public:
    SpineScript() : self_(NULL), sprite_(NULL) {}
    virtual void ready(Node* n) {
        self_ = n;
        /* the sprite: the script's parent, or its exported _spineNode */
        const Json* p = n->get_extra("_spineNode");
        Node* s = p ? n->get_node((*p)["np"].str()) : n->parent;
        sprite_ = s && s->type == "SpineSprite" ? (SpineSprite*)s : NULL;
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
    Node2D* node2d(const char* path) {
        Node* n = node(path);
        return n && n->is_node2d() ? (Node2D*)n : NULL;
    }
    /* a slot node's normal material (MegaSlotNode.GetNormalMaterial()) */
    static const MaterialRes* slot_material(Node* slot) {
        return slot && slot->type == "SpineSlotNode" ? ((SpineFollowNode*)slot)->slot_material[0] : NULL;
    }
    /* SetShaderParameter(name, Vector2) on a slot node's material */
    static void set_param2(Node* slot, const char* name, const Vec2& v) {
        float* x = param(slot, (std::string(name) + ".x").c_str());
        float* y = param(slot, (std::string(name) + ".y").c_str());
        if (x && y) {
            *x = v.x;
            *y = v.y;
        }
    }
    /* the animation now on a track of the sprite */
    std::string track_animation(int track) {
        SpAnimationState* st = sprite_->animation_state();
        const SpAnimation* a = st ? st->current(track) : NULL;
        return a ? a->name : std::string();
    }
    /* a scene added to the fight's NCombatRoom.CombatVfxContainer at a
     * point (VfxCmd.PlayVfx) */
    void play_vfx(const Vec2& at, const char* path) {
        Node* root = self_;
        while (root && root->parent) root = root->parent;
        Node* box = root ? find_named(root, "CombatVfxContainer") : NULL;
        SceneLibrary* lib = SceneLibrary::current();
        Node* fx = box && lib ? instance(lib, std::string("scenes/") + path + ".tscn") : NULL;
        if (!fx) return;
        Vec2 local = box->is_canvas_item() ? at - ((CanvasItem*)box)->global_transform().origin : at;
        if (fx->is_node2d()) ((Node2D*)fx)->position = local;
        else if (fx->is_control()) ((Control*)fx)->set_position(local);
        box->add_child(fx);
    }
    /* an exported NodePath, from the script's node */
    Node* exported(const char* name) {
        const Json* p = self_->get_extra(name);
        return p ? self_->get_node((*p)["np"].str()) : NULL;
    }
    /* an exported NodePath array */
    std::vector<Node*> exported_list(const char* name) {
        std::vector<Node*> out;
        const Json* p = self_->get_extra(name);
        for (size_t i = 0; p && i < p->size(); ++i) {
            Node* n = self_->get_node((*p)[i]["np"].str());
            if (n) out.push_back(n);
        }
        return out;
    }
    static Vec2 global_position(Node2D* n) { return n->global_transform().origin; }
    static void set_global_position(Node2D* n, const Vec2& g) { SpineScriptHelpers::set_global(n, g); }
    /* a float shader parameter's value in a material */
    static float material_float(const MaterialRes* m, const char* name, float fallback) {
        if (!m || !m->props) return fallback;
        std::string k = std::string("shader_parameter/") + name;
        return m->props->has(k.c_str()) ? (float)(*m->props)[k.c_str()].num(fallback) : fallback;
    }
    static float chaotic(float a, float b) { return a + (b - a) * (float)(rand() % 10001) / 10000.0f; }
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

/* NKinPriestBeamVfx (the Kin Priest's "Visuals/Beam"): the beam flickers
 * (its scale and the node's alpha jittered each frame); Fire shows it, the
 * node turning from 1 to -1 degree over 0.8 s while the holder stretches to
 * 4 (0.38 s) and back to 0.5 (0.6 s), then hides. */
class KinPriestBeam : public ScriptBehavior {
public:
    KinPriestBeam() : self_(NULL), beam_(NULL), holder_(NULL), particles_(NULL), firing_(false) {}
    virtual void ready(Node* n) {
        self_ = n->is_node2d() ? (Node2D*)n : NULL;
        Node* b = n->get_node("BeamHolder/Beam");
        Node* h = n->get_node("BeamHolder");
        beam_ = b && b->is_node2d() ? (Node2D*)b : NULL;
        holder_ = h && h->is_node2d() ? (Node2D*)h : NULL;
        Node* pn = n->get_node("BeamHolder/StaticParticles");
        particles_ = pn && (pn->type == "GPUParticles2D" || pn->type == "CPUParticles2D") ? (Particles2D*)pn : NULL;
        if (beam_) base_ = beam_->scale;
        if (particles_) particles_->emitting = false;
        set_visible(particles_, false);
        set_visible(holder_, false);
    }
    virtual void process(Node*, double dt) {
        if (!self_) return;
        if (beam_) beam_->scale = base_ + Vec2(rnd(-0.05f, 0.05f), rnd(-0.7f, 0.7f));
        self_->modulate.a = rnd(0.8f, 1.0f);
        tweens_.update(dt);
        if (firing_ && !tweens_.running()) {
            firing_ = false;
            if (particles_) particles_->emitting = false;
            set_visible(particles_, false);
            set_visible(holder_, false);
        }
    }
    void fire() {
        if (!self_ || !holder_) return;
        if (particles_) particles_->restart();
        set_visible(particles_, true);
        set_visible(holder_, true);
        tweens_.clear();
        self_->rotation = 1.0f * 3.14159265f / 180.0f;
        tweens_.tween(&self_->rotation, -1.0f * 3.14159265f / 180.0f, 0.8f, TRANS_CUBIC, EASE_OUT);
        holder_->scale = Vec2(1, 1);
        tweens_.tween(&holder_->scale.x, 4.0f, 0.38f, TRANS_EXPO, EASE_OUT);
        tweens_.tween(&holder_->scale.x, 0.5f, 0.6f, TRANS_EXPO, EASE_IN, 0.38f);
        firing_ = true;
    }

private:
    static float rnd(float a, float b) { return a + (b - a) * (float)(rand() % 10001) / 10000.0f; }
    Node2D* self_;
    Node2D* beam_;
    Node2D* holder_;
    Particles2D* particles_;
    Vec2 base_;
    bool firing_;
    TweenSet tweens_;
};

static const char* const KIN_BEAM_SCRIPT = "res://src/Core/Nodes/Vfx/NKinPriestBeamVfx.cs";

/* NKinPriestBeamVfx.Fire on its node (false when it is not one). */
bool kin_priest_beam_fire(Node* beam) {
    if (!beam || !beam->behavior || beam->script != KIN_BEAM_SCRIPT) return false;
    ((KinPriestBeam*)beam->behavior)->fire();
    return true;
}

/* The scripts that only switch particles and nodes from the sprite's
 * events, as data: each op runs on a node (a path from the sprite) when
 * its moment comes: "ready", "event:<name>", "start" (any animation
 * started), "start!<anim>" (an animation other than <anim> started) or
 * "start=<anim>" (<anim> started).
 * Ops: 'o' one shot and idle, '0' stop emitting, '1' emit, 'r' restart,
 * 'v' show, 'h' hide, 'c' clear a trail's points, 'F' fire the Kin Priest's
 * beam, 'T' / 't' put the bone at the target (SetTarget's creature) 150 /
 * 130 px above its feet. play: what RunWhenSpineReady sets on the sprite. */
struct VfxOp {
    const char* when;
    const char* path;
    char op;
};
struct VfxPlay {
    const char* anim;
    bool loop;
    int track;
};
struct VfxTable {
    const char* script;
    VfxPlay play[2];
    VfxOp ops[32];
};

static const VfxTable VFX_TABLES[] = {
    { "res://src/Core/Nodes/Vfx/NBattlewornDummyVfx.cs", {},
      { { "ready", "ParticlesSlot/DamageParticles", 'o' }, { "event:fire_chips", "ParticlesSlot/DamageParticles", 'r' } } },
    { "res://src/Core/Nodes/Vfx/NKaiserCrabBossExplosionVfx.cs", {},
      { { "ready", "ExplosionSlot/ExplosionParticles", 'o' }, { "event:left_embers_start", "ExplosionSlot/ExplosionParticles", 'r' } } },
    { "res://src/Core/Nodes/Vfx/NFakeMerchantVfx.cs", {},
      { { "ready", "ParticlesSlot/Particles", '0' }, { "event:particles_start", "ParticlesSlot/Particles", '1' },
        { "event:particles_end", "ParticlesSlot/Particles", '0' } } },
    { "res://src/Core/Nodes/Vfx/NParafrightVfx.cs", {},
      { { "ready", "ParticlesSlot/Particles", '0' }, { "event:particles_start", "ParticlesSlot/Particles", '1' },
        { "event:particles_end", "ParticlesSlot/Particles", '0' } } },
    { "res://src/Core/Nodes/Vfx/NTheObscuraVfx.cs", {},
      { { "ready", "ParticlesSlot/Particles", '0' }, { "event:particles_start", "ParticlesSlot/Particles", '1' },
        { "event:particles_end", "ParticlesSlot/Particles", '0' } } },
    { "res://src/Core/Nodes/Vfx/NPhantasmalGardenerVfx.cs", {},
      { { "ready", "SpewSlotNode/SpewParticles", '0' }, { "event:spew_start", "SpewSlotNode/SpewParticles", '1' },
        { "event:spew_end", "SpewSlotNode/SpewParticles", '0' } } },
    { "res://src/Core/Nodes/Vfx/NSpinyToadVfx.cs", { { "explode", true, 0 } },
      { { "ready", "BurstParticles", 'o' }, { "ready", "BurstParticlesSub", 'o' }, { "event:explode", "BurstParticles", 'r' } } },
    { "res://src/Core/Nodes/Vfx/NDevotedSculptorVfx.cs", {},
      { { "ready", "VoiceBoneNode/VoiceParticles", 'o' }, { "ready", "AttackParticles", 'o' },
        { "event:caw", "VoiceBoneNode/VoiceParticles", 'r' }, { "event:attack", "AttackParticles", 'r' } } },
    { "res://src/Core/Nodes/Vfx/NHunterKillerVfx.cs", {},
      { { "ready", "MouthBone/Particles", '0' }, { "event:spit_start", "MouthBone/Particles", '1' },
        { "event:spit_end", "MouthBone/Particles", '0' }, { "start!cast", "MouthBone/Particles", '0' } } },
    { "res://src/Core/Nodes/Vfx/NFogmogVfx.cs", {},
      { { "ready", "ThrustSlotNode/ThrustParticles", '0' }, { "ready", "DustSlotNode/DustLeftParticles", '0' },
        { "ready", "DustSlotNode/DustRightParticles", '0' }, { "event:thrust_start", "ThrustSlotNode/ThrustParticles", 'r' },
        { "event:thrust_start", "DustSlotNode/DustRightParticles", 'r' }, { "event:thrust_start", "DustSlotNode/DustLeftParticles", 'r' },
        { "event:thrust_end", "ThrustSlotNode/ThrustParticles", '0' }, { "event:thrust_end", "DustSlotNode/DustLeftParticles", '0' },
        { "event:thrust_end", "DustSlotNode/DustRightParticles", '0' } } },
    { "res://src/Core/Nodes/Vfx/NFossilStalkerVfx.cs", {},
      { { "ready", "DebuffBone/DebuffParticles", '0' }, { "event:debuff_spray_start", "DebuffBone/DebuffParticles", '1' },
        { "event:debuff_spray_stop", "DebuffBone/DebuffParticles", '0' }, { "start!attack", "DebuffBone/DebuffParticles", '0' } } },
    /* the Queen's script sits in SpineParticleSlot (its _spineNode the sprite) */
    { "res://src/Core/Nodes/Vfx/NQueenVfx.cs", {},
      { { "ready", "SpineParticleSlot/Vfx/GPUParticles2D", '0' }, { "event:attack_start", "SpineParticleSlot/Vfx/GPUParticles2D", 'r' },
        { "event:attack_end", "SpineParticleSlot/Vfx/GPUParticles2D", '0' },
        { "start!attack", "SpineParticleSlot/Vfx/GPUParticles2D", '0' } } },
    { "res://src/Core/Nodes/Vfx/NFuzzyWurmCrawlerSpitTrailVfx.cs", {},
      { { "ready", "SpitParticlesBone/TrailParticles", '0' }, { "event:launch_start", "SpitParticlesBone/TrailParticles", 'r' },
        { "event:launch_end", "SpitParticlesBone/TrailParticles", '0' }, { "start!attack", "SpitParticlesBone/TrailParticles", '0' } } },
    { "res://src/Core/Nodes/Vfx/NSeapunkVfx.cs", { { "cast", true, 0 } },
      { { "ready", "BubbleParticles", 'o' }, { "ready", "WeedParticles", 'o' }, { "event:bubbles_start", "BubbleParticles", 'r' },
        { "event:weeds_start", "WeedParticles", 'r' } } },
    { "res://src/Core/Nodes/Vfx/NArchitectVfx.cs", { { "idle_loop", true, 0 }, { "_tracks/head_normal", true, 1 } },
      { { "event:trail_start", "TrailSlot/TrailInner", 'v' }, { "event:trail_start", "TrailSlot/TrailOuter", 'v' },
        { "event:trail_start", "TrailSlot/TrailInner", 'c' }, { "event:trail_start", "TrailSlot/TrailOuter", 'c' },
        { "event:trail_end", "TrailSlot/TrailInner", 'h' }, { "event:trail_end", "TrailSlot/TrailOuter", 'h' } } },
    { "res://src/Core/Nodes/Vfx/NKinPriestVfx.cs", { { "attack_laser", true, 0 } },
      { { "ready", "TorchFireBone/SparkParticles", '0' }, { "event:sparks_start", "TorchFireBone/SparkParticles", '1' },
        { "event:sparks_end", "TorchFireBone/SparkParticles", '0' }, { "event:laser_fire", "Beam", 'F' } } },
    { "res://src/Core/Nodes/Vfx/NSkulkingColonyVfx.cs", {},
      { { "ready", "ParticleSlot1/Particles", 'o' }, { "ready", "ParticleSlot2/ParticlesWide", 'o' },
        { "ready", "ParticleSlot3/Particles", 'o' }, { "event:take_damage", "ParticleSlot1/Particles", 'r' },
        { "event:take_fatal_damage", "ParticleSlot2/ParticlesWide", 'r' }, { "event:final_poof", "ParticleSlot3/Particles", 'r' } } },
    { "res://src/Core/Nodes/Vfx/NFlyconidSporesVfx.cs", {},
      { { "ready", "SporesAttach/FlyconidSporesVfx/FrailSpores", '0' }, { "ready", "SporesAttach/FlyconidSporesVfx/VulnerableSpores", '0' },
        { "event:cast_start", "SporesAttach/FlyconidSporesVfx/FrailSpores", 'r' },
        { "event:cast_start", "SporesAttach/FlyconidSporesVfx/VulnerableSpores", 'r' },
        { "event:cast_end", "SporesAttach/FlyconidSporesVfx/FrailSpores", '0' },
        { "event:cast_end", "SporesAttach/FlyconidSporesVfx/VulnerableSpores", '0' },
        { "start!attack", "SporesAttach/FlyconidSporesVfx/FrailSpores", '0' },
        { "start!attack", "SporesAttach/FlyconidSporesVfx/VulnerableSpores", '0' } } },
    { "res://src/Core/Nodes/Vfx/NLostAndForgottenVfx.cs", { { "die", true, 0 } },
      { { "ready", "GranuleEmitterBone/DustParticles", '0' }, { "start!die", "GranuleEmitterBone/DustParticles", '0' },
        { "event:start_dust", "GranuleEmitterBone/DustParticles", '1' }, { "event:stop_dust", "GranuleEmitterBone/DustParticles", '0' } } },
    { "res://src/Core/Nodes/Vfx/NSpectralKnightVfx.cs", {},
      { { "ready", "FlameParticlesAdd", '0' }, { "ready", "FlameParticlesFlat", '0' }, { "ready", "CinderParticles", '0' },
        { "event:flame_start", "FlameParticlesAdd", 'r' }, { "event:flame_start", "FlameParticlesFlat", 'r' },
        { "event:flame_start", "CinderParticles", 'r' }, { "event:flame_end", "FlameParticlesAdd", '0' },
        { "event:flame_end", "FlameParticlesFlat", '0' }, { "event:flame_end", "CinderParticles", '0' },
        { "start!attack", "FlameParticlesAdd", '0' }, { "start!attack", "FlameParticlesFlat", '0' }, { "start!attack", "CinderParticles", '0' } } },
    { "res://src/Core/Nodes/Vfx/NGasBombVfx.cs", {},
      { { "ready", "SmokeBallSlot/ExplodePuffParticles", 'o' }, { "ready", "SmokeBallSlot/BitParticles", 'o' },
        { "event:burst", "SmokeBallSlot/DotParticles", '0' }, { "event:burst", "SmokeBallSlot/PuffParticles", '0' },
        { "event:burst", "SmokeBallSlot/PuffParticles", 'h' }, { "event:burst", "SmokeBallSlot/ExplodePuffParticles", 'r' },
        { "event:burst", "SmokeBallSlot/BitParticles", 'r' }, { "event:idle_particles", "SmokeBallSlot/DotParticles", '1' },
        { "event:idle_particles", "SmokeBallSlot/PuffParticles", '1' }, { "event:idle_particles", "SmokeBallSlot/PuffParticles", 'v' },
        { "event:dissipate", "SmokeBallSlot/DotParticles", '0' }, { "event:dissipate", "SmokeBallSlot/PuffParticles", '0' } } },
    { "res://src/Core/Nodes/Vfx/NSlimedBerserkerVfx.cs", {},
      { { "ready", "ParticleSlotNodeR/GooParticles", '0' }, { "ready", "ParticleSlotNodeL/GooParticles", '0' },
        { "ready", "ParticleSlotNodeVomit/GooParticles", '0' }, { "event:goo_start", "ParticleSlotNodeR/GooParticles", '1' },
        { "event:goo_start", "ParticleSlotNodeL/GooParticles", '1' }, { "event:goo_stop", "ParticleSlotNodeR/GooParticles", '0' },
        { "event:goo_stop", "ParticleSlotNodeL/GooParticles", '0' }, { "event:vomit_start", "ParticleSlotNodeVomit/GooParticles", '1' },
        { "event:vomit_stop", "ParticleSlotNodeVomit/GooParticles", '0' } } },
    { "res://src/Core/Nodes/Vfx/NHauntedShipVfx.cs", {},
      { { "ready", "EyeBone1/BubbleParticles", '0' }, { "ready", "EyeBone2/BubbleParticles", '0' }, { "ready", "EyeBone3/BubbleParticles", '0' },
        { "event:eye_bubbles_start", "EyeBone1/BubbleParticles", '1' }, { "event:eye_bubbles_start", "EyeBone2/BubbleParticles", '1' },
        { "event:eye_bubbles_start", "EyeBone3/BubbleParticles", '1' }, { "event:eye_bubbles_end", "EyeBone1/BubbleParticles", '0' },
        { "event:eye_bubbles_end", "EyeBone2/BubbleParticles", '0' }, { "event:eye_bubbles_end", "EyeBone3/BubbleParticles", '0' },
        { "event:head_bubbles_start", "HeadSlot/BubbleParticles", '1' }, { "event:head_bubbles_start", "HeadSlot/BubbleParticles2", '1' },
        { "event:head_bubbles_end", "HeadSlot/BubbleParticles", '0' }, { "event:head_bubbles_end", "HeadSlot/BubbleParticles2", '0' } } },
    { "res://src/Core/Nodes/Vfx/NKinFollowerVfx.cs", {},
      { { "ready", "Boomerang1Slot/Trail", 'h' }, { "ready", "Boomerang2Slot/Trail", 'h' }, { "ready", "HaySlot/HayParticles", 'o' },
        { "event:start_trail1", "Boomerang1Slot/Trail", 'c' }, { "event:start_trail1", "Boomerang1Slot/Trail", 'v' },
        { "event:end_trail1", "Boomerang1Slot/Trail", 'h' }, { "event:start_trail2", "Boomerang2Slot/Trail", 'c' },
        { "event:start_trail2", "Boomerang2Slot/Trail", 'v' }, { "event:end_trail2", "Boomerang2Slot/Trail", 'h' },
        { "event:start_hay", "HaySlot/HayParticles", 'r' } } },
    { "res://src/Core/Nodes/Vfx/NPhrogParasiteVfx.cs", { { "die", true, 0 } },
      { { "ready", "BubbleABoneNode/WormParticlesA", '0' }, { "ready", "BubbleBSlotNode/WormParticlesB", '0' },
        { "ready", "BubbleCBoneNode/WormParticlesC", '0' }, { "ready", "DeathParticles", 'o' }, { "ready", "DeathWormParticles", 'o' },
        { "start", "BubbleABoneNode/WormParticlesA", '0' }, { "start", "BubbleBSlotNode/WormParticlesB", '0' },
        { "start", "BubbleCBoneNode/WormParticlesC", '0' }, { "event:infect", "BubbleABoneNode/WormParticlesA", '1' },
        { "event:infect", "BubbleBSlotNode/WormParticlesB", '1' }, { "event:infect", "BubbleCBoneNode/WormParticlesC", '1' },
        { "event:stop_infect", "BubbleABoneNode/WormParticlesA", '0' }, { "event:stop_infect", "BubbleBSlotNode/WormParticlesB", '0' },
        { "event:stop_infect", "BubbleCBoneNode/WormParticlesC", '0' }, { "event:explode", "DeathParticles", 'r' },
        { "event:explode", "DeathWormParticles", 'r' } } },
    { "res://src/Core/Nodes/Vfx/NSnappingJaxfruitVfx.cs", {},
      { { "ready", "ProjectileAttachBone/BlobParticles", '0' }, { "ready", "ProjectileAttachBone/GlowParticles", '0' },
        { "ready", "ProjectileAttachBone", 'h' }, { "event:cast_start", "TargetBone", 't' },
        { "event:cast_start", "ProjectileAttachBone", 'v' },
        { "event:cast_start", "ProjectileAttachBone/Trail", 'c' }, { "event:cast_start", "ProjectileAttachBone/BlobParticles", 'r' },
        { "event:cast_start", "ProjectileAttachBone/GlowParticles", 'r' }, { "event:cast_end", "ProjectileAttachBone/BlobParticles", '0' },
        { "event:cast_end", "ProjectileAttachBone/GlowParticles", '0' }, { "event:cast_end", "ProjectileAttachBone", 'h' },
        { "start!attack", "ProjectileAttachBone/BlobParticles", '0' }, { "start!attack", "ProjectileAttachBone/GlowParticles", '0' },
        { "start!attack", "ProjectileAttachBone", 'h' } } },
    { "res://src/Core/Nodes/Vfx/NMyteVfx.cs", { { "cast", true, 0 } }, { { "event:start_cast", "TargetBone", 'T' } } },
    { "res://src/Core/Nodes/Vfx/NSoulNexusVfx.cs", {},
      { { "ready", "PathSlot1/Trail", 'h' }, { "ready", "PathSlot2/Trail", 'h' }, { "ready", "PathSlot3/Trail", 'h' },
        { "event:hide_fire", "HeadFireSlot/FireTexture", 'h' }, { "event:show_fire", "HeadFireSlot/FireTexture", 'v' },
        { "event:path_1_start", "PathSlot1/Trail", 'v' }, { "event:path_1_start", "PathSlot1/Trail", 'c' },
        { "event:path_1_stop", "PathSlot1/Trail", 'h' }, { "event:path_2_start", "PathSlot2/Trail", 'v' },
        { "event:path_2_start", "PathSlot2/Trail", 'c' }, { "event:path_2_stop", "PathSlot2/Trail", 'h' },
        { "event:path_3_start", "PathSlot3/Trail", 'v' }, { "event:path_3_start", "PathSlot3/Trail", 'c' },
        { "event:path_3_stop", "PathSlot3/Trail", 'h' } } },
    { "res://src/Core/Nodes/Vfx/NMechaKnightVfx.cs", {},
      { { "ready", "FlameParticlesBone/FlameParticlesDark", '0' }, { "ready", "FlameParticlesBone/FlameParticlesLight", '0' },
        { "ready", "FlameParticlesBone/CinderParticles", '0' }, { "ready", "FlameParticlesBone/GlowParticles", '0' },
        { "ready", "EngineSlot/EngineBone/EngineParticles", '0' }, { "ready", "EngineSlot/EngineBone/EngineParticlesDark", '0' },
        { "event:flame_start", "FlameParticlesBone/FlameParticlesDark", 'r' }, { "event:flame_start", "FlameParticlesBone/FlameParticlesLight", 'r' },
        { "event:flame_start", "FlameParticlesBone/CinderParticles", 'r' }, { "event:flame_start", "FlameParticlesBone/GlowParticles", 'r' },
        { "event:flame_end", "FlameParticlesBone/FlameParticlesDark", '0' }, { "event:flame_end", "FlameParticlesBone/FlameParticlesLight", '0' },
        { "event:flame_end", "FlameParticlesBone/CinderParticles", '0' }, { "event:flame_end", "FlameParticlesBone/GlowParticles", '0' },
        { "event:engine_start", "EngineSlot/EngineBone/EngineParticles", 'r' },
        { "event:engine_start", "EngineSlot/EngineBone/EngineParticlesDark", 'r' },
        { "event:engine_stop", "EngineSlot/EngineBone/EngineParticles", '0' },
        { "event:engine_stop", "EngineSlot/EngineBone/EngineParticlesDark", '0' },
        { "start!attack", "FlameParticlesBone/FlameParticlesDark", '0' }, { "start!attack", "FlameParticlesBone/FlameParticlesLight", '0' },
        { "start!attack", "FlameParticlesBone/CinderParticles", '0' }, { "start!attack", "FlameParticlesBone/GlowParticles", '0' },
        /* the flames hidden while hurt */
        { "start=hurt", "FlameParticlesBone/FlameParticlesDark", 'h' }, { "start=hurt", "FlameParticlesBone/FlameParticlesLight", 'h' },
        { "start=hurt", "FlameParticlesBone/CinderParticles", 'h' }, { "start=hurt", "FlameParticlesBone/GlowParticles", 'h' },
        { "start!hurt", "FlameParticlesBone/FlameParticlesDark", 'v' }, { "start!hurt", "FlameParticlesBone/FlameParticlesLight", 'v' },
        { "start!hurt", "FlameParticlesBone/CinderParticles", 'v' }, { "start!hurt", "FlameParticlesBone/GlowParticles", 'v' } } },
    { "res://src/Core/Nodes/Vfx/NTheInsatiableVfx.cs", {},
      { { "ready", "SalivaSlotNode/SalivaFountainParticles", '0' }, { "ready", "SalivaSlotNode/SalivaDroolParticles", '0' },
        { "ready", "SalivaSlotNode/SalivaCloudParticles", '0' }, { "ready", "BaseBlastSlot/BaseBlastParticles", '0' },
        { "event:saliva_start", "SalivaSlotNode/SalivaFountainParticles", 'r' }, { "event:saliva_start", "SalivaSlotNode/SalivaCloudParticles", 'r' },
        { "event:saliva_end", "SalivaSlotNode/SalivaFountainParticles", '0' }, { "event:saliva_end", "SalivaSlotNode/SalivaCloudParticles", '0' },
        { "event:drool_start", "SalivaSlotNode/SalivaDroolParticles", 'r' }, { "event:drool_end", "SalivaSlotNode/SalivaDroolParticles", '0' },
        { "event:base_blast_start", "BaseBlastSlot/BaseBlastParticles", '1' }, { "event:base_blast_end", "BaseBlastSlot/BaseBlastParticles", '0' },
        { "start!attack_thrash", "BaseBlastSlot/BaseBlastParticles", '0' },
        { "start!salivate", "SalivaSlotNode/SalivaFountainParticles", '0' }, { "start!salivate", "SalivaSlotNode/SalivaCloudParticles", '0' } } },
    { "res://src/Core/Nodes/Vfx/NKnowledgeDemonVfx.cs", { { "idle_loop", true, 0 } },
      { { "ready", "FireSlot1/FireHolder1", 'h' }, { "ready", "FireSlot2/FireHolder2", 'h' }, { "ready", "FireSlot3/FireHolder3", 'h' },
        { "ready", "FireSlot4/FireHolder4", 'h' }, { "ready", "ExplosionParticles", 'o' }, { "ready", "DamageParticles", 'o' },
        { "ready", "EmberParticles", '0' }, { "ready", "ThinEmberParticles", '0' }, { "event:explode", "ExplosionParticles", 'r' },
        { "event:take_damage", "DamageParticles", 'r' }, { "event:burning_start", "FireSlot1/FireHolder1", 'v' },
        { "event:burning_start", "FireSlot2/FireHolder2", 'v' }, { "event:burning_start", "FireSlot3/FireHolder3", 'v' },
        { "event:burning_start", "FireSlot4/FireHolder4", 'v' }, { "event:burning_end", "FireSlot1/FireHolder1", 'h' },
        { "event:burning_end", "FireSlot2/FireHolder2", 'h' }, { "event:burning_end", "FireSlot3/FireHolder3", 'h' },
        { "event:burning_end", "FireSlot4/FireHolder4", 'h' }, { "event:embers_start", "EmberParticles", 'r' },
        { "event:embers_end", "EmberParticles", '0' }, { "event:thin_embers_start", "ThinEmberParticles", 'r' },
        { "event:thin_embers_end", "ThinEmberParticles", '0' }, { "start", "FireSlot1/FireHolder1", 'h' },
        { "start", "FireSlot2/FireHolder2", 'h' }, { "start", "FireSlot3/FireHolder3", 'h' },
        { "start", "FireSlot4/FireHolder4", 'h' }, { "start", "EmberParticles", '0' }, { "start", "ThinEmberParticles", '0' } } },
};

class TableVfx : public SpineScript {
public:
    explicit TableVfx(const VfxTable* t) : t_(t), has_target_(false) {}
    /* SetTarget: the target's NCreature position (none: false) */
    void set_target(bool has, const Vec2& at) {
        has_target_ = has;
        target_ = at;
    }

private:
    /* the ops for the moment: when ("ready", "event:<name>", "start");
     * a start also runs the "start!<anim>" ops for animations not <anim> */
    void run(const std::string& when, const std::string& anim = std::string()) {
        for (size_t i = 0; i < sizeof(t_->ops) / sizeof(t_->ops[0]) && t_->ops[i].when; ++i) {
            const VfxOp& op = t_->ops[i];
            bool now = when == op.when || (when == "start" && strncmp(op.when, "start!", 6) == 0 && anim != op.when + 6) ||
                       (when == "start" && strncmp(op.when, "start=", 6) == 0 && anim == op.when + 6);
            if (!now) continue;
            Node* n = node(op.path);
            Particles2D* p = particles(n);
            switch (op.op) {
            case 'o': idle(p, true); break;
            case '0': emit(p, false); break;
            case '1': emit(p, true); break;
            case 'r': restart(p); break;
            case 'v': set_visible(n, true); break;
            case 'h': set_visible(n, false); break;
            case 'c':
                if (n && n->type == "Line2D") ((Line2D*)n)->points.clear();
                break;
            case 'F': kin_priest_beam_fire(n); break;
            case 'T':
            case 't':
                if (has_target_ && n && n->is_node2d())
                    set_global_position((Node2D*)n, target_ - Vec2(0, op.op == 'T' ? 150.0f : 130.0f));
                break;
            }
        }
    }
    void on_ready() {
        run("ready");
        for (int i = 0; i < 2 && t_->play[i].anim; ++i) sprite_->set_animation(t_->play[i].anim, t_->play[i].loop, t_->play[i].track);
    }
    void on_event(const std::string& e) { run("event:" + e); }
    void on_start(const std::string& anim) { run("start", anim); }

    const VfxTable* t_;
    bool has_target_;
    Vec2 target_;
};

template <int I> static ScriptBehavior* make_table(Node*) { return new TableVfx(&VFX_TABLES[I]); }
template <int N> struct RegisterTables {
    static void run() {
        RegisterTables<N - 1>::run();
        register_script(VFX_TABLES[N - 1].script, make_table<N - 1>);
    }
};
template <> struct RegisterTables<0> {
    static void run() {}
};

/* NAxebotVfx: hurt sparks from either side in turn, death sparks, the
 * landing's smoke. */
class AxebotVfx : public SpineScript {
    Particles2D* hurt1_;
    Particles2D* hurt2_;
    Particles2D* smoke_l_;
    Particles2D* smoke_r_;
    int weapon_;
    void on_ready() {
        hurt1_ = particles(node("SparksBoneNode/HurtParticles1"));
        hurt2_ = particles(node("SparksBoneNode/HurtParticles2"));
        smoke_l_ = particles(node("SmokeNodeLeft/SmokeParticles"));
        smoke_r_ = particles(node("SmokeNodeRight/SmokeParticles"));
        idle(hurt1_, true);
        idle(hurt2_, true);
        emit(smoke_l_, false);
        emit(smoke_r_, false);
    }
    void on_event(const std::string& e) {
        if (e == "start_hurt_sparks") {
            restart(weapon_ == 1 ? hurt1_ : hurt2_);
            weapon_ = weapon_ == 1 ? 2 : 1;
        } else if (e == "start_death_sparks1") restart(hurt1_);
        else if (e == "start_death_sparks2") restart(hurt2_);
        else if (e == "landing_smoke_start") {
            restart(smoke_l_);
            restart(smoke_r_);
        } else if (e == "landing_smoke_end") {
            emit(smoke_l_, false);
            emit(smoke_r_, false);
        }
    }

public:
    AxebotVfx() : hurt1_(NULL), hurt2_(NULL), smoke_l_(NULL), smoke_r_(NULL), weapon_(1) {}
};

/* NKaiserCrabBossVfx: the crab's steam, spit, plow chunks, regeneration
 * splats, rocket thrust (stopped as the right arm's animation, track 2,
 * moves on) and the left claw's explosion. */
class KaiserCrabBossVfx : public SpineScript {
    Particles2D *regen_, *plow_, *steam1_, *steam2_, *steam3_, *smoke_, *spark_, *spit_;
    Node2D* claw_;
    void on_ready() {
        regen_ = particles(node("RegenSplatSlot/RegenSplatParticles"));
        plow_ = particles(node("PlowChunkSlot/PlowChunkParticles"));
        steam1_ = particles(node("RocketSlot/SteamParticles1"));
        steam2_ = particles(node("RocketSlot/SteamParticles2"));
        steam3_ = particles(node("RocketSlot/SteamParticles3"));
        spark_ = particles(node("RocketSlot/SparkParticles"));
        smoke_ = particles(node("RocketSlot/SmokeParticles"));
        spit_ = particles(node("SpittleSlot/SpittleParticles"));
        Node* c = sprite_->find_unique("LeftArmExplosionPosition");
        claw_ = c && c->is_node2d() ? (Node2D*)c : NULL;
        emit(regen_, false);
        emit(plow_, false);
        emit(steam1_, false);
        emit(steam2_, false);
        emit(steam3_, false);
        emit(spit_, false);
        emit(spark_, false);
    }
    void steam(bool on) {
        if (on) {
            restart(steam1_);
            restart(steam2_);
            restart(steam3_);
        } else {
            emit(steam1_, false);
            emit(steam2_, false);
            emit(steam3_, false);
        }
    }
    void thrust(bool on) {
        if (on) {
            restart(smoke_);
            restart(spark_);
        } else {
            emit(smoke_, false);
            emit(spark_, false);
        }
    }
    void on_event(const std::string& e) {
        if (e == "charge_steam_start") steam(true);
        else if (e == "charge_steam_end") steam(false);
        else if (e == "regen_splats_start") restart(regen_);
        else if (e == "regen_splats_end") emit(regen_, false);
        else if (e == "death_spit_start") restart(spit_);
        else if (e == "death_spit_end") emit(spit_, false);
        else if (e == "plow_chunks_start") restart(plow_);
        else if (e == "plow_chunks_end") emit(plow_, false);
        else if (e == "rocket_thrust_start") thrust(true);
        else if (e == "rocket_thrust_end") thrust(false);
        else if (e == "claw_explode_l" && claw_) play_vfx(claw_->global_transform().origin, "vfx/monsters/kaiser_crab_boss_explosion");
    }
    void on_start(const std::string&) {
        std::string right = track_animation(2);
        if (right != "right/charged_loop" && right != "right/charge_up" && right != "right/hurt_charged") steam(false);
        if (right != "right/attack_heavy") thrust(false);
    }

public:
    KaiserCrabBossVfx()
        : regen_(NULL), plow_(NULL), steam1_(NULL), steam2_(NULL), steam3_(NULL), smoke_(NULL), spark_(NULL), spit_(NULL),
          claw_(NULL) {}
};

/* NSoulFyshVfx: the soundwave's and the beckoning wave's "amount" swept
 * up and back down (their slots' materials). */
class SoulFyshVfx : public SpineScript {
    float* sound_;
    float* beckon_;
    void on_ready() {
        sound_ = param(node("Soundwave"), "amount");
        beckon_ = param(node("Beckonwave"), "amount");
        if (sound_) *sound_ = 0.3f;
        if (beckon_) *beckon_ = 0.3f;
        sprite_->set_animation("attack_debuff", true, 0);
    }
    void sweep(float* p, float to, float seconds, TweenEase ease) {
        if (!p) return;
        tweens_.kill(p);
        tweens_.tween(p, to, seconds, TRANS_QUAD, ease);
    }
    void on_event(const std::string& e) {
        if (e == "soundwave_start") sweep(sound_, 1.0f, 0.45f, EASE_OUT);
        else if (e == "soundwave_end") sweep(sound_, 0.3f, 0.5f, EASE_IN);
        else if (e == "beckon_start") sweep(beckon_, 1.0f, 0.25f, EASE_OUT);
        else if (e == "beckon_end") sweep(beckon_, 0.3f, 0.5f, EASE_IN);
    }

public:
    SoulFyshVfx() : sound_(NULL), beckon_(NULL) {}
};

/* NVantomVfx: the tail dissolving (its slot's step to 1 over a second,
 * then the charge on track 1), the spray and the death's spray and
 * explosion. */
class VantomVfx : public SpineScript {
    float* step_;
    Particles2D *spray_, *death_, *death_back_, *explosion_;
    double charge_at_; /* the dissolve's end (< 0: none) */
    double t_;
    void on_ready() {
        step_ = param(node("TailSlotNode"), "step");
        spray_ = particles(node("SprayBoneNode/SprayParticles"));
        death_ = particles(node("DeathSpraySlotNode/DeathSprayParticles"));
        death_back_ = particles(node("DeathSprayBackSlotNode/DeathSprayParticlesBack"));
        explosion_ = particles(node("DeathExplosionSlotNode/DeathExplosionParticles"));
        if (step_) *step_ = -0.1f;
        sprite_->set_animation("idle_loop", true, 0);
        sprite_->set_animation("_tracks/charged_0", true, 1);
        emit(spray_, false);
        emit(death_, false);
        emit(death_back_, false);
        idle(explosion_, true);
    }
    void on_event(const std::string& e) {
        if (e == "dissolve_tail" && step_) {
            tweens_.kill(step_);
            tweens_.tween(step_, 1.0f, 1.0f, TRANS_QUAD, EASE_IN);
            charge_at_ = t_ + 1.0;
        } else if (e == "spray_on") emit(spray_, true);
        else if (e == "spray_off") emit(spray_, false);
        else if (e == "death_spray_on") {
            emit(death_, true);
            emit(death_back_, true);
        } else if (e == "death_spray_off") {
            emit(death_, false);
            emit(death_back_, false);
        } else if (e == "death_explosion") restart(explosion_);
    }
    void on_start(const std::string& anim) {
        if (anim != "die" && step_) *step_ = 0;
    }
    void tick(double dt) {
        t_ += dt;
        if (charge_at_ >= 0 && t_ >= charge_at_) {
            charge_at_ = -1;
            sprite_->set_animation("_tracks/charge_up_1", false, 1);
            sprite_->add_animation("_tracks/charged_1", true, 0, 1);
        }
    }

public:
    VantomVfx() : step_(NULL), spray_(NULL), death_(NULL), death_back_(NULL), explosion_(NULL), charge_at_(-1), t_(0) {}
};

/* NSewerClamVfx: the death, darkness and chomp particles; the coral
 * growing 0.2 a time (to 1.5), springing to it after half a second. */
class SewerClamVfx : public SpineScript {
    Particles2D *death_, *buff_, *chomp_;
    Node2D* coral_;
    void on_ready() {
        death_ = particles(node("MouthSlot/DeathParticles"));
        buff_ = particles(node("MouthSlot/BuffParticles"));
        chomp_ = particles(node("MouthSlot/ChompParticles"));
        coral_ = node2d("CoralScaleBone");
        idle(death_, true);
        emit(buff_, false);
        idle(chomp_, true);
        if (coral_) coral_->scale = Vec2(0.1f, 0.1f);
    }
    void on_event(const std::string& e) {
        if (e == "death_explode") restart(death_);
        else if (e == "darkness_start") restart(buff_);
        else if (e == "darkness_end") emit(buff_, false);
        else if (e == "chomp") restart(chomp_);
        else if (e == "grow" && coral_) {
            float to = coral_->scale.x + 0.2f;
            if (to >= 1.5f) to = 1.5f;
            tweens_.kill(&coral_->scale);
            tweens_.tween_from(&coral_->scale, Vec2(to - 0.2f, to - 0.2f), Vec2(to, to), 0.5f, TRANS_ELASTIC, EASE_OUT, 0.5f);
        }
    }

public:
    SewerClamVfx() : death_(NULL), buff_(NULL), chomp_(NULL), coral_(NULL) {}
};

/* NOilSpillVfx: the drool, the spray attack (2000 particles) and the
 * death's (500), the slam's raindrops (800, lasting 0.75 s). */
class OilSpillVfx : public SpineScript {
    Particles2D *drool_, *spray_, *rain_;
    void on_ready() {
        drool_ = particles(node("MouthDribbleBoneNode/DribbleParticles"));
        spray_ = particles(node("MouthSpraySlot/SprayParticles"));
        rain_ = particles(node("MouthSpraySlot/RainDropParticles"));
        if (rain_) rain_->one_shot = true;
        restart(drool_);
        restart(rain_);
        restart(spray_);
        emit(spray_, false);
        emit(rain_, false);
    }
    void on_event(const std::string& e) {
        if ((e == "spray_start" || e == "death_spray_start") && spray_) {
            spray_->amount = e == "spray_start" ? 2000 : 500;
            spray_->emitting = true;
        } else if (e == "spray_end" || e == "death_spray_end") emit(spray_, false);
        else if (e == "drool_start") emit(drool_, true);
        else if (e == "drool_end") emit(drool_, false);
        else if (e == "slam_spray_start" && rain_) {
            rain_->one_shot = false;
            rain_->amount = 800;
            rain_->explosiveness = 0;
            rain_->lifetime = 0.75;
            rain_->restart();
        } else if (e == "slam_spray_end") emit(rain_, false);
    }
    void on_start(const std::string& anim) {
        if (anim != "slam") emit(spray_, false);
        if (anim != "spray") emit(rain_, false);
    }

public:
    OilSpillVfx() : drool_(NULL), spray_(NULL), rain_(NULL) {}
};

/* NLivingGasVfx: attack, hurt, debuff and death-breath puffs; dissipating
 * (the three smoke slots' AlphaStep from their own to (1, 1), Quart In over
 * 1.4 s) and reconstituting. */
class LivingGasVfx : public SpineScript {
    Particles2D *attack_puff_, *attack_spark_, *debuff_, *gas_;
    Node* smoke_[3];
    Vec2 step_[3];
    float dissipate_; /* tweened 0..1 */
    void on_ready() {
        attack_puff_ = particles(node("AttackFXNode/AttackPuffParticles"));
        attack_spark_ = particles(node("AttackFXNode/AttackSparkParticles"));
        debuff_ = particles(node("AttackFXNode/DebuffPuffParticles"));
        gas_ = particles(node("AttackFXNode/GasPuffParticles"));
        static const char* const slots[3] = { "SmokeSlot1", "SmokeSlot2", "SmokeSlot3" };
        for (int i = 0; i < 3; ++i) {
            smoke_[i] = node(slots[i]);
            step_[i] = shader_param_vec2(slot_material(smoke_[i]), "AlphaStep", Vec2(0, 1));
        }
        emit(attack_puff_, false);
        emit(attack_spark_, false);
        emit(debuff_, false);
        emit(gas_, false);
    }
    void apply(float t) {
        for (int i = 0; i < 3; ++i) set_param2(smoke_[i], "AlphaStep", step_[i] + (Vec2(1, 1) - step_[i]) * t);
    }
    void on_event(const std::string& e) {
        if (e == "attack_start") {
            emit(attack_puff_, true);
            if (attack_spark_) attack_spark_->amount = 24;
            restart(attack_spark_);
        } else if (e == "attack_end") {
            emit(attack_puff_, false);
            emit(attack_spark_, false);
        } else if (e == "hurt_start") {
            if (attack_spark_) attack_spark_->amount = 100;
            emit(attack_spark_, true);
        } else if (e == "hurt_end") emit(attack_spark_, false);
        else if (e == "debuff_start") emit(debuff_, true);
        else if (e == "debuff_end") emit(debuff_, false);
        else if (e == "gas_breath_start") emit(gas_, true);
        else if (e == "gas_breath_end") emit(gas_, false);
        else if (e == "dissipate") {
            tweens_.kill(&dissipate_);
            dissipate_ = 0;
            tweens_.tween(&dissipate_, 1.0f, 1.4f, TRANS_QUART, EASE_IN);
        } else if (e == "reconstitute") {
            tweens_.kill(&dissipate_);
            dissipate_ = -1;
            apply(0);
        }
    }
    void tick(double) {
        if (dissipate_ >= 0) apply(dissipate_);
    }

public:
    LivingGasVfx() : attack_puff_(NULL), attack_spark_(NULL), debuff_(NULL), gas_(NULL), dissipate_(-1) {
        for (int i = 0; i < 3; ++i) smoke_[i] = NULL;
    }
};

/* NEntomancerVfx: the swarm flying to its target node and back (1 s Cubic
 * Out, 1.5 s Cubic InOut), the attacking bugs on meanwhile. */
class EntomancerVfx : public SpineScript {
    Node2D* swarm_;
    Node2D* target_;
    Particles2D* attacking_;
    Vec2 base_;
    double done_at_, t_;
    void on_ready() {
        swarm_ = node2d("SwarmParticles");
        target_ = node2d("SwarmTargetNode");
        attacking_ = particles(node("SwarmParticles/AttackingBugParticles"));
        if (swarm_) base_ = swarm_->position;
        emit(attacking_, false);
    }
    void on_event(const std::string& e) {
        if (e == "launch_swarm" && swarm_ && target_) {
            emit(attacking_, true);
            tweens_.kill(&swarm_->position);
            tweens_.tween(&swarm_->position, target_->position, 1.0f, TRANS_CUBIC, EASE_OUT);
            tweens_.tween(&swarm_->position, base_, 1.5f, TRANS_CUBIC, EASE_IN_OUT, 1.0f);
            done_at_ = t_ + 2.51;
        } else if (e == "turn_off_swarm") emit(particles(swarm_), false);
    }
    void tick(double dt) {
        t_ += dt;
        if (done_at_ >= 0 && t_ >= done_at_) {
            done_at_ = -1;
            emit(attacking_, false);
        }
    }

public:
    EntomancerVfx() : swarm_(NULL), target_(NULL), attacking_(NULL), done_at_(-1), t_(0) {}
};

/* NWaterfallGiantVfx: the steam vents (shown again and restarted when
 * hidden: EmitGracefully), the waterfall, the leaks building up (their
 * scale, amount and lifetime), the explosion hiding the steam. */
class WaterfallGiantVfx : public SpineScript {
    Particles2D* steam_[6];
    Particles2D* leak_[3];
    Particles2D *mist_, *mouth_, *droplets_;
    bool dead_;
    static void gracefully(Particles2D* p) {
        if (!p) return;
        if (!p->visible) {
            p->visible = true;
            p->restart();
        } else {
            p->emitting = true;
        }
    }
    void on_ready() {
        static const char* const steams[6] = { "SteamSlot1/steamParticles1", "SteamSlot2/steamParticles2", "SteamSlot3/steamParticles3",
                                               "SteamSlot4/steamParticles4", "SteamSlot5/steamParticles5", "SteamSlot6/steamParticles6" };
        static const char* const leaks[3] = { "SteamLeakSlot1/steamLeakParticles1", "SteamLeakSlot2/steamLeakParticles2",
                                              "SteamLeakSlot3/steamLeakParticles3" };
        for (int i = 0; i < 6; ++i) {
            steam_[i] = particles(node(steams[i]));
            emit(steam_[i], false);
        }
        for (int i = 0; i < 3; ++i) {
            leak_[i] = particles(node(leaks[i]));
            emit(leak_[i], false);
        }
        mist_ = particles(node("MistSlot/MistParticles"));
        droplets_ = particles(node("MistSlot/Droplets"));
        mouth_ = particles(node("MouthDropletsSlot/MouthDroplets"));
    }
    void buildup(float smin, float smax, int amount, double lifetime) {
        if (dead_) return;
        for (int i = 0; i < 3; ++i) {
            Particles2D* p = leak_[i];
            gracefully(p);
            if (!p) continue;
            p->pmin[Particles2D::P_SCALE] = smin;
            p->pmax[Particles2D::P_SCALE] = smax;
            p->amount = amount;
            p->lifetime = lifetime;
        }
    }
    void on_event(const std::string& e) {
        if (e == "steam_1_start") gracefully(steam_[0]);
        else if (e == "steam_1_end") emit(steam_[0], false);
        else if (e == "steam_2_start") gracefully(steam_[1]);
        else if (e == "steam_2_end") emit(steam_[1], false);
        else if (e == "steam_3_start") {
            gracefully(steam_[2]);
            gracefully(steam_[3]);
        } else if (e == "steam_3_end") {
            emit(steam_[2], false);
            emit(steam_[3], false);
        } else if (e == "steam_5_start") {
            gracefully(steam_[4]);
            gracefully(steam_[5]);
        } else if (e == "steam_5_end") {
            emit(steam_[4], false);
            emit(steam_[5], false);
        } else if (e == "waterfall_start") {
            emit(mouth_, true);
            emit(droplets_, true);
            emit(mist_, true);
            dead_ = false;
        } else if (e == "waterfall_end") {
            emit(mouth_, false);
            emit(droplets_, false);
            emit(mist_, false);
        } else if (e == "explode") {
            for (int i = 0; i < 6; ++i) set_visible(steam_[i], false);
            for (int i = 0; i < 3; ++i) set_visible(leak_[i], false);
            dead_ = true;
        } else if (e == "buildup1") buildup(0.2f, 0.4f, 10, 0.37);
        else if (e == "buildup2") buildup(0.3f, 0.6f, 20, 0.75);
        else if (e == "buildup3") buildup(0.4f, 0.8f, 28, 1.2);
        else if (e == "clear_death_steam") {
            for (int i = 2; i < 6; ++i) {
                emit(steam_[i], false);
                set_visible(steam_[i], false);
            }
            dead_ = false;
        }
    }

public:
    WaterfallGiantVfx() : mist_(NULL), mouth_(NULL), droplets_(NULL), dead_(false) {
        for (int i = 0; i < 6; ++i) steam_[i] = NULL;
        for (int i = 0; i < 3; ++i) leak_[i] = NULL;
    }
};

/* NAeonGlassVfx: the rings on track 1 following the body's animation
 * (normal, the attacks' then normal, the death's), the liquid's scroll
 * speed (reversed while withering), the wither, leak, shard, dump, spark
 * and ground particles. */
class AeonGlassVfx : public SpineScript {
    Node* liquid_;
    float base_scroll_;
    Particles2D *wither_, *leak_, *shard_, *dump_, *top_, *bottom_, *dust_, *chunk_;
    std::string cur_;
    bool rings_normal_;
    void reset() {
        float* s = param(liquid_, "ScrollSpeed");
        if (s) *s = base_scroll_;
        restart(wither_);
        emit(wither_, false);
        restart(leak_);
        emit(leak_, false);
        emit(shard_, false);
        restart(dump_);
        emit(dump_, false);
        emit(top_, false);
        emit(bottom_, false);
        emit(chunk_, false);
        emit(dust_, false);
    }
    void on_ready() {
        liquid_ = node("LiquidSlot");
        base_scroll_ = material_float(slot_material(liquid_), "ScrollSpeed", 0);
        wither_ = particles(node("WitherSlot/WitherParticles"));
        leak_ = particles(node("LiquidSlot/LeakParticles"));
        shard_ = particles(node("GlassCenterSlot/ShardParticles"));
        dump_ = particles(node("GlassCenterSlot/DumpParticles"));
        top_ = particles(node("TopSparksSlot/TopSparkParticles"));
        bottom_ = particles(node("BottomSparksSlot/BottomSparkParticles"));
        dust_ = particles(node("GroundPlowSlot/DustParticles"));
        chunk_ = particles(node("GroundPlowSlot/ChunkParticles"));
        Particles2D* shots[5] = { bottom_, top_, wither_, shard_, dump_ };
        for (int i = 0; i < 5; ++i)
            if (shots[i]) shots[i]->one_shot = true;
        reset();
        sprite_->set_animation("_track1/rings_normal", true, 1);
    }
    void on_event(const std::string& e) {
        if (e == "start_wither") {
            restart(wither_);
            float* s = param(liquid_, "ScrollSpeed");
            if (s) *s = -2.0f;
        } else if (e == "end_wither") reset();
        else if (e == "start_die") {
            restart(dump_);
            restart(leak_);
            restart(shard_);
        } else if (e == "end_die") emit(leak_, false);
        else if (e == "start_sparks") {
            restart(top_);
            restart(bottom_);
        } else if (e == "start_ground_scrape") {
            restart(chunk_);
            restart(dust_);
        } else if (e == "end_ground_scrape") {
            emit(chunk_, false);
            emit(dust_, false);
        }
    }
    void on_start(const std::string& anim) {
        reset();
        if (anim == cur_) return;
        cur_ = anim;
        if (anim == "idle_loop" || anim == "hurt" || anim == "wither") {
            if (!rings_normal_) sprite_->set_animation("_track1/rings_normal", true, 1);
            rings_normal_ = true;
        }
        if (anim == "attack_heavy" || anim == "attack_double") {
            sprite_->set_animation("_track1/rings_" + anim, false, 1);
            sprite_->add_animation("_track1/rings_normal", true, 0, 1);
            rings_normal_ = false;
        } else if (anim == "die") {
            sprite_->set_animation("_track1/rings_die", false, 1);
            rings_normal_ = false;
        }
    }

public:
    AeonGlassVfx()
        : liquid_(NULL), base_scroll_(0), wither_(NULL), leak_(NULL), shard_(NULL), dump_(NULL), top_(NULL), bottom_(NULL), dust_(NULL),
          chunk_(NULL), rings_normal_(false) {}
};

/* NAmalgamVfx: its torches (out and lit again), the hits on each torch,
 * the laser's base and its hit (at the hit bone), the death's poof. */
class AmalgamVfx : public SpineScript {
    Particles2D *hit_fx_, *death_, *laser_base_, *hit_[3], *sparks_[3];
    Node2D* hit_bone_;
    Node* torch_[3];
    void on_ready() {
        hit_fx_ = particles(exported("_hitFxParticles"));
        Node* hb = exported("_hitBoneNode");
        hit_bone_ = hb && hb->is_node2d() ? (Node2D*)hb : NULL;
        death_ = particles(node("CPUDeathParticles"));
        idle(death_, true);
        laser_base_ = particles(node("laserBaseBone/laserBaseParticles"));
        emit(laser_base_, false);
        static const char* const torches[3] = { "torch1Slot/fire1_small_green", "torch2Slot/fire2_small_green", "torch3Slot/fire3_small_green" };
        static const char* const hits[3] = { "torch1UnscaledBone/hitParticles", "torch2UnscaledBone/hitParticles", "torch3UnscaledBone/hitParticles" };
        static const char* const sparks[3] = { "torch1Slot/constantParticles", "torch2Slot/constantParticles", "torch3Slot/constantParticles" };
        for (int i = 0; i < 3; ++i) {
            torch_[i] = node(torches[i]);
            set_visible(torch_[i], true);
            hit_[i] = particles(node(hits[i]));
            idle(hit_[i], true);
            sparks_[i] = particles(node(sparks[i]));
        }
        set_visible(hit_fx_, false);
    }
    void on_event(const std::string& e) {
        if (e == "hit1") restart(hit_[0]);
        else if (e == "hit2") restart(hit_[1]);
        else if (e == "hit3") restart(hit_[2]);
        else if (e == "go_poof") restart(death_);
        else if (e == "torches_out" || e == "torches_on") {
            bool on = e == "torches_on";
            for (int i = 0; i < 3; ++i) {
                set_visible(torch_[i], on);
                emit(sparks_[i], on);
            }
        } else if (e == "laser_base_fire") {
            set_visible(laser_base_, true);
            restart(laser_base_);
        } else if (e == "laser_base_off") {
            emit(laser_base_, false);
            set_visible(laser_base_, false);
        } else if (e == "laser_hit_fire") {
            if (hit_fx_ && hit_bone_) set_global_position(hit_fx_, global_position(hit_bone_));
            set_visible(hit_fx_, true);
            restart(hit_fx_);
        } else if (e == "laser_hit_off") {
            emit(hit_fx_, false);
            set_visible(hit_fx_, false);
        }
    }

public:
    AmalgamVfx() : hit_fx_(NULL), death_(NULL), laser_base_(NULL), hit_bone_(NULL) {
        for (int i = 0; i < 3; ++i) hit_[i] = sparks_[i] = NULL, torch_[i] = NULL;
    }
};

/* NLaserVfx (the Cubex Construct's laser): a spine whose targeting bone
 * reaches out to the target (0.15 s), the beam reddening (0.2 s); drawn
 * back the same way and hidden. */
class LaserVfx : public ScriptBehavior {
public:
    LaserVfx() : self_(NULL), anim_(NULL), bone_(NULL), hide_at_(-1), t_(0) {}
    virtual void ready(Node* n) {
        self_ = n->is_node2d() ? (Node2D*)n : NULL;
        Node* a = n->get_node("SpineSprite");
        Node* b = n->get_node("SpineSprite/TargetingBone");
        anim_ = a && a->type == "SpineSprite" ? (SpineSprite*)a : NULL;
        bone_ = b && b->is_node2d() ? (Node2D*)b : NULL;
        if (anim_) anim_->set_animation("animation", true, 0);
        set_visible(anim_, false);
    }
    virtual void process(Node*, double dt) {
        t_ += dt;
        tweens_.update(dt);
        if (hide_at_ >= 0 && t_ >= hide_at_) {
            hide_at_ = -1;
            set_visible(anim_, false);
        }
    }
    void extend(const Vec2& target) {
        if (!anim_ || !bone_ || !self_) return;
        set_visible(anim_, true);
        anim_->set_animation("animation", true, 0);
        SpineScriptHelpers::set_global(bone_, self_->global_transform().origin);
        tweens_.kill(&bone_->position);
        tweens_.tween(&bone_->position, target, 0.15f, TRANS_EXPO, EASE_OUT);
        tweens_.tween(&anim_->modulate.g, 0.0f, 0.2f, TRANS_LINEAR, EASE_IN_OUT, 0.15f);
        tweens_.tween(&anim_->modulate.b, 0.0f, 0.2f, TRANS_LINEAR, EASE_IN_OUT, 0.15f);
        hide_at_ = -1;
    }
    void retract() {
        if (!bone_ || !self_) return;
        tweens_.kill(&bone_->position);
        tweens_.tween(&bone_->position, self_->position, 0.15f, TRANS_EXPO, EASE_IN);
        hide_at_ = t_ + 0.15;
    }
    void reset() {
        if (bone_ && self_) bone_->position = self_->position;
    }

private:
    Node2D* self_;
    SpineSprite* anim_;
    Node2D* bone_;
    double hide_at_, t_;
    TweenSet tweens_;
};

static const char* const LASER_SCRIPT = "res://src/Core/Nodes/Vfx/NLaserVfx.cs";

static LaserVfx* laser_of(Node* n) {
    return n && n->behavior && n->script == LASER_SCRIPT ? (LaserVfx*)n->behavior : NULL;
}

/* NCubexConstructVfx: the moss skin, the laser fired 3000 px to the left
 * with the rings springing out (0.4 s), drawn back. */
class CubexConstructVfx : public SpineScript {
    Node2D* laser_;
    Node2D* rings_;
    Vec2 rings_scale_;
    void on_ready() {
        laser_ = node2d("LaserBone/Laser");
        rings_ = node2d("LaserBone/Rings");
        if (rings_) rings_scale_ = rings_->scale;
        set_visible(rings_, false);
        if (laser_ && sprite_->scale.x != 0 && sprite_->scale.y != 0) laser_->scale = Vec2(1 / sprite_->scale.x, 1 / sprite_->scale.y);
        set_visible(laser_, false);
        if (laser_of(laser_)) laser_of(laser_)->reset();
        sprite_->set_skins(std::vector<std::string>(1, "moss3"));
        sprite_->set_animation("idle_loop", true, 0);
    }
    void on_event(const std::string& e) {
        if (e == "laser_start" && laser_) {
            set_visible(laser_, true);
            if (laser_of(laser_)) laser_of(laser_)->extend(Vec2(laser_->position.x - 3000.0f, sprite_->position.y));
            if (rings_) {
                tweens_.kill(&rings_->scale);
                tweens_.tween_from(&rings_->scale, Vec2(0, 0), rings_scale_, 0.4f, TRANS_EXPO, EASE_OUT);
                set_visible(rings_, true);
            }
        } else if (e == "laser_end") {
            if (laser_of(laser_)) laser_of(laser_)->retract();
            set_visible(rings_, false);
        }
    }

public:
    CubexConstructVfx() : laser_(NULL), rings_(NULL) {}
};

/* NCeremonialBeastVfx: the energy (on from the start), the death's
 * particles, the plow's targets held where they were. An IDeathDelayer:
 * the creature stays until its death particles have run out. */
class CeremonialBeastVfx : public SpineScript {
public:
    bool death_done() const { return death_started_ && (!death_ || death_->finished()); }

private:
    bool death_started_;
    Particles2D *death_, *front_, *back_;
    Node2D *plow_start_, *plow_end_;
    Vec2 start_at_, end_at_;
    static Node2D* n2(Node* n) { return n && n->is_node2d() ? (Node2D*)n : NULL; }
    void on_ready() {
        death_ = particles(exported("_deathParticles"));
        front_ = particles(exported("_energyParticlesFront"));
        back_ = particles(exported("_energyParticlesBack"));
        plow_start_ = n2(exported("_plowStartTarget"));
        plow_end_ = n2(exported("_plowEndTarget"));
        idle(death_, true);
        emit(back_, true);
        emit(front_, true);
        if (plow_start_) start_at_ = global_position(plow_start_);
        if (plow_end_) end_at_ = global_position(plow_end_);
    }
    void on_event(const std::string& e) {
        if (e == "turnOffEnergy" || e == "turnOnEnergy") {
            emit(front_, e == "turnOnEnergy");
            emit(back_, e == "turnOnEnergy");
        } else if (e == "deathParticles") {
            restart(death_);
            death_started_ = true;
        }
        else if (e == "plowStart" && plow_start_) set_global_position(plow_start_, start_at_);
        else if (e == "plowEnd" && plow_end_) set_global_position(plow_end_, end_at_);
    }

public:
    CeremonialBeastVfx() : death_started_(false), death_(NULL), front_(NULL), back_(NULL), plow_start_(NULL), plow_end_(NULL) {}
};

/* NDecimillipedeSegmentVfx: the sprays shrinking away once the suck
 * completes, the flakes thrown when it explodes; Regenerate (the
 * reattach's) brings the sprays back. */
class DecimillipedeSegmentVfx : public SpineScript {
    std::vector<Node*> damage_, sprays_;
    std::vector<Vec2> scales_;
    struct Restore {
        Node2D* node;
        Vec2 scale;
        double at;
    };
    std::vector<Restore> restore_;
    double t_;
    void on_ready() {
        damage_ = exported_list("_damageParticleNodes");
        sprays_ = exported_list("_sprayNodes");
        for (size_t i = 0; i < sprays_.size(); ++i) {
            set_visible(sprays_[i], false);
            scales_.push_back(sprays_[i]->is_node2d() ? ((Node2D*)sprays_[i])->scale : Vec2(1, 1));
        }
    }
    void on_event(const std::string& e) {
        if (e == "suck_complete") {
            /* EndRegenerate */
            for (size_t i = 0; i < sprays_.size(); ++i) {
                if (!sprays_[i]->is_node2d()) continue;
                Node2D* n = (Node2D*)sprays_[i];
                float d = chaotic(0.9f, 1.25f);
                Restore r = { n, n->scale, t_ + d };
                restore_.push_back(r);
                tweens_.kill(&n->scale);
                tweens_.tween(&n->scale, Vec2(0, 0), d, TRANS_QUAD, EASE_IN);
            }
        } else if (e == "explode") {
            /* Wither */
            for (size_t i = 0; i < damage_.size(); ++i) {
                Particles2D* p = particles(damage_[i]);
                if (!p) continue;
                p->gravity = Vec2(0, 300);
                p->speed_scale = 2;
                p->pmin[Particles2D::P_VELOCITY] = 400;
                p->pmax[Particles2D::P_VELOCITY] = 600;
                p->restart();
            }
        }
    }
    void tick(double dt) {
        t_ += dt;
        for (size_t i = restore_.size(); i-- > 0;)
            if (t_ >= restore_[i].at) {
                set_visible(restore_[i].node, false);
                restore_[i].node->scale = restore_[i].scale;
                restore_.erase(restore_.begin() + (long)i);
            }
    }

public:
    DecimillipedeSegmentVfx() : t_(0) {}
    void regenerate() {
        for (size_t i = 0; i < sprays_.size(); ++i) {
            if (!sprays_[i]->is_node2d()) continue;
            Node2D* n = (Node2D*)sprays_[i];
            set_visible(n, true);
            n->shader_params["direction"] = -1;
            float d = chaotic(0.0f, 0.5f);
            n->shader_params["opacity"] = 0.5f;
            tweens_.kill(&n->scale);
            tweens_.tween(&n->scale, scales_[i], d + 0.5f, TRANS_EXPO, EASE_OUT);
            tweens_.tween(&n->shader_params["opacity"], 0.9f, d, TRANS_EXPO, EASE_OUT);
        }
    }
};

/* NTestSubjectVfx: its neck squirt, dizzies, embers and flames; the burn
 * (the two burn sprites, the burn particles and three fires springing out
 * then shrinking away). */
class TestSubjectVfx : public SpineScript {
    Particles2D *neck_, *dizzy_, *ember_, *flame_, *burn_, *targeted_, *fountain_;
    SpineSprite *front_, *back_;
    Node* fire_[3];
    Vec2 fire_scale_[3];
    double clear_at_, t_;
    static SpineSprite* spine(Node* n) { return n && n->type == "SpineSprite" ? (SpineSprite*)n : NULL; }
    static Control* ctl(Node* n) { return n && n->is_control() ? (Control*)n : NULL; }
    void on_ready() {
        front_ = spine(node("FrontBurnVfxSlot/FrontBurnVfx"));
        back_ = spine(node("BackBurnVfxSlot/BackBurnVfx"));
        neck_ = particles(node("NeckParticlesSlot/NeckParticles"));
        dizzy_ = particles(node("NeckParticlesSlot/DizzyPaticles"));
        ember_ = particles(node("../../EmberParticles"));
        flame_ = particles(node("../../FlameParticles"));
        burn_ = particles(node("../../BurnParticleContainer/BurnParticles"));
        targeted_ = particles(node("../../BurnParticleContainer/TargetedBurnParticle"));
        fountain_ = particles(node("../../BurnParticleContainer/BurnParticleFountain"));
        static const char* const fires[3] = { "../../BurnFire1", "../../BurnFire2", "../../BurnFire3" };
        for (int i = 0; i < 3; ++i) {
            fire_[i] = node(fires[i]);
            set_visible(fire_[i], false);
            fire_scale_[i] = ctl(fire_[i]) ? ctl(fire_[i])->scale : Vec2(1, 1);
        }
        idle(neck_, true);
        emit(dizzy_, false);
        idle(ember_, true);
        emit(flame_, false);
        emit(burn_, false);
        emit(targeted_, false);
        emit(fountain_, false);
        sprite_->set_animation("idle_loop3", true, 0);
        if (front_) front_->set_animation("empty", true, 0);
        if (back_) back_->set_animation("empty", true, 0);
    }
    void on_event(const std::string& e) {
        if (e == "neck_explode") restart(neck_);
        else if (e == "start_dizzies") emit(dizzy_, true);
        else if (e == "end_dizzies") emit(dizzy_, false);
        else if (e == "start_embers") restart(ember_);
        else if (e == "start_flames") emit(flame_, true);
        else if (e == "end_flames") emit(flame_, false);
        else if (e == "start_burn_vfx") {
            if (front_) front_->set_animation("burn", false, 0);
            if (back_) back_->set_animation("burn", false, 0);
            restart(burn_);
            emit(targeted_, true);
            restart(fountain_);
            static const float delays[3] = { 0.2f, 0.2f, 0.3f };
            static const float out_delays[3] = { 1.2f, 1.1f, 1.0f };
            for (int i = 0; i < 3; ++i) {
                Control* f = ctl(fire_[i]);
                set_visible(fire_[i], true);
                if (!f) continue;
                tweens_.kill(&f->scale);
                f->scale = Vec2(0, 0);
                tweens_.tween(&f->scale, fire_scale_[i], 0.1f, TRANS_LINEAR, EASE_IN_OUT, delays[i]);
                /* TweenOutBurnFire, when the third fire is in (0.4 s) */
                tweens_.tween(&f->scale, Vec2(0.2f, 0), 0.8f, TRANS_QUAD, EASE_IN, 0.4f + out_delays[i]);
            }
            clear_at_ = t_ + 0.4 + 1.2 + 0.8;
        } else if (e == "end_burn_vfx") {
            emit(burn_, false);
            emit(targeted_, false);
            emit(fountain_, false);
        }
    }
    void tick(double dt) {
        t_ += dt;
        if (clear_at_ >= 0 && t_ >= clear_at_) {
            clear_at_ = -1;
            for (int i = 0; i < 3; ++i) set_visible(fire_[i], false);
        }
    }

public:
    TestSubjectVfx()
        : neck_(NULL), dizzy_(NULL), ember_(NULL), flame_(NULL), burn_(NULL), targeted_(NULL), fountain_(NULL), front_(NULL), back_(NULL),
          clear_at_(-1), t_(0) {
        for (int i = 0; i < 3; ++i) fire_[i] = NULL;
    }
};

/* NVineShamblerVinesVfx (its grasping vines): the front vines' animation
 * and dirt blasts, the back vines moved behind the creatures (the room's
 * BackCombatVfxContainer), both freed when the front's animation ends. */
class VineShamblerVines : public ScriptBehavior {
public:
    VineShamblerVines() : self_(NULL), front_(NULL), back_(NULL), moved_(false) {
        for (int i = 0; i < 4; ++i) dirt_[i] = NULL;
    }
    virtual void ready(Node* n) {
        self_ = n;
        Node* f = n->get_node("VinesFront");
        Node* b = n->get_node("VinesBackScene/VinesBack");
        front_ = f && f->type == "SpineSprite" ? (SpineSprite*)f : NULL;
        back_ = b && b->type == "SpineSprite" ? (SpineSprite*)b : NULL;
        static const char* const dirt[4] = { "DirtBlast1", "VinesBackScene/DirtBlast2", "DirtBlast3", "VinesBackScene/DirtBlast4" };
        for (int i = 0; i < 4; ++i) {
            Node* d = n->get_node(dirt[i]);
            dirt_[i] = d && (d->type == "GPUParticles2D" || d->type == "CPUParticles2D") ? (Particles2D*)d : NULL;
            if (dirt_[i]) {
                dirt_[i]->emitting = false;
                dirt_[i]->one_shot = true;
            }
        }
        if (front_) front_->set_animation("animation", true, 0);
        if (back_) back_->set_animation("animation", true, 0);
    }
    virtual void process(Node* n, double) {
        if (!front_) return;
        if (!moved_ && back_) {
            /* Reparent(BackCombatVfxContainer), keeping its place */
            moved_ = true;
            Node* root = n;
            while (root->parent) root = root->parent;
            Node* box = find_named(root, "BackCombatVfxContainer");
            if (box && box->is_canvas_item() && back_->parent) {
                Vec2 g = back_->global_transform().origin;
                back_->parent->remove_child(back_);
                box->add_child(back_);
                Xform2D bt = ((CanvasItem*)box)->global_transform();
                Vec2 d = g - bt.origin;
                float det = bt.x.x * bt.y.y - bt.y.x * bt.x.y;
                if (det != 0) back_->position = Vec2((d.x * bt.y.y - d.y * bt.y.x) / det, (bt.x.x * d.y - bt.x.y * d.x) / det);
            }
        }
        const std::vector<SpFired>& f = front_->fired();
        for (size_t i = 0; i < f.size(); ++i) {
            if (f[i].start) continue;
            if (f[i].name == "dirt_1" && dirt_[0]) dirt_[0]->restart();
            else if (f[i].name == "dirt_2" && dirt_[1]) dirt_[1]->restart();
            else if (f[i].name == "dirt_3" && dirt_[2]) dirt_[2]->restart();
            else if (f[i].name == "dirt_4" && dirt_[3]) dirt_[3]->restart();
        }
        /* animation_completed: the front's first pass over */
        SpAnimationState* st = front_->animation_state();
        const SpAnimation* a = st ? st->current(0) : NULL;
        if (a && st->track_time(0) >= a->duration) {
            if (back_ && moved_) back_->queue_free();
            n->queue_free();
            front_ = NULL;
        }
    }

private:
    Node* self_;
    SpineSprite* front_;
    SpineSprite* back_;
    Particles2D* dirt_[4];
    bool moved_;
};

static const char* const DECI_SEGMENT_SCRIPT = "res://src/Core/Nodes/Vfx/NDecimillipedeSegmentVfx.cs";

/* NDecimillipedeSegmentVfx.Regenerate on its node (false when it is not one). */
bool decimillipede_segment_regenerate(Node* n) {
    if (!n || !n->behavior || n->script != DECI_SEGMENT_SCRIPT) return false;
    ((DecimillipedeSegmentVfx*)n->behavior)->regenerate();
    return true;
}


/* NDecimillipedeSegmentDriver: a segment's link swaying on Perlin noise
 * (the left one faster and less far), AttackShake jerking it 100 px left,
 * then right, then back (0.4 / 0.1 / 0.75 s, Sine InOut). */
class SegmentDriver : public ScriptBehavior {
public:
    SegmentDriver() : self_(NULL), left_(false), speed_(0), magnitude_(0), time_(0) {}
    virtual void ready(Node* n) {
        self_ = n->is_node2d() ? (Node2D*)n : NULL;
        if (!self_) return;
        const Json* l = n->get_extra("_leftSegment");
        left_ = l && l->b();
        origin_ = self_->position;
        speed_ = left_ ? 0.1f : 0.05f;
        magnitude_ = left_ ? 250.0f : 300.0f;
    }
    virtual void process(Node*, double dt) {
        if (!self_) return;
        tweens_.update(dt);
        time_ += (float)dt;
        float x = time_ * speed_ + (left_ ? 0.25f : 0.0f);
        Vec2 v(fbm(x, 1.0f, 0), fbm(x + 0.25f, 1.0f, 0));
        self_->position = origin_ + v * magnitude_ + strike_;
    }
    void attack_shake() {
        tweens_.kill(&strike_);
        tweens_.tween(&strike_, Vec2(-100, 0), 0.4f, TRANS_SINE, EASE_IN_OUT);
        tweens_.tween(&strike_, Vec2(100, 0), 0.1f, TRANS_SINE, EASE_IN_OUT, 0.4f);
        tweens_.tween(&strike_, Vec2(0, 0), 0.75f, TRANS_SINE, EASE_IN_OUT, 0.5f);
    }

private:
    Node2D* self_;
    bool left_;
    float speed_, magnitude_, time_;
    Vec2 origin_, strike_;
    TweenSet tweens_;
};

static const char* const SEGMENT_DRIVER_SCRIPT = "res://src/Core/Nodes/Animation/NDecimillipedeSegmentDriver.cs";

bool death_delayed(Node* n) {
    if (!n) return false;
    if (n->behavior && n->script == "res://src/Core/Nodes/Vfx/NCeremonialBeastVfx.cs" && !((CeremonialBeastVfx*)n->behavior)->death_done())
        return true;
    for (size_t i = 0; i < n->children.size(); ++i)
        if (death_delayed(n->children[i])) return true;
    return false;
}

bool special_node_call(const std::string& call, const Json& args, VfxAnchors& anchors) {
    const std::string& name = args[(size_t)0].str();
    size_t hash = name.find('#');
    /* "who#path": a creature before it (a colour, "#ffffff80", is none) */
    if (hash == std::string::npos || hash == 0) return false;
    std::string who = name.substr(0, hash), path = name.substr(hash + 1);
    Node* visuals = anchors.visuals_of(who);
    if (path.compare(0, 5, "form/") == 0) {
        /* the power's form (StandIns' "who#form/NDemonFormVfx") */
        form_vfx_call(visuals, path.substr(5), call, args);
        return true;
    }
    Node* n = NULL;
    if (visuals && path.compare(0, 5, "body:") == 0) {
        /* NCreatureVisuals.GetCurrentBody: the phobia body when shown */
        Node* body = visuals->find_unique("PhobiaModeVisuals");
        if (!body || !body->is_canvas_item() || !((CanvasItem*)body)->visible) body = visuals->find_unique("Visuals");
        n = body ? body->get_node(path.substr(5)) : NULL;
    } else if (visuals) {
        n = visuals->get_node(path);
    }
    if (call == "NKinPriestBeamVfx.Fire") kin_priest_beam_fire(n);
    else if (call == "NDecimillipedeSegmentVfx.Regenerate") decimillipede_segment_regenerate(n);
    else if (call == "NDecimillipedeSegmentDriver.AttackShake") {
        if (n && n->behavior && n->script == SEGMENT_DRIVER_SCRIPT) ((SegmentDriver*)n->behavior)->attack_shake();
    } else if (call == "NMyteVfx.SetTarget" || call == "NSnappingJaxfruitVfx.SetTarget") {
        Vec2 at;
        bool has = args.size() > 1 && args[1].is_string() && anchors.origin_of(args[1].str(), &at);
        if (n && n->behavior && (n->script == "res://src/Core/Nodes/Vfx/NMyteVfx.cs" || n->script == "res://src/Core/Nodes/Vfx/NSnappingJaxfruitVfx.cs"))
            ((TableVfx*)n->behavior)->set_target(has, at);
    } else if (call == "NFlyconidSporesVfx.SetSporeTypeIsVulnerable") {
        bool vulnerable = args.size() > 1 && args[1].b();
        if (n) {
            set_visible(n->get_node("FrailSpores"), !vulnerable);
            set_visible(n->get_node("VulnerableSpores"), vulnerable);
        }
    }
    return true;
}

template <class T> static ScriptBehavior* make(Node*) { return new T(); }

void install_creature_vfx_scripts() {
    register_script("res://src/Core/Nodes/Vfx/NIroncladVfx.cs", make<IroncladVfx>);
    register_script("res://src/Core/Nodes/Vfx/NDefectVfx.cs", make<DefectVfx>);
    register_script("res://src/Core/Nodes/Vfx/NNecrobinderVfx.cs", make<NecrobinderVfx>);
    register_script("res://src/Core/Nodes/Vfx/NRegentVfx.cs", make<RegentVfx>);
    register_script(KIN_BEAM_SCRIPT, make<KinPriestBeam>);
    RegisterTables<sizeof(VFX_TABLES) / sizeof(VFX_TABLES[0])>::run();
    register_script("res://src/Core/Nodes/Vfx/NAxebotVfx.cs", make<AxebotVfx>);
    register_script("res://src/Core/Nodes/Vfx/NKaiserCrabBossVfx.cs", make<KaiserCrabBossVfx>);
    register_script("res://src/Core/Nodes/Vfx/NSoulFyshVfx.cs", make<SoulFyshVfx>);
    register_script("res://src/Core/Nodes/Vfx/NVantomVfx.cs", make<VantomVfx>);
    register_script("res://src/Core/Nodes/Vfx/NSewerClamVfx.cs", make<SewerClamVfx>);
    register_script("res://src/Core/Nodes/Vfx/NOilSpillVfx.cs", make<OilSpillVfx>);
    register_script("res://src/Core/Nodes/Vfx/NLivingGasVfx.cs", make<LivingGasVfx>);
    register_script("res://src/Core/Nodes/Vfx/NEntomancerVfx.cs", make<EntomancerVfx>);
    register_script("res://src/Core/Nodes/Vfx/NWaterfallGiantVfx.cs", make<WaterfallGiantVfx>);
    register_script("res://src/Core/Nodes/Vfx/NAeonGlassVfx.cs", make<AeonGlassVfx>);
    register_script("res://src/Core/Nodes/Vfx/NAmalgamVfx.cs", make<AmalgamVfx>);
    register_script(LASER_SCRIPT, make<LaserVfx>);
    register_script("res://src/Core/Nodes/Vfx/NCubexConstructVfx.cs", make<CubexConstructVfx>);
    register_script("res://src/Core/Nodes/Vfx/NCeremonialBeastVfx.cs", make<CeremonialBeastVfx>);
    register_script(DECI_SEGMENT_SCRIPT, make<DecimillipedeSegmentVfx>);
    register_script(SEGMENT_DRIVER_SCRIPT, make<SegmentDriver>);
    register_script("res://src/Core/Nodes/Vfx/NTestSubjectVfx.cs", make<TestSubjectVfx>);
    register_script("res://src/Core/Nodes/Vfx/NVineShamblerVinesVfx.cs", make<VineShamblerVines>);
}

} /* namespace spire */

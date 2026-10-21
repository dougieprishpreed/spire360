#include "form_vfx.h"
#include "combat_vfx.h"
#include "ui_util.h"

#include "../scene/node.h"
#include "../scene/particles.h"
#include "../scene/scene.h"
#include "../scene/script.h"
#include "../scene/spine_sprite.h"
#include "../scene/value.h"
#include "../spine/spine.h"

#include <map>

namespace spire {

namespace {

const char* const FORMS_DIR = "res://src/Core/Nodes/Vfx/Forms/";
const char* const FOLLOWER_SCRIPT = "res://src/Core/Nodes/Vfx/Utilities/NSpineSpriteBoneFollower.cs";
const char* const RAMP_SCRIPT = "res://src/Core/Nodes/Vfx/Utilities/NValueRamp.cs";
const char* const COPIER_SCRIPT = "res://src/Core/Nodes/Vfx/Utilities/NSpineSpriteCopier.cs";

enum Kind { DEMON, VOID_FORM, SERPENT, REAPER, ECHO };

float extra_float(Node* n, const char* key, float fallback) {
    const Json* v = n->get_extra(key);
    return v ? jv_float(*v, fallback) : fallback;
}

SpineSprite* find_spine(Node* n) {
    if (!n) return NULL;
    if (n->type == "SpineSprite") return (SpineSprite*)n;
    for (size_t i = 0; i < n->children.size(); ++i)
        if (SpineSprite* s = find_spine(n->children[i])) return s;
    return NULL;
}

/* NSpineSpriteBoneFollower: at the bone (snapped, or a fixed fraction of
 * the way each frame). */
class BoneFollower : public ScriptBehavior {
public:
    explicit BoneFollower(Node* n)
        : target(NULL), snap_(false), speed_(extra_float(n, "_interpolationSpeed", 0.5f)) {
        const Json* s = n->get_extra("_snap");
        snap_ = s && s->b();
        const Json* b = n->get_extra("_boneName");
        if (b && b->is_string()) bone = b->str();
    }
    SpineSprite* target;
    std::string bone;
    virtual void process(Node* n, double) {
        Vec2 local;
        if (!target || bone.empty() || !n->is_node2d() || !target->bone_position(bone, &local)) return;
        Node2D* self = (Node2D*)n;
        Vec2 g = target->global_transform().xform(local);
        Vec2 at = self->global_transform().origin;
        if (!snap_) g = at + (g - at) * speed_;
        Xform2D parent = n->parent && n->parent->is_canvas_item() ? ((CanvasItem*)n->parent)->global_transform() : Xform2D();
        self->position = inverse(parent).xform(g);
    }

private:
    bool snap_;
    float speed_;
};

/* NValueRamp: 0..1 at _rampSpeed a second up or down, through _rampCurve;
 * TryProcess answers while it moves (or just after a forced value). */
class ValueRamp : public ScriptBehavior {
public:
    explicit ValueRamp(Node* n) : speed_(extra_float(n, "_rampSpeed", 1)), value_(0), up_(false), forced_(true) {
        const Json* c = n->get_extra("_rampCurve");
        Resource* r = c ? script_resource(*c) : NULL;
        if (r) CurveData::from(r, curve_);
    }
    bool try_process(double dt, float* out) {
        bool forced = forced_;
        forced_ = false;
        float step = (float)dt * speed_ * (up_ ? 1.0f : -1.0f);
        bool at_end = step > 0 ? value_ >= 1 : (!(step < 0) || value_ <= 0);
        if (!forced && at_end) return false;
        value_ = value_ + step < 0 ? 0 : value_ + step > 1 ? 1 : value_ + step;
        *out = curve_.valid() ? curve_.sample(value_) : value_;
        return true;
    }
    void set_increasing(bool up) { up_ = up; }
    void force(float v) {
        value_ = v;
        forced_ = true;
    }

private:
    float speed_;
    CurveData curve_;
    float value_;
    bool up_, forced_;
};

class Form : public ScriptBehavior {
public:
    Form(Node* n, Kind kind)
        : kind_(kind), follower_(NULL), ramp_(NULL), glow_(NULL), lines_(NULL), copier_(NULL),
          body_sprite_(NULL), swords_min_(0.7f), swords_max_(1) {
        static const char* const KEYS[] = { "_ironcladBoneName", "_silentBoneName", "_regentBoneName",
                                            "_necrobinderBoneName", "_defectBoneName" };
        static const char* const NAMES[] = { "ironclad", "silent", "regent", "necrobinder", "defect" };
        for (int i = 0; i < 5; ++i) {
            const Json* b = n->get_extra(KEYS[i]);
            if (b && b->is_string()) bones_[NAMES[i]] = b->str();
        }
        gradient(n, kind == SERPENT ? "_snakesModulateGradient" : "_glowSelfModulateGradient", glow_gradient_);
        gradient(n, "_echoFormLinesSelfModulateGradient", lines_gradient_);
        const Json* r = n->get_extra("_swordsScaleRange");
        Vec2 range;
        if (r && jv_vec2(*r, range)) {
            swords_min_ = range.x;
            swords_max_ = range.y;
        }
    }

    virtual void ready(Node* n) {
        Node* f = vfx_exported(n, "_boneFollower");
        follower_ = f && f->script == FOLLOWER_SCRIPT && f->behavior ? (BoneFollower*)f->behavior : NULL;
        Node* r = vfx_exported(n, "_valueRamp");
        ramp_ = r && r->script == RAMP_SCRIPT && r->behavior ? (ValueRamp*)r->behavior : NULL;
        Node* g = vfx_exported(n, kind_ == SERPENT ? "_snakesContainer" : "_glow");
        glow_ = g && g->is_canvas_item() ? (CanvasItem*)g : NULL;
        Node* l = vfx_exported(n, "_echoLines");
        lines_ = l && l->is_canvas_item() ? (CanvasItem*)l : NULL;
        Node* c = vfx_exported(n, "_spineCopier");
        copier_ = c && c->type == "SpineSprite" ? (SpineSprite*)c : NULL;
        std::vector<Node*> swords;
        vfx_exported_all(n, "_swords", swords);
        for (size_t i = 0; i < swords.size(); ++i)
            if (swords[i]->is_node2d()) swords_.push_back((Node2D*)swords[i]);
    }

    /* NFormVfx.Initialize: the character's body to follow (and copy) */
    void initialize(Node* n, SceneLibrary* lib, const std::string& character, Node* visuals) {
        body_sprite_ = find_spine(visuals);
        if (follower_ && body_sprite_) {
            follower_->target = body_sprite_;
            std::map<std::string, std::string>::const_iterator b = bones_.find(character);
            follower_->bone = b != bones_.end() ? b->second : std::string();
        }
        if (copier_ && body_sprite_ && body_sprite_->data_res) {
            /* NSpineSpriteCopier.Initialize: the body's place, size and data */
            copier_->position = body_sprite_->position;
            copier_->scale = body_sprite_->scale;
            copier_->set_data(lib, body_sprite_->data_res);
            copy_animation();
        }
        if (kind_ == VOID_FORM && ramp_) {
            ramp_->set_increasing(true);
            ramp_->force(1);
            update(1);
        } else if (kind_ == SERPENT && ramp_) {
            ramp_->set_increasing(false);
            ramp_->force(0);
            update(0);
        } else if (kind_ == ECHO) {
            set_active(n, false); /* NEchoFormVfx.Create */
        }
    }

    void set_active(Node* n, bool on) {
        if (kind_ == VOID_FORM) vfx_emitting(vfx_exported(n, "_powerActiveParticles"), on);
        if (kind_ == ECHO) vfx_emitting(vfx_exported(n, "_speckParticles"), on);
        if ((kind_ == VOID_FORM || kind_ == ECHO) && ramp_) ramp_->set_increasing(on);
    }

    void triggered(Node* n) {
        if (kind_ == DEMON || kind_ == REAPER || kind_ == SERPENT) vfx_restart(vfx_exported(n, "_effectTriggeredParticles"));
        if (kind_ == SERPENT && ramp_) ramp_->force(1);
    }

    virtual void process(Node*, double dt) {
        float p;
        if (ramp_ && kind_ != DEMON && kind_ != REAPER && ramp_->try_process(dt, &p)) update(p);
        if (copier_ && body_sprite_)
            for (size_t i = 0; i < body_sprite_->fired().size(); ++i)
                if (body_sprite_->fired()[i].start && body_sprite_->fired()[i].track == 0) copy_animation();
    }

private:
    static void gradient(Node* n, const char* key, GradientData& out) {
        const Json* v = n->get_extra(key);
        Resource* r = v ? script_resource(*v) : NULL;
        if (r) GradientData::from(r, out);
    }
    /* OnAnimationStart: the body's track 0 animation */
    void copy_animation() {
        SpAnimationState* s = body_sprite_->animation_state();
        const SpAnimation* a = s ? s->current(0) : NULL;
        if (a) copier_->set_animation(a->name, s->current_loop(0), 0);
    }
    void update(float p) {
        if (kind_ == SERPENT) {
            if (glow_ && glow_gradient_.valid()) glow_->modulate = glow_gradient_.at(p);
            return;
        }
        if (glow_ && glow_gradient_.valid()) glow_->self_modulate = glow_gradient_.at(p);
        if (kind_ == ECHO && lines_ && lines_gradient_.valid()) lines_->self_modulate = lines_gradient_.at(p);
        if (kind_ == VOID_FORM)
            for (size_t i = 0; i < swords_.size(); ++i) {
                float s = swords_min_ + (swords_max_ - swords_min_) * p;
                swords_[i]->scale = Vec2(s, s);
            }
    }

    Kind kind_;
    std::map<std::string, std::string> bones_;
    BoneFollower* follower_;
    ValueRamp* ramp_;
    CanvasItem* glow_; /* the Serpent's: the snakes' container */
    CanvasItem* lines_;
    SpineSprite* copier_;
    SpineSprite* body_sprite_;
    GradientData glow_gradient_, lines_gradient_;
    std::vector<Node2D*> swords_;
    float swords_min_, swords_max_;
};

const char* const FORM_NAMES[] = { "NDemonFormVfx", "NVoidFormVfx", "NSerpentFormVfx", "NReaperFormVfx", "NEchoFormVfx" };
const char* const FORM_SCENES[] = { "demon", "void", "serpent", "reaper", "echo" };

ScriptBehavior* make_demon(Node* n) { return new Form(n, DEMON); }
ScriptBehavior* make_void(Node* n) { return new Form(n, VOID_FORM); }
ScriptBehavior* make_serpent(Node* n) { return new Form(n, SERPENT); }
ScriptBehavior* make_reaper(Node* n) { return new Form(n, REAPER); }
ScriptBehavior* make_echo(Node* n) { return new Form(n, ECHO); }
ScriptBehavior* make_follower(Node* n) { return new BoneFollower(n); }
ScriptBehavior* make_ramp(Node* n) { return new ValueRamp(n); }
/* NSpineSpriteCopier: the form drives it (Form::copy_animation) */
ScriptBehavior* make_copier(Node*) { return new ScriptBehavior(); }

Node* holder_of(Node* visuals) { return visuals ? visuals->find_unique("FormVfx") : NULL; }

} /* namespace */

bool form_vfx_create(SceneLibrary* lib, const std::string& call, Node* visuals, const std::string& character) {
    int kind = -1;
    for (int i = 0; i < 5; ++i)
        if (call == std::string(FORM_NAMES[i]) + ".Create") kind = i;
    if (kind < 0) return false;
    Node* holder = holder_of(visuals);
    if (!holder) return true; /* "This creature has no form holder" */
    /* NCreatureVisuals.AddFormVfx: the holder's form replaced, at (0, 0) */
    for (size_t i = 0; i < holder->children.size(); ++i)
        if (!holder->children[i]->queued_for_deletion()) holder->children[i]->queue_free();
    Node* fx = instance(lib, format("scenes/vfx/forms/%s/vfx_%s_form_idle_vfx.tscn", FORM_SCENES[kind], FORM_SCENES[kind]));
    if (!fx || !fx->behavior || fx->script != std::string(FORMS_DIR) + FORM_NAMES[kind] + ".cs") {
        delete fx;
        return true;
    }
    holder->add_child(fx);
    if (fx->is_node2d()) ((Node2D*)fx)->position = Vec2(0, 0);
    ((Form*)fx->behavior)->initialize(fx, lib, lower(character), visuals);
    return true;
}

void form_vfx_call(Node* visuals, const std::string& type, const std::string& call, const Json& args) {
    Node* holder = holder_of(visuals);
    Node* fx = NULL;
    for (size_t i = 0; holder && i < holder->children.size(); ++i) {
        Node* c = holder->children[i];
        if (!c->queued_for_deletion() && c->behavior && c->script == std::string(FORMS_DIR) + type + ".cs") fx = c;
    }
    if (!fx) return;
    if (call == "NFormVfx.SetActive" || call == type + ".SetActive")
        ((Form*)fx->behavior)->set_active(fx, args.size() > 1 && args[1].b());
    else if (call == "NFormVfx.OnEffectTriggered" || call == type + ".OnEffectTriggered")
        ((Form*)fx->behavior)->triggered(fx);
}

void install_form_vfx_scripts() {
    static const ScriptFactory MAKE[] = { make_demon, make_void, make_serpent, make_reaper, make_echo };
    static std::string paths[5];
    for (int i = 0; i < 5; ++i) {
        paths[i] = std::string(FORMS_DIR) + FORM_NAMES[i] + ".cs";
        register_script(paths[i].c_str(), MAKE[i]);
    }
    register_script(FOLLOWER_SCRIPT, make_follower);
    register_script(RAMP_SCRIPT, make_ramp);
    register_script(COPIER_SCRIPT, make_copier);
}

} /* namespace spire */

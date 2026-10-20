#include "combat_vfx.h"

#include "../scene/controls.h"
#include "../scene/node.h"
#include "../scene/particles.h"
#include "../scene/scene.h"
#include "../scene/script.h"
#include "../scene/tween.h"
#include "../scene/value.h"
#include "loc.h"
#include "screen_shake.h"
#include "ui_util.h"

#include <ctype.h>
#include <math.h>
#include <string.h>

namespace spire {

namespace {

const float PI_F = 3.14159265f;

float rand_range(float lo, float hi) { return lo + (hi - lo) * (float)(chaotic_rand() % 1000001) / 1000000.0f; }

Color html(unsigned rgb) {
    return Color((float)((rgb >> 16) & 255) / 255.0f, (float)((rgb >> 8) & 255) / 255.0f, (float)(rgb & 255) / 255.0f, 1);
}

/* "#rrggbbaa" (Steps.Ui's colours) */
bool parse_color(const std::string& s, Color* out) {
    if (s.size() != 9 || s[0] != '#') return false;
    unsigned v[4];
    for (int i = 0; i < 4; ++i) {
        unsigned x = 0;
        for (int k = 0; k < 2; ++k) {
            char c = s[(size_t)(1 + i * 2 + k)];
            x = x * 16 + (unsigned)(isdigit((unsigned char)c) ? c - '0' : (tolower((unsigned char)c) - 'a' + 10));
        }
        v[i] = x;
    }
    *out = Color(v[0] / 255.0f, v[1] / 255.0f, v[2] / 255.0f, v[3] / 255.0f);
    return true;
}

bool is_creature_id(const std::string& s) {
    if (s == "player" || s == "osty") return true;
    return s.size() > 1 && s[0] == 'e' && isdigit((unsigned char)s[1]);
}

/* A cue's arguments by kind, in order. */
struct Args {
    std::vector<std::string> creatures, words;
    std::vector<Color> colors;
    std::vector<bool> bools;
    std::vector<float> numbers;
    explicit Args(const Json& a) {
        for (size_t i = 0; i < a.size(); ++i) {
            const Json& v = a[i];
            if (v.is_bool()) bools.push_back(v.b());
            else if (v.is_number()) numbers.push_back((float)v.num());
            else if (v.is_string()) {
                Color c;
                if (parse_color(v.str(), &c)) colors.push_back(c);
                else if (is_creature_id(v.str())) creatures.push_back(v.str());
                else words.push_back(v.str());
            }
        }
    }
    bool has_word(const char* w) const {
        for (size_t i = 0; i < words.size(); ++i)
            if (words[i] == w) return true;
        return false;
    }
};

Vec2 global_of(Node* n) {
    return n && n->is_canvas_item() ? ((CanvasItem*)n)->global_transform().origin : Vec2();
}

Vec2 to_local(Node* parent, const Vec2& g) {
    if (!parent || !parent->is_canvas_item()) return g;
    Xform2D xf = ((CanvasItem*)parent)->global_transform();
    Vec2 v = g - xf.origin;
    float det = xf.x.x * xf.y.y - xf.x.y * xf.y.x;
    if (fabsf(det) < 0.000001f) return Vec2();
    return Vec2((v.x * xf.y.y - v.y * xf.y.x) / det, (v.y * xf.x.x - v.x * xf.x.y) / det);
}

/* Node.GlobalPosition = g */
void set_global(Node* n, const Vec2& g) {
    if (!n) return;
    Vec2 local = to_local(n->parent, g);
    if (n->is_node2d()) ((Node2D*)n)->position = local;
    else if (n->is_control()) ((Control*)n)->set_position(local);
}

bool is_particles(Node* n) { return n && (n->type == "GPUParticles2D" || n->type == "CPUParticles2D"); }

/* The nodes an exported NodePath or NodePath array names. */
void exported(Node* root, const char* key, std::vector<Node*>& out) {
    const Json* v = root ? root->get_extra(key) : NULL;
    if (!v) return;
    if (v->is_array()) {
        for (size_t i = 0; i < v->size(); ++i) {
            Node* n = root->get_node(jv_nodepath((*v)[i]));
            if (n) out.push_back(n);
        }
    } else {
        Node* n = root->get_node(jv_nodepath(*v));
        if (n) out.push_back(n);
    }
}

Node* exported_one(Node* root, const char* key) {
    std::vector<Node*> v;
    exported(root, key, v);
    return v.empty() ? NULL : v[0];
}

/* GpuParticles2D.Restart, or NParticlesContainer.Restart (its _particles). */
void restart(Node* n) {
    if (!n) return;
    if (is_particles(n)) {
        ((Particles2D*)n)->restart();
        return;
    }
    std::vector<Node*> list;
    exported(n, "_particles", list);
    for (size_t i = 0; i < list.size(); ++i)
        if (is_particles(list[i])) ((Particles2D*)list[i])->restart();
}

/* An export list ("_name") or comma-separated node paths ("Ember,Clouds"). */
void nodes_of(Node* root, const char* spec, std::vector<Node*>& out) {
    if (!spec || !*spec) return;
    if (spec[0] == '_') {
        exported(root, spec, out);
        return;
    }
    std::string s = spec;
    size_t at = 0;
    while (at <= s.size()) {
        size_t comma = s.find(',', at);
        std::string path = s.substr(at, comma == std::string::npos ? std::string::npos : comma - at);
        Node* n = path == "." ? root : root->get_node(path);
        if (n) out.push_back(n);
        if (comma == std::string::npos) break;
        at = comma + 1;
    }
}

/* ---- the common shape: place, tint, restart particle lists on a
 * timetable, free ---- */

enum Anchor { AT_CENTER, AT_BOTTOM, AT_TOP, AT_TARGET_CENTER /* the second creature's */ };
enum TintMode { TINT_NONE, TINT_MODULATE, TINT_SELF, TINT_MATERIAL };
enum FlipMode { FLIP_NONE, FLIP_RANDOM, FLIP_BOOL, FLIP_BOOL_INVERTED, FLIP_ENEMY };
enum StepOp { OP_NONE, OP_RESTART, OP_EMIT, OP_STOP, OP_SHOW, OP_HIDE, OP_SHAKE /* nodes: "Strength/Duration" */ };

struct Step {
    float at;
    StepOp op;
    const char* nodes;
};

struct Spec {
    const char* name;  /* the class: "NPoisonImpactVfx" */
    const char* scene; /* scenes/... */
    const char* script;
    Anchor anchor;
    TintMode tint;
    const char* tint_nodes; /* TINT_SELF / TINT_MATERIAL: an export list */
    unsigned default_tint;  /* 0xRRGGBB when the call gives none; 0: none */
    FlipMode flip;
    const char* flip_node; /* export of the node flipped (NULL: the effect) */
    bool scale_arg;        /* the first number scales it */
    bool aim;              /* rotated from the first creature toward the second */
    Step steps[5];
    float free_at; /* < 0: when the effect's own particles finish */
};

/* MegaCrit.Sts2.Core.Nodes.Vfx: each Create overload taking creatures, and
 * PlaySequence's timetable. */
const Spec SPECS[] = {
    { "NGaseousImpactVfx", "scenes/vfx/vfx_gaseous_impact.tscn", "NGaseousImpactVfx.cs", AT_CENTER, TINT_MODULATE, NULL, 0,
      FLIP_NONE, NULL, false, false, { { 0, OP_RESTART, "_impactParticles" } }, 2.0f },
    { "NWormyImpactVfx", "scenes/vfx/vfx_wormy_impact.tscn", "NWormyImpactVfx.cs", AT_BOTTOM, TINT_NONE, NULL, 0,
      FLIP_NONE, NULL, false, false, { { 0, OP_RESTART, "_particles" } }, 2.0f },
    { "NFireBurstVfx", "scenes/vfx/vfx_fire_burst.tscn", "NFireBurstVfx.cs", AT_BOTTOM, TINT_SELF, "_modulateParticles",
      0xff8b57, FLIP_RANDOM, NULL, true, false, { { 0, OP_RESTART, "_particles" }, { 0, OP_SHAKE, "Weak/Normal" } }, 2.0f },
    { "NScratchVfx", "scenes/vfx/vfx_scratch_impact.tscn", "NScratchVfx.cs", AT_CENTER, TINT_NONE, NULL, 0,
      FLIP_BOOL_INVERTED, NULL, false, false,
      { { 0, OP_RESTART, "_anticipationParticles" }, { 0.1f, OP_RESTART, "_impactParticles" },
        { 0.1f, OP_SHAKE, "VeryWeak/Short" } }, 2.1f },
    { "NFireSmokePuffVfx", "scenes/vfx/vfx_fire_smoke_puff.tscn", "NFireSmokePuffVfx.cs", AT_CENTER, TINT_NONE, NULL,
      0, FLIP_NONE, NULL, false, false, { { 0, OP_EMIT, "Ember,Clouds" } }, 2.5f },
    { "NSmokePuffVfx", "scenes/vfx/vfx_smoke_puff.tscn", "NSmokePuffVfx.cs", AT_CENTER, TINT_NONE, NULL, 0, FLIP_NONE,
      NULL, false, false, { { 0, OP_EMIT, "Ember,Clouds" } }, 2.5f },
    { "NPoisonImpactVfx", "scenes/vfx/vfx_poison_impact.tscn", "NPoisonImpactVfx.cs", AT_CENTER, TINT_NONE, NULL, 0,
      FLIP_RANDOM, "_horizontalSmokeContainer", false, false, { { 0, OP_RESTART, "_impactParticles" } }, 2.0f },
    { "NGoopyImpactVfx", "scenes/vfx/vfx_goopy_impact.tscn", "NGoopyImpactVfx.cs", AT_CENTER, TINT_MODULATE, NULL,
      0x00ff00, FLIP_NONE, NULL, false, false, { { 0, OP_RESTART, "_impactParticles" } }, 3.5f },
    { "NSporeImpactVfx", "scenes/vfx/vfx_spore_impact.tscn", "NSporeImpactVfx.cs", AT_BOTTOM, TINT_MODULATE, NULL, 0,
      FLIP_NONE, NULL, false, false, { { 0, OP_RESTART, "_impactParticles" } }, 3.5f },
    { "NLineBurstVfx", "scenes/vfx/vfx_line_burst.tscn", "NLineBurstVfx.cs", AT_CENTER, TINT_NONE, NULL, 0, FLIP_NONE,
      NULL, false, false, { { 0, OP_EMIT, "." } }, -1.0f },
    { "NFireBurningVfx", "scenes/vfx/vfx_fire_burning.tscn", "NFireBurningVfx.cs", AT_BOTTOM, TINT_MODULATE, NULL,
      0xff8b57, FLIP_BOOL, NULL, true, false,
      { { 0, OP_RESTART, "_startParticles" }, { 0.3f, OP_RESTART, "_endParticles" } }, 2.3f },
    { "NDaggerSprayImpactVfx", "scenes/vfx/vfx_dagger_spray_impact.tscn", "NDaggerSprayImpactVfx.cs", AT_CENTER,
      TINT_SELF, "_modulateParticles", 0, FLIP_BOOL, NULL, false, false,
      { { 0.1f, OP_SHAKE, "Weak/Short" }, { 0.1f, OP_RESTART, "_particles" } }, 2.1f },
    { "NDaggerSprayFlurryVfx", "scenes/vfx/vfx_dagger_spray_flurry.tscn", "NDaggerSprayFlurryVfx.cs", AT_CENTER,
      TINT_MATERIAL, "_modulateParticles", 0, FLIP_BOOL, NULL, false, false, { { 0, OP_RESTART, "_particles" } }, 2.0f },
    { "NShivThrowVfx", "scenes/vfx/vfx_shiv_throw.tscn", "NShivThrowVfx.cs", AT_TARGET_CENTER, TINT_MATERIAL,
      "_modulateParticles", 0, FLIP_NONE, NULL, false, true,
      { { 0, OP_RESTART, "_throwParticles" }, { 0.15f, OP_RESTART, "_impactParticles" },
        { 0.15f, OP_SHAKE, "Weak/Short" } }, 2.15f },
    { "NHyperbeamImpactVfx", "scenes/vfx/vfx_hyperbeam_impact.tscn", "NHyperbeamImpactVfx.cs", AT_TARGET_CENTER,
      TINT_NONE, NULL, 0, FLIP_NONE, NULL, false, true,
      { { 0, OP_SHOW, "_impactStartParticles" }, { 0, OP_RESTART, "_impactStartParticles" },
        { 0.5f, OP_HIDE, "_impactStartParticles" }, { 0.5f, OP_RESTART, "_impactEndParticles" } }, 2.5f },
    { "NSweepingBeamImpactVfx", "scenes/vfx/vfx_sweeping_beam_impact.tscn", "NSweepingBeamImpactVfx.cs", AT_CENTER,
      TINT_NONE, NULL, 0, FLIP_NONE, NULL, false, false, { { 0, OP_RESTART, "_impactParticles" } }, 2.0f },
    { "NGrandFinaleImpactVfx", "scenes/vfx/vfx_grand_finale_impact.tscn", "NGrandFinaleImpactVfx.cs", AT_CENTER,
      TINT_NONE, NULL, 0, FLIP_NONE, NULL, false, false,
      { { 0, OP_RESTART, "_centerParticles" }, { 0, OP_RESTART, "_groundParticles" } }, 2.0f },
};

const Spec* spec_named(const std::string& name) {
    for (size_t i = 0; i < sizeof(SPECS) / sizeof(SPECS[0]); ++i)
        if (name == SPECS[i].name) return &SPECS[i];
    return NULL;
}

const Spec* spec_for_script(const std::string& res) {
    for (size_t i = 0; i < sizeof(SPECS) / sizeof(SPECS[0]); ++i)
        if (res == std::string("res://src/Core/Nodes/Vfx/") + SPECS[i].script) return &SPECS[i];
    return NULL;
}

/* Every effect script waits for its Create's setup (start), as Godot runs
 * _Ready once the effect is added; one instanced another way starts on its
 * first frame. */
class Effect : public ScriptBehavior {
public:
    Effect() : started_(false), t_(0) {}
    virtual void process(Node* n, double dt) {
        if (!started_) start(n);
        tweens_.update(dt);
        t_ += dt;
        tick(n, dt);
    }
    void start(Node* n) {
        if (started_) return;
        started_ = true;
        begin(n);
    }

protected:
    virtual void begin(Node*) {}
    virtual void tick(Node*, double) {}
    bool started_;
    double t_;
    TweenSet tweens_;
};

class Sequence : public Effect {
public:
    explicit Sequence(const Spec* s) : spec_(s), next_(0), freed_(false) {}
    virtual void tick(Node* n, double) {
        while (next_ < 5 && spec_->steps[next_].op != OP_NONE && t_ >= spec_->steps[next_].at) run(n, spec_->steps[next_++]);
        if (freed_) return;
        if (spec_->free_at >= 0 ? t_ >= spec_->free_at : (t_ > 0.1 && finished(n))) {
            freed_ = true;
            n->queue_free();
        }
    }

private:
    void run(Node* n, const Step& s) {
        if (s.op == OP_SHAKE) {
            /* NGame.Instance?.ScreenShake(strength, duration) */
            std::string v = s.nodes;
            size_t slash = v.find('/');
            ScreenShake::get().shake(v.substr(0, slash), v.substr(slash + 1));
            return;
        }
        std::vector<Node*> list;
        nodes_of(n, s.nodes, list);
        for (size_t i = 0; i < list.size(); ++i) {
            Node* x = list[i];
            switch (s.op) {
            case OP_RESTART: restart(x); break;
            case OP_EMIT: if (is_particles(x)) ((Particles2D*)x)->restart(); break;
            case OP_STOP: if (is_particles(x)) ((Particles2D*)x)->emitting = false; break;
            case OP_SHOW: set_visible(x, true); break;
            case OP_HIDE: set_visible(x, false); break;
            default: break;
            }
        }
    }
    static bool finished(Node* n) { return !is_particles(n) || ((Particles2D*)n)->finished(); }
    const Spec* spec_;
    int next_;
    bool freed_;
};

/* NThinSliceVfx: the slash flies in from 400-500 px away toward a point
 * near the target's centre, the sparkle there; its colour on the slash's
 * process material; gone after 1 s. */
class ThinSlice : public Effect {
public:
    ThinSlice() : color_("Cyan") {}
    void setup(const Vec2& center, const std::string& color) {
        center_ = center + Vec2(rand_range(-50, 50), rand_range(-50, 50));
        color_ = color;
    }
    virtual void begin(Node* n) {
        Node* slash = n->get_node("Slash");
        if (!slash || !is_particles(slash)) return;
        float s = rand_range(0, PI_F * 2), d = rand_range(400, 500);
        Vec2 from(center_.x + d * cosf(s), center_.y + d * sinf(s));
        set_global(slash, from);
        Vec2 v = center_ - from;
        ((Node2D*)slash)->rotation = atan2f(v.y, v.x);
        ((Particles2D*)slash)->restart();
        Node* sparkle = slash->get_node("Sparkle");
        if (sparkle && is_particles(sparkle)) {
            set_global(sparkle, center_);
            ((Particles2D*)sparkle)->restart();
        }
        Particles2D* p = (Particles2D*)slash;
        if (color_ == "Red") p->color = html(0xFF9900);
        else if (color_ == "White") p->color = Color(1, 1, 1, 1);
        else if (color_ == "Cyan") p->color = html(0xC4FFE6);
    }
    virtual void tick(Node* n, double) {
        if (t_ >= 1.0 && !n->queued_for_deletion()) n->queue_free();
    }

private:
    Vec2 center_;
    std::string color_;
};

/* NStabVfx: the blade from 200 px out (the side it faces) springs onto a
 * point near the target's centre while the effect fades in and out. */
class Stab : public Effect {
public:
    Stab() : facing_enemies_(false), color_("Red") {}
    void setup(const Vec2& center, bool facing_enemies, const std::string& color) {
        facing_enemies_ = facing_enemies;
        color_ = color;
        center_ = center + Vec2(facing_enemies ? rand_range(0, 48) : rand_range(-48, 0), rand_range(-50, 50));
    }
    virtual void begin(Node* n) {
        Node* primary = n->get_node("Primary");
        Node* secondary = n->find_unique("Secondary");
        if (!primary || !primary->is_node2d()) return;
        Vec2 spawn = center_ + Vec2(rand_range(-12, 12), rand_range(-64, 64)) + Vec2(facing_enemies_ ? -200.0f : 200.0f, 0);
        set_global(primary, spawn);
        Vec2 d = spawn - center_;
        ((Node2D*)primary)->rotation = atan2f(d.y, d.x) + PI_F / 2;
        static const struct {
            const char* name;
            unsigned a, b;
        } colors[] = { { "Green", 0x00A52F, 0xFFCB2D }, { "Blue", 0x007BDD, 0x00EFF6 }, { "Purple", 0xA803FF, 0x00EFF3 },
                       { "White", 0x808080, 0xFFFFFF }, { "Cyan", 0x009599, 0x5CDCFF }, { "Gold", 0xEBA800, 0xFFE39C } };
        if (color_ != "Black") {
            unsigned a = 0xFF0000, b = 0xFFCB2D;
            for (size_t i = 0; i < sizeof(colors) / sizeof(colors[0]); ++i)
                if (color_ == colors[i].name) {
                    a = colors[i].a;
                    b = colors[i].b;
                }
            ((CanvasItem*)primary)->self_modulate = html(a);
            if (secondary && secondary->is_canvas_item()) ((CanvasItem*)secondary)->self_modulate = html(b);
        }
        CanvasItem* self = (CanvasItem*)n;
        tweens_.tween(&self->modulate.a, 1.0f, 0.25f);
        tweens_.tween(&((Node2D*)primary)->position, to_local(n, center_), 0.5f, TRANS_ELASTIC, EASE_OUT);
        tweens_.tween(&self->modulate.a, 0.0f, 0.25f, TRANS_LINEAR, EASE_IN_OUT, 0.25f);
    }
    virtual void tick(Node* n, double) {
        if (t_ >= 0.5 && !n->queued_for_deletion()) n->queue_free();
    }

private:
    Vec2 center_;
    bool facing_enemies_;
    std::string color_;
};

/* NGroundFireVfx: at the target's feet, the fire grows (Back Out) and fades
 * in, then the flame sprites start while it fades and shrinks to 2x;
 * other colours than red recolour its stepped fire. */
class GroundFire : public Effect {
public:
    GroundFire() : color_("Red"), phase_(0), main_(NULL), flames_(NULL), ember_(NULL) {}
    void setup(const std::string& color) { color_ = color; }
    virtual void begin(Node* n) {
        Node* mf = n->get_node("MainFire");
        main_ = mf && mf->is_node2d() ? (Node2D*)mf : NULL;
        Node* fl = n->get_node("FlameSprites");
        flames_ = is_particles(fl) ? (Particles2D*)fl : NULL;
        Node* em = n->get_node("Ember");
        ember_ = is_particles(em) ? (Particles2D*)em : NULL;
        apply_color(n);
        if (ember_) ember_->restart();
        if (!main_) return;
        main_->modulate = Color(1, 1, 1, 0);
        main_->scale = Vec2(0, 0);
        tweens_.tween(&main_->scale, Vec2(4, 4), 0.5f, TRANS_BACK, EASE_OUT);
        tweens_.tween(&main_->modulate.a, 0.9f, 0.5f, TRANS_CUBIC, EASE_OUT);
        phase_ = 1;
    }
    virtual void tick(Node* n, double) {
        if (phase_ == 1 && t_ >= 0.5) {
            phase_ = 2;
            if (flames_) flames_->restart();
            if (main_) {
                tweens_.tween(&main_->modulate.a, 0.0f, 0.5f);
                tweens_.tween(&main_->scale, Vec2(2, 2), 2.0f, TRANS_CUBIC, EASE_IN_OUT);
            }
            if (flames_) tweens_.tween(&flames_->modulate.a, 0.0f, 0.5f);
        } else if (phase_ == 2 && t_ >= 2.5) {
            phase_ = 3;
            if (flames_) flames_->emitting = false;
        } else if (phase_ == 3 && (!ember_ || ember_->finished())) {
            phase_ = 4;
            n->queue_free();
        }
    }

private:
    void apply_color(Node* n) {
        unsigned outer, inner, add;
        if (color_ == "Green") { outer = 0x2fa800; inner = 0x06a000; add = 0x541b00; }
        else if (color_ == "Blue") { outer = 0x0099cd; inner = 0x00a3bf; add = 0x000000; }
        else if (color_ == "Purple") { outer = 0x7821ff; inner = 0x3f21ff; add = 0x541b00; }
        else return;
        Node* fire = n->get_node("MainFire/VfxAdditiveStepFire");
        Node* mix = fire ? fire->get_node("SteppedFireMix") : NULL;
        Node* plus = fire ? fire->get_node("SteppedFireAdd") : NULL;
        set_color_param(mix, "OuterColor", html(outer));
        set_color_param(mix, "InnerColor", html(inner));
        set_color_param(plus, "OuterColor", html(add));
    }
    static void set_color_param(Node* n, const std::string& name, const Color& c) {
        if (!n || !n->is_canvas_item()) return;
        CanvasItem* ci = (CanvasItem*)n;
        ci->shader_params[name + ".r"] = c.r;
        ci->shader_params[name + ".g"] = c.g;
        ci->shader_params[name + ".b"] = c.b;
    }
    std::string color_;
    int phase_;
    Node2D* main_;
    Particles2D* flames_;
    Particles2D* ember_;
};

/* NStunnedVfx: "Stunned!" over the creature's hitbox, fading in (0.25 s
 * Expo Out), held 0.5 s, fading out (1 s) while it rises 100 px (2 s Quart
 * Out). */
class Stunned : public Effect {
public:
    virtual void begin(Node* n) {
        Node* l = n->find_unique("Label");
        if (!l || !l->is_control()) return;
        Control* label = (Control*)l;
        set_label(label, Loc::get("vfx", "STUNNED"));
        tweens_.tween(&label->modulate.a, 1.0f, 0.25f, TRANS_EXPO, EASE_OUT);
        tweens_.tween(&label->modulate.a, 0.0f, 1.0f, TRANS_LINEAR, EASE_OUT, 0.75f);
        float h = label->offset[SIDE_BOTTOM] - label->offset[SIDE_TOP];
        tweens_.tween(&label->offset[SIDE_TOP], label->offset[SIDE_TOP] - 100, 2.0f, TRANS_QUART, EASE_OUT);
        tweens_.tween(&label->offset[SIDE_BOTTOM], label->offset[SIDE_TOP] - 100 + h, 2.0f, TRANS_QUART, EASE_OUT);
    }
    virtual void tick(Node* n, double) {
        if (t_ >= 1.75 && !n->queued_for_deletion()) n->queue_free();
    }
};

/* NPowerUpVfx: its %BackVfx goes behind the creatures; both fade in and out
 * over the effect's second. */
class PowerUp : public Effect {
public:
    PowerUp() : back_(NULL), back_box_(NULL) {}
    void setup(Node* back_box) { back_box_ = back_box; }
    virtual void begin(Node* n) {
        Node* b = n->find_unique("BackVfx");
        if (!b || !b->is_canvas_item() || !back_box_) return;
        Vec2 g = global_of(b);
        b->parent->remove_child(b);
        back_box_->add_child(b);
        set_global(b, g);
        back_ = (CanvasItem*)b;
    }
    virtual void tick(Node* n, double) {
        float timer = 1.0f - (float)t_;
        float a = 1;
        float d = fabsf(timer - 0.5f);
        if (d > 0.4f) a = maxf(0, 1 - (d - 0.4f) / 0.1f);
        ((CanvasItem*)n)->modulate = Color(1, 1, 1, a);
        if (back_) back_->modulate = Color(1, 1, 1, a);
        if (timer < 0 && !n->queued_for_deletion()) {
            n->queue_free();
            if (back_) back_->queue_free();
            back_ = NULL;
        }
    }

private:
    CanvasItem* back_;
    Node* back_box_;
};

/* NBolasVfx: thrown along a quadratic curve 400-500 px above the higher of
 * the two, spinning down from 30 rad/s while its two weights spread. */
class Bolas : public Effect {
public:
    Bolas() : speed_(30), progress_(0) {}
    void setup(const Vec2& from, const Vec2& to) {
        from_ = from;
        to_ = to;
        control_ = Vec2((from.x + to.x) * 0.5f, minf(from.y, to.y) - rand_range(400, 500));
    }
    virtual void begin(Node* n) {
        CanvasItem* self = (CanvasItem*)n;
        tweens_.tween_from(&self->modulate.a, 0.0f, 1.0f, 0.25f);
        tweens_.tween_from(&progress_, 0.0f, 1.0f, 0.6f, TRANS_SINE, EASE_OUT);
        tweens_.tween(&self->modulate.a, 0.0f, 0.15f, TRANS_LINEAR, EASE_IN_OUT, 0.6f);
        follow(n);
    }
    virtual void tick(Node* n, double dt) {
        follow(n);
        float k = (float)dt;
        if (n->is_node2d()) ((Node2D*)n)->rotation -= k * speed_;
        speed_ -= k * 12;
        Node* b2 = n->get_node("Bola2");
        Node* b3 = n->get_node("Bola3");
        if (b2 && b2->is_node2d()) ((Node2D*)b2)->position.x -= 150 * k;
        if (b3 && b3->is_node2d()) ((Node2D*)b3)->position.x += 150 * k;
        if (t_ >= 0.75 && !n->queued_for_deletion()) n->queue_free();
    }

private:
    void follow(Node* n) {
        float t = progress_, u = 1 - t;
        set_global(n, from_ * (u * u) + control_ * (2 * u * t) + to_ * (t * t));
    }
    Vec2 from_, to_, control_;
    float speed_, progress_;
};

ScriptBehavior* make_sequence(Node* n) {
    const Spec* s = spec_for_script(n->script);
    return s ? new Sequence(s) : NULL;
}
ScriptBehavior* make_thin_slice(Node*) { return new ThinSlice(); }
ScriptBehavior* make_stab(Node*) { return new Stab(); }
ScriptBehavior* make_ground_fire(Node*) { return new GroundFire(); }
ScriptBehavior* make_stunned(Node*) { return new Stunned(); }
ScriptBehavior* make_power_up(Node*) { return new PowerUp(); }
ScriptBehavior* make_bolas(Node*) { return new Bolas(); }

std::string class_of(const std::string& call) {
    size_t dot = call.find('.');
    return dot == std::string::npos ? call : call.substr(0, dot);
}

/* The effect's colour: the call's, else the class's default. */
bool tint_of(const Spec& s, const Args& a, Color* out) {
    if (!a.colors.empty()) {
        *out = a.colors[0];
        return true;
    }
    if (s.default_tint) {
        *out = html(s.default_tint);
        return true;
    }
    return false;
}

Node* add_effect(SceneLibrary* lib, Node* box, const std::string& scene) {
    Node* fx = instance(lib, scene);
    if (!fx) return NULL;
    box->add_child(fx);
    return fx;
}

bool spawn_spec(SceneLibrary* lib, Node* box, const Spec& s, const Args& a, VfxAnchors& anchors) {
    if (a.creatures.empty() && !(a.has_word("Player") || a.has_word("Enemy"))) return false;
    Vec2 at;
    VfxCreature first, second;
    bool have_first = !a.creatures.empty() && anchors.creature(a.creatures[0], &first);
    bool have_second = a.creatures.size() > 1 && anchors.creature(a.creatures[1], &second);
    if (a.creatures.empty()) {
        /* Create(CombatSide, combatState, tint): the side's centre. */
        if (!anchors.side_center(a.has_word("Enemy"), &at)) return false;
    } else if (!have_first) {
        return false;
    } else if (s.anchor == AT_TARGET_CENTER) {
        if (!have_second) return false;
        at = second.center;
    } else {
        at = s.anchor == AT_BOTTOM ? first.bottom : s.anchor == AT_TOP ? first.top : first.center;
    }
    Node* fx = add_effect(lib, box, s.scene);
    if (!fx) return true;
    set_global(fx, at);
    if (!strcmp(s.name, "NWormyImpactVfx") && have_first) {
        set_global(exported_one(fx, "_groundPivot"), first.bottom);
        set_global(exported_one(fx, "_centerPivot"), first.center);
    }
    if (!strcmp(s.name, "NSporeImpactVfx")) {
        Node* pivot = exported_one(fx, "_poofPivot");
        const Json* range = fx->get_extra("_scaleRange");
        Vec2 r(1, 1);
        if (range) jv_vec2(*range, r);
        if (pivot && pivot->is_node2d()) {
            float k = rand_range(r.x, r.y);
            ((Node2D*)pivot)->scale = Vec2(k, k);
        }
    }
    if (!strcmp(s.name, "NSmokePuffVfx") && a.has_word("Purple")) {
        Node* clouds = fx->get_node("Clouds");
        if (is_particles(clouds)) ((Particles2D*)clouds)->color = html(0xF6B1FF);
    }
    if (!strcmp(s.name, "NGrandFinaleImpactVfx") && have_first) {
        set_global(exported_one(fx, "_centerParticles"), first.center);
        set_global(exported_one(fx, "_groundParticles"), first.bottom);
    }
    Color tint;
    if (s.tint != TINT_NONE && tint_of(s, a, &tint)) {
        if (s.tint == TINT_MODULATE) {
            ((CanvasItem*)fx)->modulate = tint;
        } else {
            std::vector<Node*> list;
            exported(fx, s.tint_nodes, list);
            for (size_t i = 0; i < list.size(); ++i) {
                if (s.tint == TINT_SELF && list[i]->is_canvas_item()) ((CanvasItem*)list[i])->self_modulate = tint;
                if (s.tint == TINT_MATERIAL && is_particles(list[i])) ((Particles2D*)list[i])->color = tint;
            }
        }
    }
    Node* flip = s.flip_node ? exported_one(fx, s.flip_node) : fx;
    if (flip && flip->is_node2d()) {
        Node2D* f = (Node2D*)flip;
        bool b = !a.bools.empty() && a.bools[0];
        if (s.flip == FLIP_RANDOM && chaotic_rand() % 2) f->scale.x = -f->scale.x;
        if (s.flip == FLIP_BOOL && !b) f->scale.x = -f->scale.x;
        if (s.flip == FLIP_BOOL_INVERTED && b) f->scale.x = -f->scale.x;
        if (s.flip == FLIP_ENEMY && !first.enemy) f->scale.x = -f->scale.x;
    }
    if (s.scale_arg && !a.numbers.empty() && fx->is_node2d()) ((Node2D*)fx)->scale = ((Node2D*)fx)->scale * a.numbers[0];
    if (s.aim && have_first && have_second && fx->is_node2d()) {
        Vec2 from = first.center;
        if (first.defect && strstr(s.name, "Hyperbeam")) from = from + Vec2(34, -30); /* Defect.EyelineOffset */
        Vec2 d = second.center - from;
        ((Node2D*)fx)->rotation = atan2f(d.y, d.x);
    }
    if (fx->behavior) ((Effect*)fx->behavior)->start(fx);
    return true;
}

/* NBigSlashVfx / NBigSlashImpactVfx (scripts in vfx_scripts.cpp): their
 * Create(Creature) placement and colours. */
bool spawn_big_slash(SceneLibrary* lib, Node* box, bool impact, const Args& a, VfxAnchors& anchors) {
    VfxCreature c;
    if (a.creatures.empty() || !anchors.creature(a.creatures[0], &c)) return true;
    Node* fx = add_effect(lib, box, impact ? "scenes/vfx/vfx_big_slash_impact.tscn" : "scenes/vfx/vfx_big_slash.tscn");
    if (!fx || !fx->is_node2d()) return true;
    set_global(fx, c.center);
    std::vector<Node*> list;
    exported(fx, "_modulateParticles", list);
    Color tint = impact ? html(0x80dbff) : html(0x50b598);
    for (size_t i = 0; i < list.size(); ++i)
        if (list[i]->is_canvas_item()) ((CanvasItem*)list[i])->self_modulate = tint;
    if (impact) {
        Node* core = exported_one(fx, "_corePivot");
        if (core && core->is_node2d()) ((Node2D*)core)->rotation = 60 * PI_F / 180;
    } else if (!c.enemy) {
        ((Node2D*)fx)->scale.x = -1; /* facingRight = creature.IsEnemy */
    }
    return true;
}

} /* namespace */

static const char* const OTHER_CALLS[] = { "NThinSliceVfx", "NStabVfx", "NGroundFireVfx", "NStunnedVfx", "NPowerUpVfx",
                                           "NBolasVfx", "NBigSlashVfx", "NBigSlashImpactVfx", "NHitSparkVfx" };

bool is_combat_vfx_call(const std::string& call) {
    if (call == "NCombatRoom.PlaySplashVfx") return true;
    std::string cls = class_of(call);
    if (call.compare(cls.size(), std::string::npos, ".Create") != 0 && call.find(".Create") == std::string::npos)
        return false;
    if (spec_named(cls)) return true;
    for (size_t i = 0; i < sizeof(OTHER_CALLS) / sizeof(OTHER_CALLS[0]); ++i)
        if (cls == OTHER_CALLS[i]) return true;
    return false;
}

bool spawn_combat_vfx(SceneLibrary* lib, Node* box, const std::string& call, const Json& args, VfxAnchors& anchors) {
    if (!is_combat_vfx_call(call)) return false;
    if (!box) return true;
    Args a(args);
    std::string cls = class_of(call);
    VfxCreature c;
    bool have = !a.creatures.empty() && anchors.creature(a.creatures[0], &c);
    if (call == "NCombatRoom.PlaySplashVfx") {
        /* NSplashVfx at the bottom of the hitbox, its particles tinted
         * (NLiquidOverlayVfx's overlay on the creature is not drawn). */
        if (!have) return true;
        Node* fx = add_effect(lib, box, "scenes/vfx/vfx_splash.tscn");
        if (!fx) return true;
        set_global(fx, c.bottom);
        Color tint = a.colors.empty() ? Color(1, 1, 1, 1) : a.colors[0];
        std::vector<Node*> list;
        exported(fx, "_particles", list);
        for (size_t i = 0; i < list.size(); ++i) {
            if (list[i]->is_canvas_item()) ((CanvasItem*)list[i])->self_modulate = tint;
            restart(list[i]);
        }
        fx->behavior = NULL;
        /* NSplashVfx frees itself after 2 s: a Sequence with no steps. */
        static const Spec splash = { "NSplashVfx", "", "", AT_CENTER, TINT_NONE, NULL, 0, FLIP_NONE, NULL, false, false,
                                     { { 0, OP_NONE, NULL } }, 2.0f };
        fx->behavior = new Sequence(&splash);
        return true;
    }
    const Spec* s = spec_named(cls);
    if (s) {
        spawn_spec(lib, box, *s, a, anchors);
        return true;
    }
    if (cls == "NBigSlashVfx" || cls == "NBigSlashImpactVfx") return spawn_big_slash(lib, box, cls == "NBigSlashImpactVfx", a, anchors);
    if (!have) return true;
    if (cls == "NHitSparkVfx") {
        /* requireInteractable (default true): not on dying creatures. */
        if (!c.interactable && !(a.bools.size() && !a.bools[0])) return true;
        Node* fx = add_effect(lib, box, "scenes/vfx/hit_spark_vfx.tscn");
        if (fx) set_global(fx, c.center);
        return true;
    }
    if (cls == "NThinSliceVfx") {
        Node* fx = add_effect(lib, box, "scenes/vfx/thin_slice_vfx.tscn");
        if (fx && fx->behavior) {
            ((ThinSlice*)fx->behavior)->setup(c.center, a.words.empty() ? "Cyan" : a.words[0]);
            ((Effect*)fx->behavior)->start(fx);
        }
        return true;
    }
    if (cls == "NStabVfx") {
        Node* fx = add_effect(lib, box, "scenes/vfx/stab_vfx.tscn");
        if (fx && fx->behavior) {
            ((Stab*)fx->behavior)->setup(c.center, !a.bools.empty() && a.bools[0], a.words.empty() ? "Red" : a.words[0]);
            ((Effect*)fx->behavior)->start(fx);
        }
        return true;
    }
    if (cls == "NGroundFireVfx") {
        Node* fx = add_effect(lib, box, "scenes/vfx/fires/vfx_ground_fire.tscn");
        if (!fx) return true;
        set_global(fx, c.bottom);
        if (fx->behavior) {
            ((GroundFire*)fx->behavior)->setup(a.words.empty() ? "Red" : a.words[0]);
            ((Effect*)fx->behavior)->start(fx);
        }
        return true;
    }
    if (cls == "NStunnedVfx") {
        Node* fx = add_effect(lib, box, "scenes/vfx/stunned_vfx.tscn");
        if (!fx) return true;
        set_global(fx, c.top);
        if (fx->behavior) ((Effect*)fx->behavior)->start(fx);
        return true;
    }
    if (cls == "NPowerUpVfx") {
        if (!c.interactable) return true;
        bool ghostly = call == "NPowerUpVfx.CreateGhostly";
        Node* fx = add_effect(lib, box, ghostly ? "scenes/vfx/vfx_ghostly_power_up/vfx_ghostly_power_up.tscn"
                                                : "scenes/vfx/vfx_power_up/vfx_power_up.tscn");
        if (!fx) return true;
        set_global(fx, c.center);
        if (fx->behavior) {
            ((PowerUp*)fx->behavior)->setup(anchors.back_box());
            ((Effect*)fx->behavior)->start(fx);
        }
        return true;
    }
    if (cls == "NBolasVfx") {
        VfxCreature target;
        if (a.creatures.size() < 2 || !anchors.creature(a.creatures[1], &target)) return true;
        Node* fx = add_effect(lib, box, "scenes/vfx/cards/bolas_vfx.tscn");
        if (fx && fx->behavior) {
            ((Bolas*)fx->behavior)->setup(c.center, target.center);
            ((Effect*)fx->behavior)->start(fx);
        }
        return true;
    }
    return true;
}

void install_combat_vfx_scripts() {
    for (size_t i = 0; i < sizeof(SPECS) / sizeof(SPECS[0]); ++i)
        register_script((std::string("res://src/Core/Nodes/Vfx/") + SPECS[i].script).c_str(), make_sequence);
    register_script("res://src/Core/Nodes/Vfx/NThinSliceVfx.cs", make_thin_slice);
    register_script("res://src/Core/Nodes/Vfx/NStabVfx.cs", make_stab);
    register_script("res://src/Core/Nodes/Vfx/NGroundFireVfx.cs", make_ground_fire);
    register_script("res://src/Core/Nodes/Vfx/NStunnedVfx.cs", make_stunned);
    register_script("res://src/Core/Nodes/Vfx/NPowerUpVfx.cs", make_power_up);
    register_script("res://src/Core/Nodes/Vfx/Cards/NBolasVfx.cs", make_bolas);
}

} /* namespace spire */

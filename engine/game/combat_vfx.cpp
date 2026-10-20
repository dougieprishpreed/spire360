#include "combat_vfx.h"

#include "../audio/game_audio.h"
#include "../scene/controls.h"
#include "../scene/node.h"
#include "../scene/particles.h"
#include "../scene/scene.h"
#include "../scene/script.h"
#include "../scene/spine_sprite.h"
#include "../scene/viewport.h"
#include "../spine/spine.h"
#include "../scene/tween.h"
#include "../scene/value.h"
#include "loc.h"
#include "screen_shake.h"
#include "ui_util.h"

#include <algorithm>
#include <ctype.h>
#include <stdlib.h>
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
    if (s.size() < 2 || s[0] != 'e') return false;
    for (size_t i = 1; i < s.size(); ++i)
        if (!isdigit((unsigned char)s[i])) return false;
    return true;
}

/* A cue's arguments by kind, in order. */
/* A creature's point as the logic's stand-in nodes encode it
 * ("e1@bottom+34,-30": StandIns.Describe). */
struct Place {
    std::string who, anchor;
    Vec2 offset;
};

bool parse_place(const std::string& s, Place* out) {
    size_t at = s.find('@');
    if (at == std::string::npos || !is_creature_id(s.substr(0, at))) return false;
    out->who = s.substr(0, at);
    size_t sign = s.find_first_of("+-", at + 1);
    out->anchor = s.substr(at + 1, sign == std::string::npos ? std::string::npos : sign - at - 1);
    out->offset = Vec2();
    if (sign != std::string::npos) {
        size_t comma = s.find(',', sign);
        out->offset.x = (float)atof(s.substr(sign, comma - sign).c_str());
        if (comma != std::string::npos) out->offset.y = (float)atof(s.substr(comma + 1).c_str());
    }
    return true;
}

struct Args {
    std::vector<std::string> creatures, words, paths;
    std::vector<std::vector<std::string> > lists; /* creature lists */
    std::vector<Place> places;
    std::vector<Color> colors;
    std::vector<bool> bools;
    std::vector<float> numbers;
    explicit Args(const Json& a) {
        for (size_t i = 0; i < a.size(); ++i) {
            const Json& v = a[i];
            if (v.is_bool()) bools.push_back(v.b());
            else if (v.is_number()) numbers.push_back((float)v.num());
            else if (v.is_array()) {
                std::vector<std::string> l;
                for (size_t k = 0; k < v.size(); ++k)
                    if (v[k].is_string()) l.push_back(v[k].str());
                lists.push_back(l);
            } else if (v.is_string()) {
                Color c;
                Place p;
                if (parse_color(v.str(), &c)) colors.push_back(c);
                else if (is_creature_id(v.str())) creatures.push_back(v.str());
                else if (parse_place(v.str(), &p)) places.push_back(p);
                else if (v.str().find('/') != std::string::npos) paths.push_back(v.str());
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

/* A creature's encoded point on screen. */
bool resolve(const Place& p, VfxAnchors& anchors, Vec2* out) {
    VfxCreature c;
    if (!anchors.creature(p.who, &c)) return false;
    if (p.anchor == "hitbox") *out = c.hitbox.pos + p.offset;
    else if (p.anchor == "hitbox_size") *out = c.hitbox.size + p.offset;
    else *out = (p.anchor == "bottom" ? c.bottom : p.anchor == "top" ? c.top : c.center) + p.offset;
    return true;
}

float exported_float(Node* n, const char* key, float fallback) {
    const Json* v = n->get_extra(key);
    return v && v->is_number() ? (float)v->num() : fallback;
}

void play_sfx(const char* event) {
    SceneLibrary* lib = SceneLibrary::current();
    if (!lib || !lib->audio()) return;
    Json cue; /* set() makes it an object */
    cue.set("t", Json::make_string("sfx"));
    cue.set("op", Json::make_string("play"));
    cue.set("path", Json::make_string(event));
    cue.set("volume", Json::make_number(1));
    lib->audio()->cue(cue);
}

void restart_list(Node* n, const char* key) {
    std::vector<Node*> list;
    exported(n, key, list);
    for (size_t i = 0; i < list.size(); ++i) restart(list[i]);
}

void emitting_list(Node* n, const char* key, bool on) {
    std::vector<Node*> list;
    exported(n, key, list);
    for (size_t i = 0; i < list.size(); ++i)
        if (is_particles(list[i])) ((Particles2D*)list[i])->emitting = on;
}

/* NItemThrowVfx: the item from the source to the target along its curves
 * (_horizontalCurve lerps, _verticalCurve lifts by _heightMultiplier),
 * spinning by _rotationInfluenceCurve * _rotationMultiplier deg/s, for
 * _flightTime; 80 px wide. */
class ItemThrow : public Effect {
public:
    explicit ItemThrow(Node* n) : flight_(exported_float(n, "_flightTime", 0.55f)),
        height_(exported_float(n, "_heightMultiplier", 350)), spin_(exported_float(n, "_rotationMultiplier", -2880)),
        sprite_(NULL) {
        read(n, "_horizontalCurve", horizontal_);
        read(n, "_verticalCurve", vertical_);
        read(n, "_rotationInfluenceCurve", rotation_);
    }
    void setup(const Vec2& from, const Vec2& to, Texture2DRes* texture) {
        from_ = from;
        to_ = to;
        texture_ = texture;
    }
    virtual void begin(Node* n) {
        Node* s = exported_one(n, "_itemSprite");
        if (!s || s->type != "Sprite2D") return;
        sprite_ = (Sprite2D*)s;
        if (texture_) {
            sprite_->texture = texture_;
            Vec2 size = texture_->size();
            float k = size.x > 0 ? 80.0f / size.x : 1;
            sprite_->scale = Vec2(k, k);
        }
        sprite_->visible = texture_ != NULL;
        sprite_->rotation = rand_range(0, 2 * PI_F);
        set_global(sprite_, from_);
    }
    virtual void tick(Node* n, double dt) {
        if (!sprite_) return;
        float t = flight_ > 0 ? (float)(t_ / flight_) : 1;
        if (t >= 1) {
            sprite_->visible = false;
            if (!n->queued_for_deletion()) n->queue_free();
            return;
        }
        float w = horizontal_.valid() ? horizontal_.sample(t) : t, h = vertical_.valid() ? vertical_.sample(t) : 0;
        float r = rotation_.valid() ? rotation_.sample(t) : 1;
        sprite_->rotation += r * spin_ * PI_F / 180 * (float)dt;
        set_global(sprite_, from_ + (to_ - from_) * w + Vec2(0, -h * height_));
    }

private:
    static void read(Node* n, const char* key, CurveData& out) {
        const Json* v = n->get_extra(key);
        if (v) CurveData::from(script_resource(*v), out);
    }
    float flight_, height_, spin_;
    CurveData horizontal_, vertical_, rotation_;
    Sprite2D* sprite_;
    Vec2 from_, to_;
    Texture2DRes* texture_;
};

/* NSmallMagicMissileVfx / NLargeMagicMissileVfx: anticipation at the top of
 * the screen (y 80) on the line from the target at -30 degrees, then the
 * projectile from there to the target (over WaitTime), the impact, a shake
 * (Medium Short / Strong Normal); 2 s more. */
class Missile : public Effect {
public:
    Missile(Node* n, bool large) : large_(large), phase_(0), container_(NULL) {
        anticipation_ = exported_float(n, "_anticipationDuration", 0.2f);
        wait_ = exported_float(n, "_projectileWaitTime", 0.2f);
        offset_ = exported_float(n, "_projectileOffset", 100);
    }
    virtual void begin(Node* n) {
        Vec2 at = global_of(n);
        Vec2 dir(0.5f, 0.8660254f);
        float t = (80 - at.y) / dir.y;
        Vec2 top = at + dir * t;
        set_global(exported_one(n, "_anticipationContainer"), top);
        start_ = top + dir * offset_;
        end_ = at + dir * offset_;
        Node* c = exported_one(n, "_projectileContainer");
        container_ = c && c->is_canvas_item() ? (CanvasItem*)c : NULL;
        if (container_) container_->visible = false;
        restart_list(n, "_anticipationParticles");
    }
    virtual void tick(Node* n, double) {
        if (phase_ == 0 && t_ >= anticipation_) {
            phase_ = 1;
            restart_list(n, "_projectileStartParticles");
            if (container_) {
                set_global(container_, start_);
                container_->visible = true;
            }
            restart_list(n, "_projectileParticles");
        }
        if (phase_ == 1) {
            float k = wait_ > 0 ? (float)((t_ - anticipation_) / wait_) : 1;
            if (k < 1) {
                if (container_) set_global(container_, start_ + (end_ - start_) * k);
            } else {
                phase_ = 2;
                if (container_) container_->visible = false;
                restart_list(n, "_impactParticles");
                ScreenShake::get().shake(large_ ? "Strong" : "Medium", large_ ? "Normal" : "Short");
            }
        }
        if (phase_ == 2 && t_ >= anticipation_ + wait_ + 2 && !n->queued_for_deletion()) n->queue_free();
    }

private:
    bool large_;
    int phase_;
    float anticipation_, wait_, offset_;
    Vec2 start_, end_;
    CanvasItem* container_;
};

/* NBgGroundSpikeVfx / NFgGroundSpikeVfx: a spike springs from the ground
 * near the point (behind: 10-96 px above it; in front: 10-32 px below),
 * skewed and scaled at random, drifting sideways while it skews further,
 * shrinks and fades (Expo Out). */
class GroundSpike : public Effect {
public:
    explicit GroundSpike(bool front) : front_(front), right_(true), color_("Red") {}
    void setup(const Vec2& at, bool right, const std::string& color) {
        at_ = at;
        right_ = right;
        color_ = color;
    }
    virtual void begin(Node* n) {
        if (!n->is_node2d()) return;
        Node2D* s = (Node2D*)n;
        s->skew = (right_ ? rand_range(15, 30) : rand_range(-30, -15)) * 0.0174533f;
        float k = rand_range(0.5f, 1.5f);
        s->scale = Vec2(rand_range(0.8f, 1.2f), rand_range(0.8f, 2.0f)) * k;
        Vec2 start = at_ + Vec2(right_ ? rand_range(40, 160) : rand_range(-160, -40),
                                front_ ? rand_range(10, 32) : rand_range(-96, -10));
        set_global(n, start);
        velocity_ = Vec2(right_ ? rand_range(50, 250) : rand_range(-250, -50), rand_range(-5, 5)) * (1.0f / k);
        if (color_ == "Purple") s->modulate = Color(rand_range(0, 0.2f), rand_range(0.2f, 0.8f), 1, 0.5f);
        else if (color_ == "White") {
            float g = rand_range(0.2f, 0.8f);
            s->modulate = Color(g, g, g, 0.5f);
        } else if (color_ == "Cyan") {
            float g = rand_range(0.6f, 1);
            s->modulate = Color(0.2f, g, g, 0.5f);
        } else if (color_ == "Gold") {
            float g = rand_range(0.6f, 1);
            s->modulate = Color(g, g, 0.2f, 1);
        } else {
            s->modulate = Color(1, rand_range(0.2f, 0.8f), rand_range(0, 0.2f), 0.5f);
        }
        life_ = rand_range(0.25f, 1);
        float delay = rand_range(0.01f, 0.2f);
        tweens_.tween(&s->skew, (right_ ? rand_range(30, 60) : rand_range(-60, -30)) * 0.0174533f, life_, TRANS_EXPO,
                      EASE_OUT, delay);
        tweens_.tween(&s->scale, s->scale * rand_range(0.1f, 0.5f), life_, TRANS_EXPO, EASE_OUT, delay);
        tweens_.tween(&s->modulate.a, 0.0f, life_, TRANS_EXPO, EASE_OUT, delay);
        life_ += delay;
    }
    virtual void tick(Node* n, double dt) {
        if (n->is_node2d()) ((Node2D*)n)->position = ((Node2D*)n)->position + velocity_ * (float)dt;
        if (t_ >= life_ && !n->queued_for_deletion()) n->queue_free();
    }

private:
    bool front_, right_;
    std::string color_;
    Vec2 at_, velocity_;
    float life_;
};

/* NKinPriestGrenadeVfx: noise and the explosion's base at once, the
 * crypto particles 0.1 s later; gone after 5.1 s. */
class KinPriestGrenade : public Effect {
public:
    virtual void begin(Node* n) {
        Node* noise = n->get_node("NoiseParticles");
        Node* base = n->get_node("ExplosionBaseParticle");
        if (is_particles(noise)) ((Particles2D*)noise)->restart();
        if (is_particles(base)) ((Particles2D*)base)->restart();
        SceneLibrary* lib = SceneLibrary::current();
        if (lib && lib->audio()) lib->audio()->ui("blunt_attack.mp3");
    }
    virtual void tick(Node* n, double) {
        if (!fired_ && t_ >= 0.1) {
            fired_ = true;
            Node* crypto = n->get_node("CryptoParticles");
            if (is_particles(crypto)) ((Particles2D*)crypto)->restart();
        }
        if (t_ >= 5.1 && !n->queued_for_deletion()) n->queue_free();
    }
    KinPriestGrenade() : fired_(false) {}

private:
    bool fired_;
};

/* NSleepingVfx: a glow burst and the stars and Zs floating (to the right
 * or left) until the monster wakes (Stop: they stop emitting, gone 5 s
 * later). */
class Sleeping : public Effect {
public:
    Sleeping() : right_(true), stop_at_(-1) {}
    void setup(bool right) { right_ = right; }
    void stop() {
        if (stop_at_ < 0) stop_at_ = t_;
    }
    virtual void begin(Node* n) {
        restart_list(n, "_burstParticles");
        restart_list(n, "_continuousParticles");
        if (!right_) {
            Node* z = exported_one(n, "_zParticles");
            if (z && z->is_node2d()) ((Node2D*)z)->scale.x = -((Node2D*)z)->scale.x;
        }
    }
    virtual void tick(Node* n, double) {
        if (stop_at_ >= 0 && stop_at_ + 1e-6 >= t_ - 0.0001 && stop_at_ <= t_) emitting_list(n, "_continuousParticles", false);
        if (stop_at_ >= 0 && t_ >= stop_at_ + 5 && !n->queued_for_deletion()) n->queue_free();
    }

private:
    bool right_;
    double stop_at_;
};

/* NHyperbeamVfx: from the Defect's eye toward the target: anticipation
 * (0.525 s), the laser (0.5 s, a Medium Normal shake), its end (a Strong
 * Short shake); 2 s more. */
class Hyperbeam : public Effect {
public:
    Hyperbeam() : phase_(0) {}
    virtual void begin(Node* n) {
        play_sfx("event:/sfx/characters/defect/defect_hyperbeam");
        show_laser(n, false);
        restart_list(n, "_anticipationParticles");
    }
    virtual void tick(Node* n, double) {
        if (phase_ == 0 && t_ >= 0.525) {
            phase_ = 1;
            show_laser(n, true);
            ScreenShake::get().shake("Medium", "Normal");
        } else if (phase_ == 1 && t_ >= 1.025) {
            phase_ = 2;
            show_laser(n, false);
            restart_list(n, "_laserEndParticles");
            ScreenShake::get().shake("Strong", "Short");
        } else if (phase_ == 2 && t_ >= 3.025 && !n->queued_for_deletion()) {
            n->queue_free();
        }
    }

private:
    static void show_laser(Node* n, bool on) {
        std::vector<Node*> list;
        exported(n, "_laserParticles", list);
        for (size_t i = 0; i < list.size(); ++i) {
            set_visible(list[i], on);
            if (on) restart(list[i]);
        }
        set_visible(exported_one(n, "_laserLine"), on);
        set_visible(exported_one(n, "_laserContainer"), on);
    }
    int phase_;
};

/* NSweepingBeamVfx: from the Defect's eye, the beam sweeps over
 * _sweepDuration (the sweep particles by _sweepingIndexCurve), each target
 * hit halfway (NSweepingBeamImpactVfx, a Medium Normal shake); the end
 * particles, 2 s more. */
class SweepingBeam : public Effect {
public:
    explicit SweepingBeam(Node* n) : duration_(exported_float(n, "_sweepDuration", 0.65f)), previous_(-1), hit_(false),
        ended_(false), box_(NULL) {
        const Json* v = n->get_extra("_sweepingIndexCurve");
        if (v) CurveData::from(script_resource(*v), curve_);
    }
    void setup(const std::vector<Vec2>& targets, Node* box) {
        targets_ = targets;
        box_ = box;
    }
    virtual void begin(Node* n) {
        restart_list(n, "_startParticles");
        restart_list(n, "_emittingParticles");
    }
    virtual void tick(Node* n, double) {
        if (!ended_ && t_ < duration_) {
            float k = (float)(t_ / duration_);
            int index = (int)floorf(curve_.valid() ? curve_.sample(k) : k * 3);
            std::vector<Node*> sweep;
            exported(n, "_sweepingParticles", sweep);
            if (index != previous_ && index >= 0 && index < (int)sweep.size()) {
                restart(sweep[(size_t)index]);
                previous_ = index;
            }
            if (k >= 0.5f && !hit_) {
                hit_ = true;
                ScreenShake::get().shake("Medium", "Normal");
                for (size_t i = 0; i < targets_.size() && box_; ++i) {
                    Node* fx = instance(SceneLibrary::current(), "scenes/vfx/vfx_sweeping_beam_impact.tscn");
                    if (!fx) continue;
                    box_->add_child(fx);
                    set_global(fx, targets_[i]);
                    if (fx->behavior) ((Effect*)fx->behavior)->start(fx);
                }
            }
        } else if (!ended_) {
            ended_ = true;
            restart_list(n, "_endParticles");
            emitting_list(n, "_emittingParticles", false);
        }
        if (ended_ && t_ >= duration_ + 2 && !n->queued_for_deletion()) n->queue_free();
    }

private:
    float duration_;
    int previous_;
    bool hit_, ended_;
    CurveData curve_;
    std::vector<Vec2> targets_;
    Node* box_;
};

/* NGrandFinaleVfx: a spotlight fades in (1 s) with its particles from the
 * top middle of the screen; anticipation at 1.25 s, the slash at 1.5 s, a
 * Strong Normal shake at 1.625 s, the end particles and the spotlight
 * fading out (0.5 s) at 1.6375 s; 2 s more. */
class GrandFinale : public Effect {
public:
    GrandFinale() : phase_(0), spotlight_(NULL) {}
    void setup(const Vec2& player) { player_ = player; }
    virtual void begin(Node* n) {
        set_global(exported_one(n, "_anticipationParticles"), player_);
        set_global(exported_one(n, "_slashParticles"), player_);
        set_global(exported_one(n, "_endParticles"), player_);
        Node* s = exported_one(n, "_spotlight");
        spotlight_ = s && s->is_canvas_item() ? (CanvasItem*)s : NULL;
        if (spotlight_) {
            spotlight_->modulate = Color(1, 1, 1, 0);
            tweens_.tween(&spotlight_->modulate.a, 1.0f, 1.0f);
        }
        Node* sp = exported_one(n, "_spotlightParticles");
        set_global(sp, Vec2(960, 0));
        restart(sp);
    }
    virtual void tick(Node* n, double) {
        if (phase_ == 0 && t_ >= 1.25) { phase_ = 1; restart(exported_one(n, "_anticipationParticles")); }
        if (phase_ == 1 && t_ >= 1.5) { phase_ = 2; restart(exported_one(n, "_slashParticles")); }
        if (phase_ == 2 && t_ >= 1.625) { phase_ = 3; ScreenShake::get().shake("Strong", "Normal"); }
        if (phase_ == 3 && t_ >= 1.6375) {
            phase_ = 4;
            restart(exported_one(n, "_endParticles"));
            if (spotlight_) tweens_.tween(&spotlight_->modulate.a, 0.0f, 0.5f);
        }
        if (phase_ == 4 && t_ >= 3.6375 && !n->queued_for_deletion()) n->queue_free();
    }

private:
    int phase_;
    Vec2 player_;
    CanvasItem* spotlight_;
};

/* NHellraiserSwordVfx: a sword rising out of the ground (Spring) as it turns
 * from red to its shade, held, then sinking (Expo In) as it fades. */
class HellraiserSword : public Effect {
public:
    HellraiserSword() : pos_y_(0), shade_(1), life_(0) {}
    void setup(const Vec2& at, float pos_y, float shade) {
        at_ = at;
        pos_y_ = pos_y;
        shade_ = shade;
    }
    virtual void begin(Node* n) {
        Node* sw = n->get_node("TextureRect");
        if (!n->is_control() || !sw || !sw->is_control()) return;
        Control* self = (Control*)n;
        Control* sword = (Control*)sw;
        if (sword->type == "TextureRect" && chaotic_rand() % 2) ((TextureRect*)sword)->flip_h = true;
        sword->rotation = rand_range(-20, 20) * PI_F / 180;
        float k = rand_range(1, 2);
        self->scale = Vec2(rand_range(0.7f, 0.9f), rand_range(0.8f, 1.2f)) * k;
        float z;
        do {
            float u1 = 1 - rand_range(0, 1), u2 = rand_range(0, 1);
            z = sqrtf(-2 * logf(u1 > 1e-6f ? u1 : 1e-6f)) * cosf(2 * PI_F * u2);
        } while (z < 0 || z > 1);
        set_global(n, at_);
        self->set_position(self->position + Vec2(z * 1000 - 500, pos_y_));
        float delay = rand_range(0, 0.8f);
        sword->set_position(Vec2(25, 300));
        tweens_.tween_from(&sword->offset[SIDE_TOP], 300, 80, 0.25f, TRANS_SPRING, EASE_OUT, delay);
        tweens_.tween_from(&sword->offset[SIDE_BOTTOM], 300 + h(sword), 80 + h(sword), 0.25f, TRANS_SPRING, EASE_OUT, delay);
        self->modulate = Color(1, 0, 0, 1);
        tweens_.tween(&self->modulate.g, shade_, 0.25f, TRANS_LINEAR, EASE_IN_OUT, delay);
        tweens_.tween(&self->modulate.b, shade_, 0.25f, TRANS_LINEAR, EASE_IN_OUT, delay);
        tweens_.tween(&self->modulate.r, shade_, 0.25f, TRANS_LINEAR, EASE_IN_OUT, delay);
        tweens_.tween(&sword->offset[SIDE_TOP], 200, 0.5f, TRANS_EXPO, EASE_IN, delay + 0.5f);
        tweens_.tween(&sword->offset[SIDE_BOTTOM], 200 + h(sword), 0.5f, TRANS_EXPO, EASE_IN, delay + 0.5f);
        tweens_.tween(&self->modulate.a, 0.0f, 0.5f, TRANS_LINEAR, EASE_IN_OUT, delay + 0.5f);
        life_ = delay + 1.0f;
    }
    virtual void tick(Node* n, double) {
        if (started_ && t_ >= life_ && !n->queued_for_deletion()) n->queue_free();
    }

private:
    static float h(Control* c) { return c->offset[SIDE_BOTTOM] - c->offset[SIDE_TOP]; }
    Vec2 at_;
    float pos_y_, shade_, life_;
};

/* NAdditiveOverlayVfx: the screen tinted (additive) up to 0.1 (Expo Out,
 * 0.5 s), held 0.5 s, down in 0.5 s. */
class AdditiveOverlay : public Effect {
public:
    virtual void begin(Node* n) {
        CanvasItem* c = (CanvasItem*)n;
        tweens_.tween(&c->modulate.a, 0.1f, 0.5f, TRANS_EXPO, EASE_OUT);
        tweens_.tween(&c->modulate.a, 0.0f, 0.5f, TRANS_LINEAR, EASE_IN_OUT, 1.0f);
    }
    virtual void tick(Node* n, double) {
        if (t_ >= 1.5 && !n->queued_for_deletion()) n->queue_free();
    }
};

Node* add_effect(SceneLibrary* lib, Node* box, const std::string& scene);

/* NHellraiserVfx: ten swords in front of the target's feet (rising further
 * up the screen the higher they stand) and ten behind, a red flash over the
 * screen; its sound. */
void spawn_hellraiser(SceneLibrary* lib, Node* box, Node* back, const Vec2& feet) {
    Vec2 at = feet + Vec2(-100, -200);
    for (int side = 0; side < 2; ++side) {
        std::vector<float> ys;
        for (int i = 0; i < 10; ++i) ys.push_back(side == 0 ? rand_range(10, 50) : rand_range(-50, -10));
        std::sort(ys.begin(), ys.end());
        if (side == 1) play_sfx("event:/sfx/characters/ironclad/ironclad_hellraiser");
        for (size_t i = 0; i < ys.size(); ++i) {
            Node* parent = side == 0 ? box : (back ? back : box);
            Node* fx = add_effect(lib, parent, "scenes/vfx/cards/vfx_hellraiser/hellraiser_sword_vfx.tscn");
            if (!fx || !fx->behavior) continue;
            float shade = side == 0 ? 0.8f + (ys[i] - 10) / 40 * 0.2f : 0.7f + (ys[i] + 10) / -40 * -0.3f;
            ((HellraiserSword*)fx->behavior)->setup(at, ys[i], shade);
            ((Effect*)fx->behavior)->start(fx);
        }
    }
    Node* overlay = add_effect(lib, box, "scenes/vfx/additive_overlay_vfx.tscn");
    if (overlay && overlay->behavior) ((Effect*)overlay->behavior)->start(overlay);
}

/* NDoomSubEmitterVfx: its layers scaled across to the width (the vertical
 * shrinking layer also down while narrower than 1), its particles restarted
 * when it shows and stopped when it hides, and its spears shooting up
 * again and again while on. */
class DoomSubEmitter : public Effect {
public:
    explicit DoomSubEmitter(Node*) : cur_(1), on_(false), hide_at_(-1), shrink_(NULL) {}
    virtual void begin(Node* n) {
        /* _Ready (the children exist by now) */
        exported(n, "_scalableLayers", layers_);
        exported(n, "_spears", spears_);
        shrink_ = exported_one(n, "_verticalShrinkingLayer");
        for (size_t i = 0; i < layers_.size(); ++i)
            base_.push_back(layers_[i]->is_node2d() ? ((Node2D*)layers_[i])->scale : Vec2(1, 1));
        spear_state_.resize(spears_.size());
        visibility(false);
        cur_ = 0;
        apply();
    }
    void show_or_hide(float width, float time) {
        on_ = width > 0.1f;
        float delay = on_ ? 0.0f : 0.3f;
        if (on_) visibility(true);
        else time = cur_ * 0.15f;
        tweens_.kill(&cur_);
        tweens_.tween(&cur_, on_ ? width : 0.0f, time, TRANS_CUBIC, on_ ? EASE_OUT : EASE_IN, delay);
        hide_at_ = on_ ? -1 : t_ + delay + time;
    }
    virtual void tick(Node*, double dt) {
        apply();
        if (!on_ && hide_at_ >= 0 && t_ >= hide_at_) {
            hide_at_ = -1;
            visibility(false);
        }
        for (size_t i = 0; i < spears_.size(); ++i) {
            Spear& s = spear_state_[i];
            if (!s.live || !spears_[i]->is_control()) continue;
            Control* c = (Control*)spears_[i];
            s.t += (float)dt;
            float k = s.dur > 0 ? minf(s.t / s.dur, 1) : 1;
            c->scale = Vec2(s.sx, s.sy * (1 - k));
            c->modulate.a = s.alpha * minf(s.t / 0.2f, 1) * minf(s.t / 0.2f, 1);
            if (k >= 1) {
                if (on_) fire(i);
                else s.live = false;
            }
        }
    }

private:
    struct Spear {
        bool live;
        float t, dur, sx, sy, alpha;
        Spear() : live(false), t(0), dur(1), sx(1), sy(1), alpha(0) {}
    };
    void apply() {
        for (size_t i = 0; i < layers_.size(); ++i) {
            if (!layers_[i]->is_node2d()) continue;
            Vec2 b = base_[i];
            ((Node2D*)layers_[i])->scale = layers_[i] == shrink_ && cur_ < 1 ? Vec2(b.x * cur_, b.y * cur_) : Vec2(b.x * cur_, b.y);
        }
    }
    void visibility(bool on) {
        for (size_t i = 0; i < layers_.size(); ++i)
            if (is_particles(layers_[i])) {
                if (on) ((Particles2D*)layers_[i])->restart();
                else ((Particles2D*)layers_[i])->emitting = false;
            }
        if (on)
            for (size_t i = 0; i < spears_.size(); ++i) fire(i);
    }
    /* FireSpear: across the 140 px region (-20 for the rect's origin),
     * leaning with its x, shrinking down to nothing in 0.3-0.6 s while it
     * fades in to 0.4-0.7 (0.2 s Quad In). */
    void fire(size_t i) {
        if (!spears_[i]->is_control()) return;
        Control* c = (Control*)spears_[i];
        float x = rand_range(-70, 70) - 20;
        float w = c->offset[SIDE_RIGHT] - c->offset[SIDE_LEFT];
        c->offset[SIDE_LEFT] = x;
        c->offset[SIDE_RIGHT] = x + w;
        c->rotation = (x * 0.15f + 4) * PI_F / 180;
        Spear& s = spear_state_[i];
        s.live = true;
        s.t = 0;
        s.dur = rand_range(0.3f, 0.6f);
        /* 0.4 / width: at width 0 (the first volley, as the effect opens)
         * Godot's scale is infinite and nothing shows */
        s.sx = cur_ > 0.001f ? 0.4f / cur_ : 0.0f;
        s.sy = rand_range(1, 1.5f);
        s.alpha = rand_range(0.4f, 0.7f);
        c->modulate.a = 0;
        c->scale = Vec2(s.sx, s.sy);
    }
    std::vector<Node*> layers_, spears_;
    std::vector<Vec2> base_;
    std::vector<Spear> spear_state_;
    float cur_;
    bool on_;
    double hide_at_;
    Node* shrink_;
};

/* NDoomVfx: at the hitbox's bottom middle; the creature's body (when it
 * dies) taken into the effect, grayed (hsv s 0) and sinking below the
 * floor line (the Mask's clip) after 0.75 s over 0.75 s (Expo In); the
 * sub-emitters as wide as 1.5x the hitbox / 260; a weak shake about 180
 * degrees; its sound. (The game draws the body into a SubViewport shown by
 * Mask/Visual; the body itself goes under the Mask here.) */
class DoomVfx : public Effect {
public:
    DoomVfx() : holder_(NULL), phase_(0) {}
    void setup(const Vec2& size, Node* body, bool die) {
        size_ = size * 1.5f;
        body_ = die ? body : NULL;
    }
    virtual void begin(Node* n) {
        play_sfx("event:/sfx/characters/necrobinder/necrobinder_doom_kill");
        set_visible(n->find_unique("Visual"), false);
        Node* mask = n->find_unique("Mask");
        if (body_ && mask && mask->is_control() && body_->is_node2d() && body_->parent) {
            Xform2D xf = ((Node2D*)body_)->global_transform();
            Node* h = SceneLibrary::create_node("Node2D");
            mask->add_child(h);
            holder_ = (Node2D*)h;
            /* at the effect's origin: the Mask's top left is at its offsets
             * (it is not laid out yet) */
            Control* m = (Control*)mask;
            holder_->position = Vec2(-m->offset[SIDE_LEFT], -m->offset[SIDE_TOP]);
            body_->parent->remove_child(body_);
            holder_->add_child(body_);
            Node2D* b = (Node2D*)body_;
            b->position = xf.origin - global_of(n);
            b->rotation = 0;
            b->scale = Vec2(sqrtf(xf.x.x * xf.x.x + xf.x.y * xf.x.y), sqrtf(xf.y.x * xf.y.x + xf.y.y * xf.y.y));
            gray(body_);
            tweens_.tween(&holder_->position.y, holder_->position.y + size_.y, 0.75f, TRANS_EXPO, EASE_IN, 0.75f);
        }
        ScreenShake::get().shake("Weak", "Short", 180 + rand_range(-10, 10));
        show(n, size_.x / 260, 0.5f);
    }
    virtual void tick(Node* n, double) {
        if (phase_ == 0 && t_ >= 1.5) {
            phase_ = 1;
            if (holder_) holder_->visible = false;
            show(n, 0, 0.25f);
        }
        if (phase_ == 1 && t_ >= 3.5 && !n->queued_for_deletion()) n->queue_free();
    }

private:
    static void show(Node* n, float width, float time) {
        static const char* const sub[] = { "DoomVfxBack", "DoomVfxFront" };
        for (int i = 0; i < 2; ++i) {
            Node* s = n->get_node(sub[i]);
            if (!s || !s->behavior) continue;
            ((Effect*)s->behavior)->start(s); /* its _Ready first */
            ((DoomSubEmitter*)s->behavior)->show_or_hide(width, time);
        }
    }
    /* hsv s 0 over the whole body */
    static void gray(Node* n) {
        if (n->is_canvas_item()) {
            CanvasItem* c = (CanvasItem*)n;
            c->shader_params["s"] = 0;
            if (!c->material) {
                Resource* r = SceneLibrary::current() ? SceneLibrary::current()->ext("materials/vfx/hsv.tres") : NULL;
                if (r && r->kind == Resource::RES_SHADER_MATERIAL) c->material = (MaterialRes*)r;
            }
        }
        for (size_t i = 0; i < n->children.size(); ++i) gray(n->children[i]);
    }
    Vec2 size_;
    Node* body_;
    Node2D* holder_;
    int phase_;
};

/* NDoomOverlayVfx.GetOrCreate: one over the room, fading in (1 s Expo
 * Out), held 1.2 s, out in 0.5 s; asked again while it lasts, it plays
 * again. */
class DoomOverlay : public Effect {
public:
    DoomOverlay() : end_(0) {}
    void play(Node* n) {
        CanvasItem* c = (CanvasItem*)n;
        tweens_.clear();
        tweens_.tween(&c->modulate.a, 1.0f, 1.0f, TRANS_EXPO, EASE_OUT);
        tweens_.tween(&c->modulate.a, 0.0f, 0.5f, TRANS_LINEAR, EASE_IN_OUT, 2.2f);
        end_ = t_ + 2.7;
    }
    virtual void begin(Node* n) {
        ((CanvasItem*)n)->modulate = Color(1, 1, 1, 0);
        play(n);
    }
    virtual void tick(Node* n, double) {
        if (t_ >= end_ && !n->queued_for_deletion()) n->queue_free();
    }

private:
    double end_;
};

ScriptBehavior* make_doom_sub_emitter(Node* n) { return new DoomSubEmitter(n); }
ScriptBehavior* make_doom_vfx(Node*) { return new DoomVfx(); }

/* NMonsterDeathVfx (scenes/vfx/vfx_monster_death.tscn): Viewport, a
 * SubViewport the body is moved into and rendered once; Visual, a Sprite2D
 * of its picture through shaders/dissolve.gdshader; %Particles, the flakes
 * thrown off. */
class MonsterDeath : public Effect {
public:
    MonsterDeath() : anchors_(NULL), delay_(0), end_(-1), visual_(NULL) {}
    void setup(VfxAnchors* anchors, const std::string& who, double delay, const Vec2& padding) {
        anchors_ = anchors;
        who_ = who;
        delay_ = delay;
        padding_ = padding;
    }
    /* nothing shown while the death animation plays (the root stays
     * visible: hidden nodes do not process) */
    virtual void begin(Node* n) { set_visible(n->get_node("Visual"), false); }
    virtual void tick(Node* n, double) {
        if (end_ < 0 && t_ >= delay_) {
            if (!play(n)) {
                n->queue_free();
                end_ = 0;
                return;
            }
        }
        if (end_ >= 0 && t_ >= end_ && !n->queued_for_deletion()) n->queue_free();
    }

private:
    /* The body's bounds (Godot's MegaSkeleton.GetBounds) on screen. */
    static bool spine_bounds(SpineSprite* s, Rect2* out) {
        SpSkeleton* sk = s ? s->skeleton() : NULL;
        if (!sk) return false;
        std::vector<float> v;
        float x0 = 1e9f, y0 = 1e9f, x1 = -1e9f, y1 = -1e9f;
        for (size_t i = 0; i < sk->slots.size(); ++i) {
            const SpSlot& slot = sk->slots[i];
            if (!slot.attachment || !sk->bones[slot.data->bone].active) continue;
            if (slot.attachment->type != SP_ATT_REGION && slot.attachment->type != SP_ATT_MESH) continue;
            sk->world_vertices(slot, v);
            for (size_t k = 0; k + 1 < v.size(); k += 2) {
                x0 = minf(x0, v[k]);
                x1 = maxf(x1, v[k]);
                y0 = minf(y0, v[k + 1]);
                y1 = maxf(y1, v[k + 1]);
            }
        }
        if (x1 < x0 || y1 < y0) return false;
        Xform2D xf = s->global_transform();
        Vec2 c[4] = { xf.xform(Vec2(x0, y0)), xf.xform(Vec2(x1, y0)), xf.xform(Vec2(x1, y1)), xf.xform(Vec2(x0, y1)) };
        Vec2 lo = c[0], hi = c[0];
        for (int k = 1; k < 4; ++k) {
            lo = Vec2(minf(lo.x, c[k].x), minf(lo.y, c[k].y));
            hi = Vec2(maxf(hi.x, c[k].x), maxf(hi.y, c[k].y));
        }
        *out = Rect2(lo, hi - lo);
        return true;
    }
    static SpineSprite* find_spine(Node* n) {
        if (n->type == "SpineSprite") return (SpineSprite*)n;
        for (size_t i = 0; i < n->children.size(); ++i) {
            SpineSprite* s = find_spine(n->children[i]);
            if (s) return s;
        }
        return NULL;
    }
    /* PlayVfx: false when there is nothing to fade. */
    bool play(Node* n) {
        VfxCreature who;
        if (!anchors_ || !anchors_->creature(who_, &who)) return false;
        Node* visuals = anchors_->take_visuals(who_);
        if (!visuals || !visuals->is_canvas_item() || !((CanvasItem*)visuals)->is_visible_in_tree()) return false;
        /* GetCurrentBody: %Visuals, the spine body when there is one */
        Node* body = visuals->find_unique("Visuals");
        SpineSprite* spine = find_spine(body ? body : visuals);
        if (!body) body = spine ? (Node*)spine : visuals;
        if (!body->is_node2d()) return false;
        Rect2 r;
        if (!spine_bounds(spine, &r)) r = who.hitbox;
        Vec2 grown(r.size.x * padding_.x, r.size.y * padding_.y);
        r = Rect2(r.pos - (grown - r.size) * 0.5f, grown);
        Vec2 center = r.pos + r.size * 0.5f;
        float num = floorf(maxf(r.size.x, r.size.y) + 0.5f);
        if (num < 2) return false;
        Node* vpn = n->get_node("Viewport");
        visual_ = n->get_node("Visual");
        if (!vpn || vpn->type != "SubViewport" || !visual_ || visual_->type != "Sprite2D") return false;
        SubViewport* vp = (SubViewport*)vpn;
        set_global(n, center);
        vp->set_size((int)num * 2, (int)num * 2);
        vp->size_2d_override = Vec2(num, num);
        /* the room's scale: the effect's parent's */
        Xform2D pxf = n->parent && n->parent->is_canvas_item() ? ((CanvasItem*)n->parent)->global_transform() : Xform2D();
        float room = sqrtf(pxf.x.x * pxf.x.x + pxf.x.y * pxf.x.y);
        Sprite2D* sprite = (Sprite2D*)visual_;
        sprite->scale = sprite->scale * (1.0f / (2.0f * (room > 0 ? room : 1.0f)));
        /* the body, its global transform kept, at its place in the viewport */
        Node2D* b = (Node2D*)body;
        Xform2D bg = b->global_transform();
        b->parent->remove_child(b);
        vp->add_child(b);
        b->position = bg.origin - (center - Vec2(num, num) * 0.5f);
        b->rotation = 0;
        b->skew = 0;
        float sx = sqrtf(bg.x.x * bg.x.x + bg.x.y * bg.x.y), sy = sqrtf(bg.y.x * bg.y.x + bg.y.y * bg.y.y);
        b->scale = Vec2(bg.x.x < 0 ? -sx : sx, bg.y.y < 0 ? -sy : sy);
        b->visible = true;
        vp->update_mode = SubViewport::UPDATE_ONCE;
        ((CanvasItem*)visuals)->visible = false;
        set_visible(visual_, true);
        /* PlayVfxInternal */
        play_sfx("event:/sfx/enemy/enemy_fade");
        Node* p = n->find_unique("Particles");
        if (is_particles(p)) {
            ((Particles2D*)p)->emission_sphere_radius = (float)vp->height / 4.0f;
            restart(p);
        }
        float time = minf((float)vp->width / (0.1f * 1920.0f) * 2.5f, 2.5f);
        sprite->shader_params["threshold"] = 1.0f;
        tweens_.tween(&sprite->shader_params["threshold"], 0.0f, time, TRANS_SINE, EASE_OUT);
        end_ = t_ + time;
        return true;
    }
    VfxAnchors* anchors_;
    std::string who_;
    double delay_, end_;
    Vec2 padding_;
    Node* visual_;
};

ScriptBehavior* make_monster_death(Node*) { return new MonsterDeath(); }

/* NParticlesContainer.SetEmitting / Restart over its _particles. */
void particles_emitting(Node* container, bool on) {
    std::vector<Node*> list;
    if (container) exported(container, "_particles", list);
    for (size_t i = 0; i < list.size(); ++i)
        if (is_particles(list[i])) {
            if (on) ((Particles2D*)list[i])->restart();
            ((Particles2D*)list[i])->emitting = on;
        }
}

/* NCardExhaustVfx (scenes/vfx/ui/card/vfx_card_exhaust.tscn): the card in
 * _cardParentContainer (a SubViewport's), the SubViewportContainer its
 * picture through vfx_card_exhaust_dissolve_shader; over _exhaustDuration
 * the erosion base and the particles' height follow _exhaustCurve
 * (SetProgress), then the picture goes and, two seconds later, all. */
class CardExhaust : public Effect {
public:
    explicit CardExhaust(Node* n)
        : duration_(exported_float(n, "_exhaustDuration", 0.4f)), card_(NULL), material_(NULL), particles_(NULL),
          done_(false) {
        const Json* v = n->get_extra("_exhaustCurve");
        if (v) CurveData::from(script_resource(*v), curve_);
        erosion_ = exported_vec2(n, "_erosionBaseRange", Vec2(0, 0));
        height_ = exported_vec2(n, "_particleHeightRange", Vec2(0, 0));
    }
    void setup(Node* n, Node* card) {
        card_ = card;
        material_ = node_at(n, "_materialContainer");
        particles_ = node_at(n, "_particlesContainer");
        Node* holder = node_at(n, "_cardParentContainer");
        Vec2 at = global_of(card);
        if (card->parent) card->parent->remove_child(card);
        if (holder) holder->add_child(card);
        set_global(n, at);
        particles_emitting(particles_, false);
        progress(0);
        /* PlayAnimation */
        if (holder && holder->is_control() && card->is_control())
            ((Control*)card)->set_position(((Control*)holder)->size * 0.5f);
        else if (holder && holder->is_control() && card->is_node2d())
            ((Node2D*)card)->position = ((Control*)holder)->size * 0.5f;
        if (material_ && material_->is_canvas_item()) {
            ((CanvasItem*)material_)->self_modulate = Color(1, 1, 1, 1);
            ((CanvasItem*)material_)->shader_params["erosion_texture_x_offset"] = rand_range(0, 1);
        }
        particles_emitting(particles_, true);
    }
    virtual void tick(Node* n, double) {
        if (!done_) {
            if (t_ < duration_) {
                progress((float)(t_ / duration_));
                return;
            }
            progress(1);
            particles_emitting(particles_, false);
            if (material_ && material_->is_canvas_item()) {
                ((CanvasItem*)material_)->self_modulate = Color(1, 1, 1, 0);
                /* nothing of it shows any more: no more renders */
                SubViewportContainer* svc = material_->type == "SubViewportContainer" ? (SubViewportContainer*)material_ : NULL;
                if (svc && svc->viewport()) svc->viewport()->update_mode = SubViewport::UPDATE_DISABLED;
            }
            done_ = true;
            end_ = t_ + 2.0;
        } else if (t_ >= end_ && !n->queued_for_deletion()) {
            n->queue_free();
        }
    }

private:
    static Node* node_at(Node* n, const char* key) {
        const Json* v = n->get_extra(key);
        if (!v) return NULL;
        std::string path = v->is_string() ? v->str() : (*v)["np"].str();
        return path.empty() ? NULL : n->get_node(path);
    }
    static Vec2 exported_vec2(Node* n, const char* key, const Vec2& fallback) {
        const Json* v = n->get_extra(key);
        Vec2 out = fallback;
        if (v) jv_vec2(*v, out);
        return out;
    }
    void progress(float p) {
        float w = curve_.valid() ? curve_.sample(p) : p;
        if (material_ && material_->is_canvas_item())
            ((CanvasItem*)material_)->shader_params["erosion_base"] = erosion_.x + (erosion_.y - erosion_.x) * w;
        if (particles_ && particles_->is_node2d()) ((Node2D*)particles_)->position = Vec2(0, height_.x + (height_.y - height_.x) * w);
    }
    float duration_;
    CurveData curve_;
    Vec2 erosion_, height_;
    Node* card_;
    Node* material_;
    Node* particles_;
    bool done_;
    double end_;
};

ScriptBehavior* make_card_exhaust(Node* n) { return new CardExhaust(n); }

/* NCardExhaustQuickVfx (vfx_card_exhaust_quick.tscn): on the card's
 * CardVfxContainer, the erosion particle over it (masked to the card) for
 * _anticipationDuration; then the effect stays where it is in the room's
 * UI, the card goes, and the smoke and embers play for two seconds. */
class CardExhaustQuick : public Effect {
public:
    explicit CardExhaustQuick(Node* n)
        : anticipation_(exported_float(n, "_anticipationDuration", 0.4f)), card_(NULL), ui_(NULL), phase_(0), end_(0) {}
    void setup(Node* n, Node* card, Node* ui) {
        card_ = card;
        ui_ = ui;
        Node* box = card->find_unique("CardVfxContainer");
        Node* where = box ? box : card;
        where->add_child(n);
        Node* a = CardExhaust_node(n, "_anticipationParticlesContainer");
        if (a && a->is_canvas_item()) ((CanvasItem*)a)->modulate = Color(1, 1, 1, 1);
        restart_list(a, "_particles");
    }
    virtual void tick(Node* n, double) {
        if (phase_ == 0 && t_ >= anticipation_) {
            phase_ = 1;
            Node* a = CardExhaust_node(n, "_anticipationParticlesContainer");
            if (a && a->is_canvas_item()) ((CanvasItem*)a)->modulate = Color(1, 1, 1, 0);
            Xform2D g = ((CanvasItem*)n)->global_transform();
            float sx = sqrtf(g.x.x * g.x.x + g.x.y * g.x.y);
            if (ui_ && n->parent) {
                n->parent->remove_child(n);
                ui_->add_child(n);
                set_global(n, g.origin);
                if (n->is_control()) {
                    Control* c = (Control*)n;
                    c->rotation = atan2f(g.x.y, g.x.x);
                    c->scale = Vec2(sx, sx);
                }
            }
            /* the card goes */
            if (card_ && card_->parent) card_->parent->remove_child(card_);
            delete card_;
            card_ = NULL;
            restart_list(CardExhaust_node(n, "_particlesContainer"), "_particles");
            end_ = t_ + 2.0;
        } else if (phase_ == 1 && t_ >= end_ && !n->queued_for_deletion()) {
            n->queue_free();
        }
    }

private:
    static Node* CardExhaust_node(Node* n, const char* key) {
        const Json* v = n->get_extra(key);
        if (!v) return NULL;
        std::string path = v->is_string() ? v->str() : (*v)["np"].str();
        return path.empty() ? NULL : n->get_node(path);
    }
    float anticipation_;
    Node* card_;
    Node* ui_;
    int phase_;
    double end_;
};

ScriptBehavior* make_card_exhaust_quick(Node* n) { return new CardExhaustQuick(n); }

ScriptBehavior* make_doom_overlay(Node*) { return new DoomOverlay(); }

ScriptBehavior* make_item_throw(Node* n) { return new ItemThrow(n); }
ScriptBehavior* make_small_missile(Node* n) { return new Missile(n, false); }
ScriptBehavior* make_large_missile(Node* n) { return new Missile(n, true); }
ScriptBehavior* make_bg_spike(Node*) { return new GroundSpike(false); }
ScriptBehavior* make_fg_spike(Node*) { return new GroundSpike(true); }
ScriptBehavior* make_kin_priest_grenade(Node*) { return new KinPriestGrenade(); }
ScriptBehavior* make_sleeping(Node*) { return new Sleeping(); }
ScriptBehavior* make_hyperbeam(Node*) { return new Hyperbeam(); }
ScriptBehavior* make_sweeping_beam(Node* n) { return new SweepingBeam(n); }
ScriptBehavior* make_grand_finale(Node*) { return new GrandFinale(); }
ScriptBehavior* make_hellraiser_sword(Node*) { return new HellraiserSword(); }
ScriptBehavior* make_additive_overlay(Node*) { return new AdditiveOverlay(); }

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
                                           "NBolasVfx", "NBigSlashVfx", "NBigSlashImpactVfx", "NHitSparkVfx",
                                           "NItemThrowVfx", "NSmallMagicMissileVfx", "NLargeMagicMissileVfx",
                                           "NSpikeSplashVfx", "NKinPriestGrenadeVfx", "NSleepingVfx", "NHyperbeamVfx",
                                           "NSweepingBeamVfx", "NGrandFinaleVfx", "NHellraiserVfx",
                                           "NAdditiveOverlayVfx", "NDoomVfx", "NDoomOverlayVfx" };

bool is_combat_vfx_call(const std::string& call) {
    if (call == "NCombatRoom.PlaySplashVfx") return true;
    std::string cls = class_of(call);
    if (call == "NSleepingVfx.Stop" || call == "NDoomOverlayVfx.GetOrCreate") return true;
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
    if (call == "NDoomOverlayVfx.GetOrCreate") {
        for (size_t i = 0; i < box->children.size(); ++i) {
            Node* fx = box->children[i];
            if (fx->script == "res://src/Core/Nodes/Vfx/NDoomOverlayVfx.cs" && fx->behavior &&
                !fx->queued_for_deletion()) {
                ((DoomOverlay*)fx->behavior)->play(fx);
                return true;
            }
        }
        Node* fx = add_effect(lib, box, "scenes/vfx/doom_overlay_vfx.tscn");
        if (fx && fx->behavior) ((Effect*)fx->behavior)->start(fx);
        return true;
    }
    if (cls == "NDoomVfx") {
        /* Create(visuals, Hitbox.GlobalPosition, Hitbox.Size, shouldDie) */
        if (a.places.size() < 2) return true;
        VfxCreature who;
        if (!anchors.creature(a.places[0].who, &who)) return true;
        Node* fx = add_effect(lib, box, "scenes/vfx/vfx_doom.tscn");
        if (!fx) return true;
        set_global(fx, who.hitbox.pos + Vec2(who.hitbox.size.x * 0.5f, who.hitbox.size.y));
        bool die = !a.bools.empty() && a.bools[0];
        if (fx->behavior) {
            ((DoomVfx*)fx->behavior)->setup(who.hitbox.size, die ? anchors.take_visuals(a.places[0].who) : NULL, die);
            ((Effect*)fx->behavior)->start(fx);
        }
        return true;
    }
    if (call == "NSleepingVfx.Stop") {
        /* the monster's own effect: every one still sleeping wakes */
        for (size_t i = 0; i < box->children.size(); ++i) {
            Node* fx = box->children[i];
            if (fx->script == "res://src/Core/Nodes/Vfx/NSleepingVfx.cs" && fx->behavior) ((Sleeping*)fx->behavior)->stop();
        }
        return true;
    }
    if (cls == "NItemThrowVfx" || cls == "NSmallMagicMissileVfx" || cls == "NLargeMagicMissileVfx" ||
        cls == "NSleepingVfx") {
        /* Create(Vector2, ...): the positions the logic's stand-in nodes gave */
        std::vector<Vec2> at;
        for (size_t i = 0; i < a.places.size(); ++i) {
            Vec2 p;
            if (resolve(a.places[i], anchors, &p)) at.push_back(p);
        }
        if (at.empty()) return true;
        if (cls == "NItemThrowVfx") {
            if (at.size() < 2) return true;
            Node* fx = add_effect(lib, box, "scenes/vfx/vfx_item_throw.tscn");
            if (!fx || !fx->behavior) return true;
            Resource* r = a.paths.empty() ? NULL : lib->ext(a.paths[0]);
            ((ItemThrow*)fx->behavior)->setup(at[0], at[1], r && r->is_texture() ? (Texture2DRes*)r : NULL);
            ((Effect*)fx->behavior)->start(fx);
        } else if (cls == "NSleepingVfx") {
            Node* fx = add_effect(lib, box, "scenes/vfx/vfx_sleeping.tscn");
            if (!fx) return true;
            set_global(fx, at[0]);
            if (fx->behavior) {
                ((Sleeping*)fx->behavior)->setup(a.bools.empty() || a.bools[0]);
                ((Effect*)fx->behavior)->start(fx);
            }
        } else {
            bool large = cls == "NLargeMagicMissileVfx";
            Node* fx = add_effect(lib, box, large ? "scenes/vfx/vfx_large_magic_missile.tscn"
                                                   : "scenes/vfx/vfx_small_magic_missile.tscn");
            if (!fx) return true;
            set_global(fx, at[0]);
            Color tint = a.colors.empty() ? Color(1, 1, 1, 1) : a.colors[0];
            std::vector<Node*> list;
            exported(fx, "_modulateParticles", list);
            for (size_t i = 0; i < list.size(); ++i)
                if (list[i]->is_canvas_item()) ((CanvasItem*)list[i])->self_modulate = tint;
            if (fx->behavior) ((Effect*)fx->behavior)->start(fx);
        }
        return true;
    }
    if (cls == "NAdditiveOverlayVfx") {
        Node* fx = add_effect(lib, box, "scenes/vfx/additive_overlay_vfx.tscn");
        if (fx && fx->behavior) ((Effect*)fx->behavior)->start(fx);
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
    if (cls == "NSpikeSplashVfx") {
        /* six spikes each way in front, six each way behind; 2 s */
        std::string color = a.words.empty() ? "Red" : a.words[0];
        for (int i = 0; i < 6; ++i)
            for (int layer = 0; layer < 2; ++layer)
                for (int dir = 0; dir < 2; ++dir) {
                    Node* parent = layer == 0 ? box : (anchors.back_box() ? anchors.back_box() : box);
                    Node* fx = add_effect(lib, parent, layer == 0 ? "scenes/vfx/fg_ground_spike_vfx.tscn"
                                                                  : "scenes/vfx/bg_ground_spike_vfx.tscn");
                    if (!fx || !fx->behavior) continue;
                    ((GroundSpike*)fx->behavior)->setup(c.bottom, dir == 0, color);
                    ((Effect*)fx->behavior)->start(fx);
                }
        return true;
    }
    if (cls == "NKinPriestGrenadeVfx") {
        Node* fx = add_effect(lib, box, "scenes/vfx/monsters/kin_priest_grenade_vfx.tscn");
        if (!fx) return true;
        set_global(fx, c.bottom);
        if (fx->behavior) ((Effect*)fx->behavior)->start(fx);
        return true;
    }
    if (cls == "NHyperbeamVfx" || cls == "NSweepingBeamVfx") {
        Vec2 eye = c.center + (c.defect ? Vec2(34, -30) : Vec2(0, 0));
        if (cls == "NHyperbeamVfx") {
            VfxCreature target;
            if (a.creatures.size() < 2 || !anchors.creature(a.creatures[1], &target)) return true;
            Node* fx = add_effect(lib, box, "scenes/vfx/vfx_hyperbeam.tscn");
            if (!fx || !fx->is_node2d()) return true;
            set_global(fx, eye);
            Vec2 d = target.center - eye;
            ((Node2D*)fx)->rotation = atan2f(d.y, d.x);
            if (fx->behavior) ((Effect*)fx->behavior)->start(fx);
        } else {
            std::vector<Vec2> targets;
            for (size_t l = 0; l < a.lists.size(); ++l)
                for (size_t k = 0; k < a.lists[l].size(); ++k) {
                    VfxCreature t;
                    if (anchors.creature(a.lists[l][k], &t)) targets.push_back(t.center);
                }
            Node* fx = add_effect(lib, box, "scenes/vfx/vfx_sweeping_beam.tscn");
            if (!fx) return true;
            set_global(fx, eye);
            if (fx->behavior) {
                ((SweepingBeam*)fx->behavior)->setup(targets, box);
                ((Effect*)fx->behavior)->start(fx);
            }
        }
        return true;
    }
    if (cls == "NGrandFinaleVfx") {
        Node* fx = add_effect(lib, box, "scenes/vfx/vfx_grand_finale.tscn");
        if (fx && fx->behavior) {
            ((GrandFinale*)fx->behavior)->setup(c.center);
            ((Effect*)fx->behavior)->start(fx);
        }
        return true;
    }
    if (cls == "NHellraiserVfx") {
        spawn_hellraiser(lib, box, anchors.back_box(), c.bottom);
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

void spawn_monster_death(SceneLibrary* lib, Node* parent, VfxAnchors& anchors, const std::string& who, double delay,
                         const Vec2& padding) {
    if (!lib || !parent) return;
    Node* fx = add_effect(lib, parent, "scenes/vfx/vfx_monster_death.tscn");
    if (!fx) return;
    if (fx->behavior) ((MonsterDeath*)fx->behavior)->setup(&anchors, who, delay, padding);
}

void spawn_card_exhaust(SceneLibrary* lib, Node* ui, Node* card, bool quick) {
    if (!card) return;
    Node* fx = lib && ui ? instance(lib, quick ? "scenes/vfx/ui/card/vfx_card_exhaust_quick.tscn"
                                               : "scenes/vfx/ui/card/vfx_card_exhaust.tscn")
                         : NULL;
    if (!fx || !fx->behavior) {
        delete fx;
        if (card->parent) card->parent->remove_child(card);
        delete card;
        return;
    }
    if (quick) {
        ((CardExhaustQuick*)fx->behavior)->setup(fx, card, ui);
    } else {
        ui->add_child(fx);
        ((CardExhaust*)fx->behavior)->setup(fx, card);
    }
    if (lib->audio()) lib->audio()->ui("card_exhaust.mp3");
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
    register_script("res://src/Core/Nodes/Vfx/NItemThrowVfx.cs", make_item_throw);
    register_script("res://src/Core/Nodes/Vfx/NSmallMagicMissileVfx.cs", make_small_missile);
    register_script("res://src/Core/Nodes/Vfx/NLargeMagicMissileVfx.cs", make_large_missile);
    register_script("res://src/Core/Nodes/Vfx/NBgGroundSpikeVfx.cs", make_bg_spike);
    register_script("res://src/Core/Nodes/Vfx/NFgGroundSpikeVfx.cs", make_fg_spike);
    register_script("res://src/Core/Nodes/Vfx/NKinPriestGrenadeVfx.cs", make_kin_priest_grenade);
    register_script("res://src/Core/Nodes/Vfx/NSleepingVfx.cs", make_sleeping);
    register_script("res://src/Core/Nodes/Vfx/NHyperbeamVfx.cs", make_hyperbeam);
    register_script("res://src/Core/Nodes/Vfx/NSweepingBeamVfx.cs", make_sweeping_beam);
    register_script("res://src/Core/Nodes/Vfx/NGrandFinaleVfx.cs", make_grand_finale);
    register_script("res://src/Core/Nodes/Vfx/Cards/NHellraiserSwordVfx.cs", make_hellraiser_sword);
    register_script("res://src/Core/Nodes/Vfx/NAdditiveOverlayVfx.cs", make_additive_overlay);
    register_script("res://src/Core/Nodes/Vfx/NDoomSubEmitterVfx.cs", make_doom_sub_emitter);
    register_script("res://src/Core/Nodes/Vfx/NDoomVfx.cs", make_doom_vfx);
    register_script("res://src/Core/Nodes/Vfx/NDoomOverlayVfx.cs", make_doom_overlay);
    register_script("res://src/Core/Nodes/Vfx/NMonsterDeathVfx.cs", make_monster_death);
    register_script("res://src/Core/Nodes/Vfx/Cards/NCardExhaustVfx.cs", make_card_exhaust);
    register_script("res://src/Core/Nodes/Vfx/Cards/NCardExhaustQuickVfx.cs", make_card_exhaust_quick);
}

} /* namespace spire */

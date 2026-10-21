#include "sovereign_blade.h"
#include "ui_util.h"

#include "../scene/controls.h"
#include "../scene/line_2d.h"
#include "../scene/node.h"
#include "../scene/particles.h"
#include "../scene/path_2d.h"
#include "../scene/scene.h"
#include "../scene/script.h"
#include "../scene/spine_sprite.h"
#include "../scene/tween.h"

#include <math.h>

namespace spire {

namespace {

const char* const SCRIPT = "res://src/Core/Nodes/Vfx/NSovereignBladeVfx.cs";
const char* const CONTAINER = "SpineSword/SwordBone/ScaleContainer/";

Particles2D* particles(Node* root, const std::string& path) {
    Node* n = root->get_node(path);
    return n && (n->type == "GPUParticles2D" || n->type == "CPUParticles2D") ? (Particles2D*)n : NULL;
}

Vec2 global_of(Node* n) { return n && n->is_canvas_item() ? ((CanvasItem*)n)->global_transform().origin : Vec2(0, 0); }

float clamp01(float v) { return v < 0 ? 0 : v > 1 ? 1 : v; }

class Blade : public ScriptBehavior {
public:
    explicit Blade(Node*)
        : id(-1), orbit(0), spine_(NULL), path_(NULL), trail_(NULL), glow_(NULL), size_(0), behind_(false),
          attacking_(false), t_(0), sparks_at_(-1), slash_at_(-1), sparks2_at_(-1), cleanup_at_(-1), free_at_(-1) {}
    int id;        /* the logic's card instance */
    double orbit;  /* OrbitProgress */

    virtual void ready(Node* n) {
        Node* s = n->get_node("SpineSword");
        spine_ = s && s->type == "SpineSprite" ? (SpineSprite*)s : NULL;
        Node* p = n->find_unique("Path");
        path_ = p && p->type == "Path2D" ? (Path2D*)p : NULL;
        Node* t = n->get_node("Trail");
        trail_ = t && t->type == "Line2D" ? (Line2D*)t : NULL;
        Node* g = n->get_node(std::string(CONTAINER) + "BladeGlow");
        glow_ = g && g->is_canvas_item() ? (CanvasItem*)g : NULL;
        static const char* const ONE_SHOTS[] = { "ForgeSparks", "SpawnFlames", "SpawnFlamesBack" };
        for (int i = 0; i < 3; ++i)
            if (Particles2D* q = particles(n, std::string(CONTAINER) + ONE_SHOTS[i])) {
                q->emitting = false;
                q->one_shot = true;
            }
        if (Particles2D* q = particles(n, "SpineSword/SlashParticles")) {
            q->emitting = false;
            q->one_shot = true;
        }
        static const char* const OFF[] = { "ChargeParticles", "Spikes2", "SpikeCircle2" };
        for (int i = 0; i < 3; ++i)
            if (Particles2D* q = particles(n, std::string(CONTAINER) + OFF[i])) q->emitting = false;
        if (glow_) {
            glow_->modulate = Color(0, 0, 0, 0);
            glow_->visible = false;
        }
        if (trail_) trail_->points.clear();
        hide(n->find_unique("SelectionReticle"));
        if (spine_) {
            spine_->set_animation("idle_loop", true, 0);
            spine_->scale = Vec2(0, 0);
            spine_->visible = true;
        }
    }

    virtual void process(Node* n, double dt) {
        float k = (float)dt;
        t_ += dt;
        tweens_.update(dt);
        Node2D* root = (Node2D*)n;
        if (trail_) trail_->position = to_local(root, Vec2(0, 0)); /* its points are global */
        if (sparks_at_ >= 0 && t_ >= sparks_at_) {
            sparks_at_ = -1;
            fire(n, "ForgeSparks");
        }
        if (slash_at_ >= 0 && t_ >= slash_at_) {
            slash_at_ = -1;
            end_slash(n);
        }
        if (sparks2_at_ >= 0 && t_ >= sparks2_at_) {
            sparks2_at_ = -1;
            fire(n, "ForgeSparks");
        }
        if (cleanup_at_ >= 0 && t_ >= cleanup_at_) {
            cleanup_at_ = -1;
            cleanup_attack();
        }
        if (free_at_ >= 0 && t_ >= free_at_ && !n->queued_for_deletion()) n->queue_free();
        if (!spine_ || !path_) return;
        /* _Process: round the orbit at 60 px/s, behind the player on its far
         * side (while small), drifting out front as it grows */
        float len = path_->baked_length();
        if (len <= 0) return;
        orbit += 60.0 * dt / len;
        double u = fmod(orbit, 1.0);
        bool behind = u > 0.25 && u < 0.78;
        if (behind != behind_ && size_ < 0.6f && n->parent) {
            behind_ = behind;
            Node* parent = n->parent;
            parent->remove_child(n);
            if (behind && !parent->children.empty()) parent->add_child_before(n, parent->children[0]);
            else parent->add_child(n);
        }
        float rot = 0;
        Vec2 v = path_->global_transform().xform(path_->sample_baked((float)u * len, &rot));
        Vec2 origin = global_of(n);
        v.x = v.x + (origin.x + 200 - v.x) * clamp01(size_ / 1.25f);
        Vec2 target = v + Vec2(0, -1) * ((spine_->scale.y - 1) * 100);
        if (!attacking_) {
            Vec2 g = global_of(spine_);
            spine_->position = to_local(root, g + (target - g) * (k * 7 < 1 ? k * 7 : 1));
        }
    }

    /* Forge(bladeDamage, showFlames) */
    void forge(Node* n, float damage, bool flames) {
        size_ = clamp01(damage / 200);
        int amount = (int)(size_ * 30);
        if (Particles2D* c = particles(n, std::string(CONTAINER) + "ChargeParticles")) {
            if (amount > 0) {
                c->amount = amount;
                c->emitting = true;
            } else {
                c->emitting = false;
            }
        }
        bool hilt = size_ < 0.3f;
        set_visible(n->get_node(std::string(CONTAINER) + "Hilt"), hilt);
        set_visible(n->get_node(std::string(CONTAINER) + "Hilt2"), !hilt);
        static const char* const SMALL[] = { "Spikes", "SpikeCircle" };
        static const char* const BIG[] = { "Spikes2", "SpikeCircle2" };
        for (int i = 0; i < 2; ++i) {
            if (Particles2D* q = particles(n, std::string(CONTAINER) + SMALL[i])) q->visible = q->emitting = hilt;
            if (Particles2D* q = particles(n, std::string(CONTAINER) + BIG[i])) q->visible = q->emitting = !hilt;
        }
        /* the game compares the damage (not the size) with 0.66 */
        set_visible(n->get_node(std::string(CONTAINER) + "Detail"), damage >= 0.66f);
        if (flames) {
            fire(n, "SpawnFlames");
            fire(n, "SpawnFlamesBack");
        }
        if (glow_) {
            /* #ff7300 in 0.05 s (Out), then clear in 0.5 s (Cubic In) */
            glow_->visible = true;
            Color orange = hex(0xFF7300);
            tweens_.tween(&glow_->modulate.r, orange.r, 0.05f, TRANS_LINEAR, EASE_OUT);
            tweens_.tween(&glow_->modulate.g, orange.g, 0.05f, TRANS_LINEAR, EASE_OUT);
            tweens_.tween(&glow_->modulate.b, orange.b, 0.05f, TRANS_LINEAR, EASE_OUT);
            tweens_.tween(&glow_->modulate.a, 1, 0.05f, TRANS_LINEAR, EASE_OUT);
            tweens_.tween(&glow_->modulate.a, 0, 0.5f, TRANS_CUBIC, EASE_IN, 0.05f);
        }
        if (spine_) {
            /* to 1.2x its size (0.05 s), sparks, then its size (0.3 s) */
            float s = 0.9f + (2 - 0.9f) * size_;
            tweens_.kill(&spine_->scale);
            tweens_.tween(&spine_->scale, Vec2(s * 1.2f, s * 1.2f), 0.05f, TRANS_CUBIC, EASE_OUT);
            tweens_.tween(&spine_->scale, Vec2(s, s), 0.3f, TRANS_CUBIC, EASE_IN_OUT, 0.05f);
            sparks_at_ = t_ + 0.05;
        }
    }

    /* Attack(targetPos): drawn back 50 px and turned (0.05 / 0.08 s), then
     * at the target (0.05 s, Expo In), the slash; it returns 0.55 s later */
    void attack(Node* n, const Vec2& target) {
        if (!spine_) return;
        if (attacking_) cleanup_attack();
        attacking_ = true;
        spine_->set_animation("attack", false, 0);
        Node2D* root = (Node2D*)n;
        Vec2 g = global_of(spine_);
        Vec2 back(g.x - 50, g.y);
        trail_start_ = back;
        if (trail_) trail_->visible = true;
        float angle = atan2f(target.y - g.y, target.x - g.x);
        tweens_.tween(&spine_->rotation, angle, 0.05f);
        tweens_.tween(&spine_->position, to_local(root, back), 0.08f, TRANS_EXPO, EASE_OUT);
        tweens_.tween(&spine_->position, to_local(root, target), 0.05f, TRANS_EXPO, EASE_IN, 0.08f);
        slash_at_ = t_ + 0.13;
        sparks2_at_ = t_ + 0.68;
        cleanup_at_ = t_ + 0.68;
    }

    /* RemoveSovereignBlade: shrinks to nothing (0.2 s, Cubic Out), gone */
    void remove() {
        if (!spine_) return;
        tweens_.kill(&spine_->scale);
        tweens_.tween(&spine_->scale, Vec2(0, 0), 0.2f, TRANS_CUBIC, EASE_OUT);
        free_at_ = t_ + 0.2;
    }

private:
    static Vec2 to_local(Node2D* root, const Vec2& g) { return inverse(root->global_transform()).xform(g); }
    static void set_visible(Node* n, bool on) {
        if (n && n->is_canvas_item()) ((CanvasItem*)n)->visible = on;
    }
    static void fire(Node* n, const char* name) {
        if (Particles2D* q = particles(n, std::string(CONTAINER) + name)) q->restart();
    }
    /* EndSlash: the charge restarts, the slash particles burst along the
     * stroke and the trail is drawn from where it struck from, fading */
    void end_slash(Node* n) {
        if (Particles2D* c = particles(n, std::string(CONTAINER) + "ChargeParticles")) {
            c->emitting = false;
            c->restart();
        }
        Vec2 g = global_of(spine_);
        if (Particles2D* s = particles(n, "SpineSword/SlashParticles")) {
            s->rotation = atan2f(trail_start_.y - g.y, trail_start_.x - g.x) - 1.5708f;
            s->restart();
        }
        if (trail_) {
            trail_->points.clear();
            trail_->points.push_back(trail_start_);
            trail_->points.push_back(global_of(n->get_node(std::string(CONTAINER) + "SpikeCircle")));
            trail_->modulate.a = 1;
            tweens_.tween(&trail_->modulate.a, 0, 0.2f);
        }
    }
    void cleanup_attack() {
        attacking_ = false;
        if (spine_) {
            spine_->set_animation("idle_loop", true, 0);
            tweens_.kill(&spine_->rotation);
            tweens_.kill(&spine_->position);
            spine_->rotation = 0;
        }
        if (trail_) trail_->points.clear();
    }

    TweenSet tweens_;
    SpineSprite* spine_;
    Path2D* path_;
    Line2D* trail_;
    CanvasItem* glow_;
    float size_;
    bool behind_, attacking_;
    double t_, sparks_at_, slash_at_, sparks2_at_, cleanup_at_, free_at_;
    Vec2 trail_start_;
};

ScriptBehavior* make_blade(Node* n) { return new Blade(n); }

Node* find_blade(Node* player, int id) {
    for (size_t i = 0; i < player->children.size(); ++i) {
        Node* c = player->children[i];
        if (c->script == SCRIPT && c->behavior && !c->queued_for_deletion() && ((Blade*)c->behavior)->id == id) return c;
    }
    return NULL;
}

} /* namespace */

void sovereign_blade_cue(SceneLibrary* lib, Node* player, const Json& cue, const Vec2* target) {
    if (!player) return;
    const std::string& op = cue["op"].str();
    int id = cue["card"].i(-1);
    Node* blade = find_blade(player, id);
    if (op == "forge") {
        if (!blade) {
            /* PlayCombatRoomForgeVfx: a new one under the creature at (0, 0) */
            blade = instance(lib, "scenes/vfx/sovereign_blade.tscn");
            if (!blade || !blade->is_node2d() || blade->script != SCRIPT || !blade->behavior) {
                delete blade;
                return;
            }
            ((Blade*)blade->behavior)->id = id;
            player->add_child(blade);
        }
        ((Blade*)blade->behavior)->forge(blade, (float)cue["damage"].num(), cue["flames"].b());
    } else if (blade && op == "orbit") {
        ((Blade*)blade->behavior)->orbit = cue["progress"].num();
    } else if (blade && op == "attack" && target) {
        ((Blade*)blade->behavior)->attack(blade, *target);
    } else if (blade && op == "remove") {
        ((Blade*)blade->behavior)->remove();
    }
}

void install_sovereign_blade_script() { register_script(SCRIPT, make_blade); }

} /* namespace spire */

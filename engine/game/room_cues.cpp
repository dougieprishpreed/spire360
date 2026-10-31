#include "room_cues.h"
#include "combat_vfx.h"
#include "../core/cue_ledger.h"
#include "screen_shake.h"
#include "top_bar.h"
#include "ui_util.h"

#include "../scene/controls.h"
#include "../scene/node.h"
#include "../scene/particles.h"
#include "../scene/scene.h"
#include "../scene/script.h"
#include "../scene/tween.h"
#include "../scene/tween.h"
#include "loc.h"

#include <math.h>

namespace spire {

namespace {

/* No creature nodes outside a fight. */
class NoCreatures : public VfxAnchors {
public:
    virtual bool creature(const std::string&, VfxCreature*) { return false; }
    virtual bool side_center(bool, Vec2*) { return false; }
    virtual Node* back_box() { return NULL; }
    virtual Node* take_visuals(const std::string&) { return NULL; }
};

/* NSmokyVignetteVfx.Animate(fadeIn): the vignette to its tint's alpha and
 * its highlights to theirs in 0.1 s, then both out over 1 s (the vignette
 * Cubic In), then it goes. */
class SmokyVignette : public ScriptBehavior {
public:
    SmokyVignette(CanvasItem* self, CanvasItem* highlights, float alpha, float highlight_alpha) : left_(1.1) {
        tweens_.tween_from(&self->modulate.a, 0, alpha, 0.1f);
        tweens_.tween_from(&self->modulate.a, alpha, 0, 1.0f, TRANS_CUBIC, EASE_IN, 0.1f);
        if (highlights) {
            tweens_.tween_from(&highlights->modulate.a, 0, highlight_alpha, 0.1f);
            tweens_.tween_from(&highlights->modulate.a, highlight_alpha, 0, 1.0f, TRANS_LINEAR, EASE_IN_OUT, 0.1f);
        }
    }
    virtual void process(Node* n, double dt) {
        tweens_.update(dt);
        left_ -= dt;
        if (left_ <= 0 && !n->queued_for_deletion()) n->queue_free();
    }

private:
    TweenSet tweens_;
    double left_;
};

/* NRelicInventoryHolder.DoFlash once the new top bar is laid out. */
class HolderFlashLater : public ScriptBehavior {
public:
    HolderFlashLater(SceneLibrary* lib, Control* holder) : lib_(lib), holder_(holder), frames_(0) {}
    virtual void process(Node* n, double) {
        if (n->queued_for_deletion() || ++frames_ < 2) return;
        Node* fx = make_relic_holder_flash(lib_, holder_);
        if (fx && n->parent) n->parent->add_child(fx);
        else delete fx;
        n->queue_free();
    }

private:
    SceneLibrary* lib_;
    Control* holder_;
    int frames_;
};

/* NTopBarHp.LerpAtNeow: "0/max", then after 0.5 s the HP counting up to
 * the player's (Cubic InOut, 1 s). */
void add_under_bar(Node* screen, Node* fx);

/* A node added to its parent after its moment in the steps (their waits,
 * as GameAudio schedules the sounds); deleted with the holder if never. */
class AddLater : public ScriptBehavior {
public:
    AddLater(Node* fx, double delay) : fx_(fx), delay_(delay) {}
    ~AddLater() { delete fx_; }
    virtual void process(Node* n, double dt) {
        if (!fx_ || n->queued_for_deletion()) return;
        delay_ -= dt;
        if (delay_ > 0) return;
        if (n->parent) add_under_bar(n->parent, fx_);
        else delete fx_;
        fx_ = NULL;
        n->queue_free();
    }

private:
    Node* fx_;
    double delay_;
};

class HpLerp : public ScriptBehavior {
public:
    HpLerp(Node* label, int hp, int max) : label_(label), hp_(hp), max_(max), t_(0) {}
    virtual void process(Node* n, double dt) {
        if (n->queued_for_deletion()) return;
        t_ += dt;
        float u = t_ < 0.5 ? 0.0f : tween_interpolate(TRANS_CUBIC, EASE_IN_OUT, (float)(t_ - 0.5 < 1 ? t_ - 0.5 : 1), 0, 1, 1);
        set_label(label_, format("%d/%d", (int)(hp_ * u), max_));
        if (t_ >= 1.5) n->queue_free();
    }

private:
    Node* label_;
    int hp_, max_;
    double t_;
};

/* The room's effects go under the top bar (NEventRoom.VfxContainer,
 * NMerchantRoom, NRestSiteRoom). */
void add_under_bar(Node* screen, Node* fx) {
    Node* bar = screen->get_node("TopBarLayer");
    if (bar) screen->add_child_before(fx, bar);
    else screen->add_child(fx);
}

/* PlayerFullscreenHealVfx.Play(player, healAmount, vfxContainer). */
void heal_vfx(SceneLibrary* lib, Node* screen, const Json& player, double amount) {
    double max_hp = player["max_hp"].num();
    if (amount < 1 || max_hp <= 0) return;
    float k = (float)(amount / max_hp);
    if (k > 1) k = 1;
    float e = 1 - (1 - k) * (1 - k); /* Ease.QuadOut */
    /* NSmokyVignetteVfx.Create(StsColors.green with alpha max(e * 0.8, 0.4),
     * (0, 1, 0, 0.33)) */
    Color green = hex(0x7FFF00);
    green.a = e * 0.8f > 0.4f ? e * 0.8f : 0.4f;
    spawn_smoky_vignette(lib, screen, green, Color(0, 1, 0, 0.33f), true);
    /* The crosses over the whole viewport: e * 40 + 10 of them, rounded. */
    Node* fx = instance(lib, "scenes/vfx/vfx_cross_heal_fullscreen.tscn");
    if (!fx || !fx->is_node2d()) {
        delete fx;
        return;
    }
    ((Node2D*)fx)->position = Vec2(960, 540);
    Node* beam = fx->get_node("beam");
    if (beam && (beam->type == "GPUParticles2D" || beam->type == "CPUParticles2D")) {
        Particles2D* p = (Particles2D*)beam;
        p->emission_rect_extents = Vec2(960, 540);
        p->amount = (int)(e * 40 + 10 + 0.5f);
    }
    add_under_bar(screen, fx);
}

/* NAncientEventLayout.PlayHealVfxAfterFadeIn: the heal 0.2 s after the
 * layout's first show (on the screen's root). */
class HealLater : public ScriptBehavior {
public:
    HealLater(SceneLibrary* lib, const Json& player, double amount) : lib_(lib), player_(player), amount_(amount), left_(0.2) {}
    virtual void process(Node* n, double dt) {
        if (n->queued_for_deletion()) return;
        left_ -= dt;
        if (left_ > 0) return;
        if (n->parent) heal_vfx(lib_, n->parent, player_, amount_);
        n->queue_free();
    }

private:
    SceneLibrary* lib_;
    Json player_;
    double amount_;
    double left_;
};

} /* namespace */

bool add_room_vfx(SceneLibrary* lib, Node* screen, const std::string& call) {
    const char* scene = call == "NMirrorVfx.Create" ? "scenes/vfx/whole_screen/mirror_vfx.tscn"
                      : call == "NRainVfx.Create"   ? "scenes/vfx/whole_screen/vfx_rain.tscn"
                                                    : NULL;
    if (!scene) return false;
    Node* fx = instance(lib, scene);
    if (fx) add_under_bar(screen, fx);
    return true;
}

namespace {

const char* const GAIN_EPOCH_SCRIPT = "res://src/Core/Nodes/Vfx/NGainEpochVfx.cs";

/* NGainEpochVfx.AnimateVfx: after 3 s for each toast before it still up,
 * %EpochContainer turns from -30 degrees and slides to x 164 (0.5 s, Back
 * Out), stays 1.5 s, the whole fades (1 s) and goes. */
class GainEpoch : public ScriptBehavior {
public:
    explicit GainEpoch(double delay) : delay_(delay), t_(0), started_(false) {}
    virtual void process(Node* n, double dt) {
        if (delay_ > 0) {
            delay_ -= dt;
            return;
        }
        Node* c = n->find_unique("EpochContainer");
        Control* box = c && c->is_control() ? (Control*)c : NULL;
        if (!started_) {
            started_ = true;
            if (box) {
                box->rotation = -30.0f * 0.017453293f;
                tweens_.tween(&box->rotation, 0.0f, 0.5f, TRANS_BACK, EASE_OUT);
                tweens_.tween(&box->offset[SIDE_LEFT], 164.0f, 0.5f, TRANS_BACK, EASE_OUT);
                tweens_.tween(&box->offset[SIDE_RIGHT], 164.0f, 0.5f, TRANS_BACK, EASE_OUT);
            }
            if (n->is_canvas_item()) tweens_.tween(&((CanvasItem*)n)->modulate.a, 0.0f, 1.0f, TRANS_LINEAR, EASE_IN_OUT, 2.0f);
        }
        tweens_.update(dt);
        t_ += dt;
        if (t_ >= 3.0 && !n->queued_for_deletion()) n->queue_free();
    }

private:
    double delay_, t_;
    bool started_;
    TweenSet tweens_;
};

/* ModelDb's id of a model's class: words split at a lower case letter or
 * digit before a capital, and before a capital starting a word
 * ("Act2BEpoch" -> "ACT2_B_EPOCH"). */
std::string model_id(const std::string& cls) {
    std::string out;
    for (size_t i = 0; i < cls.size(); ++i) {
        char c = cls[i];
        bool upper = c >= 'A' && c <= 'Z';
        if (upper && i > 0) {
            char p = cls[i - 1];
            bool after_word = (p >= 'a' && p <= 'z') || (p >= '0' && p <= '9');
            bool starts_word = p >= 'A' && p <= 'Z' && i + 1 < cls.size() && cls[i + 1] >= 'a' && cls[i + 1] <= 'z';
            if (after_word || starts_word) out += '_';
        }
        out += (char)(c >= 'a' && c <= 'z' ? c - 'a' + 'A' : c);
    }
    return out;
}

} /* namespace */

void spawn_smoky_vignette(SceneLibrary* lib, Node* parent, const Color& tint, const Color& highlight, bool under_bar) {
    if (!parent) return;
    Node* v = instance(lib, "scenes/vfx/whole_screen/vfx_smoky_vignette.tscn");
    if (!v || !v->is_canvas_item()) {
        delete v;
        return;
    }
    CanvasItem* c = (CanvasItem*)v;
    c->modulate = Color(tint.r, tint.g, tint.b, 0);
    Node* h = v->get_node("Highlights");
    CanvasItem* hc = h && h->is_canvas_item() ? (CanvasItem*)h : NULL;
    if (hc) hc->modulate = Color(highlight.r, highlight.g, highlight.b, 0);
    delete v->behavior;
    v->behavior = new SmokyVignette(c, hc, tint.a, highlight.a);
    if (under_bar) add_under_bar(parent, v);
    else parent->add_child(v);
}

void show_epoch_gain(SceneLibrary* lib, Node* parent, const std::string& epoch) {
    if (!parent || epoch.empty()) return;
    int before = 0;
    for (size_t i = 0; i < parent->children.size(); ++i)
        if (parent->children[i]->script == GAIN_EPOCH_SCRIPT && !parent->children[i]->queued_for_deletion()) ++before;
    Node* fx = instance(lib, "scenes/vfx/vfx_gain_epoch.tscn");
    if (!fx) return;
    set_texture(lib, fx->find_unique("Portrait"), "images/atlases/epoch_atlas.sprites/" + lower(epoch) + ".tres");
    set_label(fx->find_unique("Label"), Loc::get("vfx", "EPOCH_GAIN"));
    fx->behavior = new GainEpoch(3.0 * before);
    parent->add_child(fx);
}

void play_global_cues(SceneLibrary* lib, Node* layer, const Json& steps) {
    for (size_t i = 0; i < steps.size(); ++i)
        for (size_t k = 0; k < steps[i].size(); ++k) {
            const Json& c = steps[i][k];
            if (c["t"].str() == "ui" && c["call"].str() == "NGainEpochVfx.Create" && c["args"].size() > 0) {
                show_epoch_gain(lib, layer, model_id(c["args"][(size_t)0].str()));
                CueLedger::used(c);
            }
        }
}

void play_room_cues(SceneLibrary* lib, Node* screen, const Json& d) {
    const Json& steps = d["steps"];
    /* an event's combat room plays its own cues (EventScreen) */
    bool room = d["layout"].str() == "Combat";
    /* an ancient's heal as its room opens (the logic sends it once) */
    if (d["ancient"]["healed"].num() >= 1) {
        Node* later = SceneLibrary::create_node("Node");
        later->name = "HealLater";
        later->behavior = new HealLater(lib, d["player"], d["ancient"]["healed"].num());
        screen->add_child(later);
    }
    NoCreatures anchors;
    double at = 0; /* the steps' time: their waits */
    for (size_t i = 0; i < steps.size(); ++i)
        for (size_t k = 0; k < steps[i].size(); ++k) {
            const Json& c = steps[i][k];
            const std::string& t = c["t"].str();
            if (t == "wait") at += c["ms"].num() / 1000.0;
            if (t == "relic_flash" && !room) {
                Node* bar = screen->get_node("TopBarLayer");
                Node* row = bar ? find_named(bar, "RelicInventory") : NULL;
                int index = c["index"].i(-1);
                if (!row || index < 0 || index >= (int)row->children.size() || !row->children[(size_t)index]->is_control()) continue;
                Node* later = SceneLibrary::create_node("Node");
                later->name = "RelicFlash";
                later->behavior = new HolderFlashLater(lib, (Control*)row->children[(size_t)index]);
                bar->add_child(later);
                CueLedger::used(c);
                continue;
            }
            if (t != "ui") continue;
            const std::string& call = c["call"].str();
            const Json& args = c["args"];
            if (room && call != "PlayerFullscreenHealVfx.Play") continue;
            if (ScreenShake::get().cue(call, args)) {
                /* NGame.ScreenShake / ScreenRumble / ScreenShakeTrauma on the room */
                CueLedger::used(c);
            } else if (call == "PlayerHurtVignetteHelper.Play") {
                spawn_combat_vfx(lib, screen, call, args, anchors);
                CueLedger::used(c);
            } else if (call == "NDamageNumVfx.Create" && args.size() >= 2 && args[(size_t)0].str() == "player" &&
                       args[1].is_number()) {
                /* NDamageNumVfx.Create(target, damage): no interactable
                 * node, the local player: at viewport * (0.25, 0.5). */
                Node* num = make_damage_number(lib, Vec2(480, 540), args[1].i());
                if (num) add_under_bar(screen, num);
                CueLedger::used(c);
            } else if (call == "VfxCmd.PlayNonCombatVfx" && args.size() >= 3) {
                /* an event's effect at a position in its room (Dense
                 * Vegetation's slashes, turned by a random -180..0 degrees
                 * there; Jungle Maze's hits) */
                Node* fx = instance(lib, "scenes/" + args[(size_t)0].str() + ".tscn");
                if (fx && fx->is_node2d()) {
                    Node2D* n = (Node2D*)fx;
                    n->position = Vec2(args[1].f(), args[2].f());
                    const std::string& path = args[(size_t)0].str();
                    if (path == "vfx/vfx_attack_slash" || path == "vfx/events/dense_vegetation_slice_vfx")
                        n->rotation = -(float)(chaotic_rand() % 1001) / 1000.0f * 3.14159265f;
                    if (at > 0) {
                        Node* later = SceneLibrary::create_node("Node");
                        later->name = "AddLater";
                        later->behavior = new AddLater(fx, at + c["delay"].num(0) / 1000.0);
                        screen->add_child(later);
                    } else {
                        add_under_bar(screen, fx);
                    }
                    CueLedger::used(c);
                } else {
                    delete fx;
                }
            } else if (call == "PlayerFullscreenHealVfx.Play" && args.size() >= 2) {
                heal_vfx(lib, screen, d["player"], args[1].num());
                CueLedger::used(c);
            } else if (call == "NTopBarHp.LerpAtNeow") {
                CueLedger::used(c);
                Node* bar = screen->get_node("TopBarLayer");
                Node* label = bar ? bar->find_unique("HpLabel") : NULL;
                if (!label) continue;
                Node* later = SceneLibrary::create_node("Node");
                later->name = "HpLerp";
                later->behavior = new HpLerp(label, d["player"]["hp"].i(), d["player"]["max_hp"].i());
                bar->add_child(later);
            }
        }
}

} /* namespace spire */

#include "room_cues.h"
#include "combat_vfx.h"
#include "screen_shake.h"
#include "top_bar.h"
#include "ui_util.h"

#include "../scene/controls.h"
#include "../scene/node.h"
#include "../scene/particles.h"
#include "../scene/scene.h"
#include "../scene/script.h"
#include "../scene/tween.h"

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
    Node* v = instance(lib, "scenes/vfx/whole_screen/vfx_smoky_vignette.tscn");
    if (v && v->is_canvas_item()) {
        CanvasItem* c = (CanvasItem*)v;
        Color green = hex(0x7FFF00);
        float a = e * 0.8f > 0.4f ? e * 0.8f : 0.4f;
        c->modulate = Color(green.r, green.g, green.b, 0);
        Node* h = v->get_node("Highlights");
        CanvasItem* hc = h && h->is_canvas_item() ? (CanvasItem*)h : NULL;
        if (hc) hc->modulate = Color(0, 1, 0, 0);
        delete v->behavior;
        v->behavior = new SmokyVignette(c, hc, a, 0.33f);
        add_under_bar(screen, v);
    } else {
        delete v;
    }
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

void play_room_cues(SceneLibrary* lib, Node* screen, const Json& d) {
    const Json& steps = d["steps"];
    /* an event's combat room plays its own cues (EventScreen) */
    bool room = d["layout"].str() == "Combat";
    NoCreatures anchors;
    for (size_t i = 0; i < steps.size(); ++i)
        for (size_t k = 0; k < steps[i].size(); ++k) {
            const Json& c = steps[i][k];
            const std::string& t = c["t"].str();
            if (t == "relic_flash" && !room) {
                Node* bar = screen->get_node("TopBarLayer");
                Node* row = bar ? find_named(bar, "RelicInventory") : NULL;
                int index = c["index"].i(-1);
                if (!row || index < 0 || index >= (int)row->children.size() || !row->children[(size_t)index]->is_control()) continue;
                Node* later = SceneLibrary::create_node("Node");
                later->name = "RelicFlash";
                later->behavior = new HolderFlashLater(lib, (Control*)row->children[(size_t)index]);
                bar->add_child(later);
                continue;
            }
            if (t != "ui") continue;
            const std::string& call = c["call"].str();
            const Json& args = c["args"];
            if (room && call != "PlayerFullscreenHealVfx.Play") continue;
            if (ScreenShake::get().cue(call, args)) {
                /* NGame.ScreenShake / ScreenRumble / ScreenShakeTrauma on the room */
            } else if (call == "PlayerHurtVignetteHelper.Play") {
                spawn_combat_vfx(lib, screen, call, args, anchors);
            } else if (call == "NDamageNumVfx.Create" && args.size() >= 2 && args[(size_t)0].str() == "player" &&
                       args[1].is_number()) {
                /* NDamageNumVfx.Create(target, damage): no interactable
                 * node, the local player: at viewport * (0.25, 0.5). */
                Node* num = make_damage_number(lib, Vec2(480, 540), args[1].i());
                if (num) add_under_bar(screen, num);
            } else if (call == "PlayerFullscreenHealVfx.Play" && args.size() >= 2) {
                heal_vfx(lib, screen, d["player"], args[1].num());
            }
        }
}

} /* namespace spire */

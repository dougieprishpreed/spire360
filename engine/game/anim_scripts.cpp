/* Animation scripts: ports of the game's C# helpers that drive Spine nodes. */
#include "../scene/animation.h"
#include "../scene/node.h"
#include "../scene/script.h"
#include "../scene/spine_sprite.h"
#include "../core/json.h"
#include "game.h"
#include "ui_util.h"

#include <algorithm>

namespace spire {

/* NSpineAutoPlayer (src/Core/Nodes/Animation/NSpineAutoPlayer.cs): plays
 * the parent SpineSprite's only animation, looping, on track 0, once the
 * skeleton is ready. */
class SpineAutoPlayer : public ScriptBehavior {
public:
    virtual void ready(Node* n) {
        SpineSprite* sprite = n->parent && n->parent->type == "SpineSprite" ? (SpineSprite*)n->parent : NULL;
        SpSkeleton* skel = sprite ? sprite->skeleton() : NULL;
        if (!skel) return;
        const std::vector<SpAnimation>& anims = skel->data()->animations;
        if (anims.size() != 1) {
            log_warn("%s: NSpineAutoPlayer's parent has %d animations (needs exactly 1)", n->path().c_str(),
                     (int)anims.size());
            return;
        }
        sprite->set_animation(anims[0].name, true, 0);
    }
};

static ScriptBehavior* make_auto_player(Node*) { return new SpineAutoPlayer(); }

/* NPhobiaAnimationToggler: its _animationPlayer inactive in phobia mode
 * (the Infection overlay's crawling stops). */
class PhobiaAnimationToggler : public ScriptBehavior {
public:
    virtual void ready(Node* n) {
        const Json* p = n->get_extra("_animationPlayer");
        Node* ap = p ? n->get_node((*p)["np"].str()) : NULL;
        if (ap && ap->type == "AnimationPlayer" && client_settings()["phobia_mode"].b()) ((AnimationPlayer*)ap)->set_active(false);
    }
};

/* NCombatBackgroundLayer: the PhobiaModeVisual shown instead of the Visual
 * in phobia mode. */
class CombatBackgroundLayer : public ScriptBehavior {
public:
    virtual void ready(Node* n) {
        Node* alt = n->get_node("PhobiaModeVisual");
        if (!alt) return;
        bool phobia = client_settings()["phobia_mode"].b();
        set_visible(alt, phobia);
        set_visible(n->get_node("Visual"), !phobia);
    }
};

/* NKaiserCrabBossBackground: the crab behind the Kaiser Crab's two parts,
 * its body and arms on tracks 0 / 1 (left) / 2 (right), reactions on 3
 * (emptied after each). The right arm rests or charges between attacks. */
class KaiserCrabBackground;
static std::vector<KaiserCrabBackground*> g_crabs; /* shown now, newest last */

class KaiserCrabBackground : public ScriptBehavior {
public:
    KaiserCrabBackground() : sprite_(NULL), right_(DEFAULT) { g_crabs.push_back(this); }
    virtual ~KaiserCrabBackground() { g_crabs.erase(std::find(g_crabs.begin(), g_crabs.end(), this)); }
    virtual void ready(Node* n) {
        Node* v = n->find_unique("Visuals");
        sprite_ = v && v->type == "SpineSprite" ? (SpineSprite*)v : NULL;
        if (!sprite_) return;
        sprite_->set_animation("right/idle_loop", true, 2);
        sprite_->set_animation("body/idle_loop", true, 0);
        sprite_->set_animation("left/idle_loop", true, 1);
    }
    bool call(const std::string& method, const Json& args, double* wait) {
        if (!sprite_) return true;
        std::string side = lower(args[(size_t)0].str());
        int track = side == "left" ? 1 : 2;
        if (method == "PlayAttackAnim") {
            sprite_->set_animation(side + "/" + args[1].str(), false, track);
            reaction("reactions/attack_" + side);
            sprite_->add_animation(side + "/idle_loop", true, 0, track);
            *wait = args[2].num(0);
        } else if (method == "PlayHurtAnim") {
            reaction("reactions/hurt_" + side);
            if (side == "left") {
                sprite_->set_animation("left/hurt", false, 1);
                sprite_->add_animation("left/idle_loop", true, 0, 1);
            } else if (right_ == DEFAULT) {
                sprite_->set_animation("right/hurt", false, 2);
                sprite_->add_animation("right/idle_loop", true, 0, 2);
            } else if (right_ == CHARGING) {
                sprite_->set_animation("right/hurt_charged", false, 2);
                sprite_->add_animation("right/charged_loop", true, 0, 2);
            } else {
                sprite_->set_animation("right/hurt_resting", false, 2);
                sprite_->add_animation("right/rest_loop", true, 0, 2);
            }
        } else if (method == "PlayArmDeathAnim") {
            reaction("reactions/hurt_" + side);
            if (side == "left") sprite_->set_animation("left/die", false, 1);
            else sprite_->set_animation(right_ == RESTING ? "right/die_resting" : "right/die", false, 2);
        } else if (method == "PlayRightSideChargeUpAnim") {
            right_ = CHARGING;
            sprite_->set_animation("right/charge_up", false, 2);
            sprite_->add_animation("right/charged_loop", true, 0, 2);
            reaction("reactions/attack_right");
            *wait = args[(size_t)0].num(0);
        } else if (method == "PlayRightSideHeavy") {
            right_ = RESTING;
            sprite_->set_animation("right/attack_heavy", false, 2);
            sprite_->add_animation("right/rest_loop", true, 0, 2);
            reaction("reactions/attack_right");
            *wait = args[(size_t)0].num(0);
        } else if (method == "PlayRightRecharge") {
            right_ = DEFAULT;
            sprite_->set_animation("right/wake_up", false, 2);
            sprite_->add_animation("right/idle_loop", true, 0, 2);
            *wait = args[(size_t)0].num(0);
        } else if (method == "PlayBodyDeathAnim") {
            sprite_->set_animation("body/die", false, 0);
        } else {
            return false;
        }
        return true;
    }

private:
    enum RightArm { DEFAULT, CHARGING, RESTING };
    void reaction(const std::string& anim) {
        sprite_->set_animation(anim, false, 3);
        sprite_->add_empty_animation(3);
    }
    SpineSprite* sprite_;
    RightArm right_;
};

bool kaiser_crab_call(const std::string& call, const Json& args, double* wait) {
    static const std::string prefix = "NKaiserCrabBossBackground.";
    *wait = 0;
    if (call.compare(0, prefix.size(), prefix) != 0) return false;
    if (g_crabs.empty()) return true;
    g_crabs.back()->call(call.substr(prefix.size()), args, wait);
    return true;
}

double kaiser_crab_wait(const std::string& call, const Json& args) {
    if (call == "NKaiserCrabBossBackground.PlayAttackAnim") return args[2].num(0);
    if (call == "NKaiserCrabBossBackground.PlayRightSideChargeUpAnim" || call == "NKaiserCrabBossBackground.PlayRightSideHeavy" ||
        call == "NKaiserCrabBossBackground.PlayRightRecharge")
        return args[(size_t)0].num(0);
    return 0;
}

static ScriptBehavior* make_kaiser_crab(Node*) { return new KaiserCrabBackground(); }
static ScriptBehavior* make_phobia_toggler(Node*) { return new PhobiaAnimationToggler(); }
static ScriptBehavior* make_background_layer(Node*) { return new CombatBackgroundLayer(); }

void install_anim_scripts() {
    register_script("res://src/Core/Nodes/Animation/NSpineAutoPlayer.cs", make_auto_player);
    register_script("res://src/Core/Nodes/Animation/NPhobiaAnimationToggler.cs", make_phobia_toggler);
    register_script("res://src/Core/Nodes/Rooms/NCombatBackgroundLayer.cs", make_background_layer);
    register_script("res://src/Core/Nodes/Vfx/Backgrounds/NKaiserCrabBossBackground.cs", make_kaiser_crab);
}

} /* namespace spire */

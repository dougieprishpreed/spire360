/* Animation scripts: ports of the game's C# helpers that drive Spine nodes. */
#include "../scene/animation.h"
#include "../scene/node.h"
#include "../scene/script.h"
#include "../scene/spine_sprite.h"
#include "game.h"
#include "ui_util.h"

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

static ScriptBehavior* make_phobia_toggler(Node*) { return new PhobiaAnimationToggler(); }
static ScriptBehavior* make_background_layer(Node*) { return new CombatBackgroundLayer(); }

void install_anim_scripts() {
    register_script("res://src/Core/Nodes/Animation/NSpineAutoPlayer.cs", make_auto_player);
    register_script("res://src/Core/Nodes/Animation/NPhobiaAnimationToggler.cs", make_phobia_toggler);
    register_script("res://src/Core/Nodes/Rooms/NCombatBackgroundLayer.cs", make_background_layer);
}

} /* namespace spire */

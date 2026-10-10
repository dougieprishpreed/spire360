/* Animation scripts: ports of the game's C# helpers that drive Spine nodes. */
#include "../scene/node.h"
#include "../scene/script.h"
#include "../scene/spine_sprite.h"
#include "game.h"

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

void install_anim_scripts() { register_script("res://src/Core/Nodes/Animation/NSpineAutoPlayer.cs", make_auto_player); }

} /* namespace spire */

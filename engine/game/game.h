/* Ports of the game's presentation scripts (C# node scripts the logic
 * build strips, see logic/patches.json ui.strip). Each module registers its
 * behaviours by script path; call install_game_scripts() once at startup. */
#ifndef SPIRE_GAME_H
#define SPIRE_GAME_H

#include "../core/base.h"

namespace spire {

class Node;
class SceneLibrary;
bool shake_vfx(Node* shaker, float strength, double duration);
Node* make_vfx_projectile(SceneLibrary* lib, const std::string& handler, const std::string& projectile,
                          const Vec2& from, const Vec2& to, double delay = 0);

void install_text_scripts();
void install_anim_scripts();
void install_vfx_scripts();
void install_combat_vfx_scripts();

inline void install_game_scripts() {
    install_text_scripts();
    install_anim_scripts();
    install_vfx_scripts();
    install_combat_vfx_scripts();
}

} /* namespace spire */

#endif

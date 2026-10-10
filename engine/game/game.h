/* Ports of the game's presentation scripts (C# node scripts the logic
 * build strips, see logic/patches.json ui.strip). Each module registers its
 * behaviours by script path; call install_game_scripts() once at startup. */
#ifndef SPIRE_GAME_H
#define SPIRE_GAME_H

namespace spire {

void install_text_scripts();

inline void install_game_scripts() { install_text_scripts(); }

} /* namespace spire */

#endif

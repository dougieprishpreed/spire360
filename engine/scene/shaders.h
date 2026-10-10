/* The game's canvas shaders, ported one by one. A ShaderMaterial whose
 * shader has no port is not drawn (SceneTree reports it); a ported one sets
 * renderer state for the item. */
#ifndef SPIRE_SHADERS_H
#define SPIRE_SHADERS_H

#include "resource.h"

namespace spire {

class Canvas;

/* Godot's TIME for shaders, in seconds. */
extern double g_shader_time;

/* Applies the material for the next item's draws; false: not ported. */
bool shader_begin(Canvas& c, const MaterialRes* m);
void shader_end(Canvas& c, const MaterialRes* m);

} /* namespace spire */

#endif

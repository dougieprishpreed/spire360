/* The game's canvas shaders, ported one by one. A ShaderMaterial whose
 * shader has no port is not drawn (SceneTree reports it); a ported one sets
 * renderer state for the item. */
#ifndef SPIRE_SHADERS_H
#define SPIRE_SHADERS_H

#include "../gfx/renderer.h"
#include "resource.h"

namespace spire {

class Canvas;
class CanvasItem;

/* Godot's TIME for shaders, in seconds. */
extern double g_shader_time;

/* Applies the material for the next item's draws; false: not ported. */
bool shader_begin(Canvas& c, const MaterialRes* m, const CanvasItem* item = NULL);
void shader_end(Canvas& c, const MaterialRes* m);
/* A material's shader parameters: a sampler2D's resource (NULL: unset),
 * a float, a vec2 (fallbacks when unset). */
Resource* shader_param_resource(const MaterialRes* m, const char* name);
float shader_param(const MaterialRes* m, const char* name, float fallback);
Vec2 shader_param_vec2(const MaterialRes* m, const char* name, const Vec2& fallback);
/* The erosion particle shader applied now (NULL: none): particles draw each
 * one with its own erosion threshold. */
const VfxParams* shader_vfx();
/* The blend a ported shader's render_mode asks for (blend_add, ...). */
bool shader_blend(const MaterialRes* m, BlendMode* out);

} /* namespace spire */

#endif

/* Effect shaders: ports of the game's own canvas shaders that are not one
 * of the canvas shader's modes. Each has an id, up to 16 parameters and up
 * to 4 textures (EffectParams); the software renderer runs a C++ version
 * (sw_effects.cpp), the D3D9 renderer a pixel shader of its own
 * (shaders/effects.hlsl, entry point ps_e<id>). The draw's texture is the
 * shader's TEXTURE, the vertex colour its COLOR's input; the output is what
 * the game's fragment() writes to COLOR (premultiplied when drawn). */
#ifndef SPIRE_EFFECTS_H
#define SPIRE_EFFECTS_H

namespace spire {

enum EffectId {
    EFFECT_NONE = 0,
    /* shaders/vfx/vfx_water_reflection.gdshader: TEXTURE where mask(UV).a
     * <= 0.1, else TEXTURE at UV - p1 * 0.5 + noise(UV + p0).r * p1 * 0.5.
     * p0: -TIME * speed (both axes), p1: strength; tex0 noise (repeat),
     * tex1 mask. The vertex colour is not applied. */
    EFFECT_WATER_REFLECTION = 1,
    EFFECT_COUNT
};

} /* namespace spire */

#endif

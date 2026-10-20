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
    /* shaders/scrolling_smoke.tres: rgb p4..6 * smoke.rgb, alpha
     * smoothstep(p1, p2, smoke.a * gradient(UV).r) * p3; smoke = tex0 at
     * UV + (0, p0) (p0: TIME * ScrollSpeed + timeOffset); tex1 gradient. */
    EFFECT_SCROLLING_SMOKE = 2,
    /* shaders/vfx/common/vfx_noise_scale_shader.gdshader (fragment): lut
     * (tex0) at TEXTURE.r times the vertex colour, alpha vertex alpha *
     * mix(TEXTURE.a, TEXTURE.r, p0). */
    EFFECT_LUT_COLOR = 3,
    /* images/vfx/simple_noise_flat_shader.tres and simple_noise_add_shader:
     * rgb the vertex colour, alpha smoothstep(p0, p1, vertex alpha *
     * TEXTURE.r * n1.r * n2.r); n1 = tex0 at (UV + (0.275, 0)) * p2p3 -
     * (0.275, 0) + p4p5, n2 = tex1 likewise with p6p7, p8p9 (tex1 unset:
     * white). */
    EFFECT_SIMPLE_NOISE = 4,
    /* shaders/vfx/monsters/decimillipede_suck_shader.tres: colour
     * (0.0153, 0.0901, 0.17), alpha smoothstep(0.4, 0.6, gradient(UV +
     * (0.2, -0.05) + pattern.rg).r) * smoothstep(0.4, 0.7, shape(UV +
     * pattern.r * (0.03, 0)).r) * p4; pattern = tex0(UV * 3 + p0p1) *
     * tex0(UV * 2 + p2p3) (the time offsets); tex1 gradient, tex2 shape. */
    EFFECT_DECIMILLIPEDE_SUCK = 5,
    /* images/vfx/vfx_molten_fist/vfx_molten_fist.tres: rgb the vertex colour
     * (only COLOR.a is written), alpha smoothstep(p0, p1, (p2 * n2.b * p3 * n1.b + m) *
     * tex3(UV).a); n1 = tex0(UV * p4p5 + p6p7), n2 = tex1(UV * p8p9 + p10p11),
     * m = tex2(((UV - (0, 0.04)) * p12p13 + (0, 0.04)) + p14p15).b, inverted
     * when p3 < 0 (p3 = |Noise1Strength|, the sign is InvertNoiseMask). */
    EFFECT_MOLTEN_FIST = 6,
    /* images/vfx/slash_shader_flat.tres: rgb p12..14, alpha smoothstep(p0, p1,
     * shape(UV).b * p2 * n1.r * n2.r * p3) * p4; n1 = tex1 at (UV + (0.275,
     * 0)) * p5p6 - (0.275, 0) + p7p8 (time panned), n2 = tex2 the same with
     * p9p10, p15p11; tex0 shape. */
    EFFECT_SLASH_FLAT = 7,
    /* shaders/texture_transition.gdshader: rgb the vertex colour (only
     * COLOR.a is written), alpha (step(1 - tex0(UV).r, mix(-0.1, 1.1, p0)) + p0 * p1) *
     * p2. */
    EFFECT_TEXTURE_TRANSITION = 8,
    /* shaders/vfx/common/vfx_panning_shader.gdshader: TEXTURE at UV * p0p1 +
     * p2p3 (time and instance offset); rgb lut(tex0 at its r) * vertex rgb *
     * p6, alpha smoothstep(p4, p4 + p5, mix(a, r, p7)) * vertex alpha *
     * mask(UV).r (tex1). p4: the erosion curve at the particle's life. */
    EFFECT_PANNING = 9,
    /* shaders/vfx/common/vfx_ray_shader.gdshader (fragment): lut (tex0) at
     * TEXTURE.r times the vertex colour, alpha vertex alpha * TEXTURE.a *
     * tex1(UV.yx).r. */
    EFFECT_RAY = 10,
    /* shaders/vfx/dagger/vfx_dagger_spray_single_shader.gdshader: (lut.rgb,
     * lut.a * TEXTURE.a) * vertex colour; lut (tex0) at TEXTURE.r. */
    EFFECT_LUT_ALPHA = 11,
    EFFECT_COUNT
};

} /* namespace spire */

#endif

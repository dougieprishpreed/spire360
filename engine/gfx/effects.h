/* Effect shaders: ports of the game's own canvas shaders that are not one
 * of the canvas shader's modes. Godot 4's fragment COLOR starts as the
 * vertex colour times the texel (the shaders keep a vertex_color varying
 * for the vertex colour alone). Each has an id, up to 16 parameters and up
 * to 5 textures (EffectParams); the software renderer runs a C++ version
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
     * rgb COLOR's (vertex times texel), alpha smoothstep(p0, p1, COLOR.a *
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
    /* images/vfx/vfx_molten_fist/vfx_molten_fist.tres: rgb COLOR's (only
     * COLOR.a is written), alpha smoothstep(p0, p1, (p2 * n2.b * p3 * n1.b + m) *
     * tex3(UV).a); n1 = tex0(UV * p4p5 + p6p7), n2 = tex1(UV * p8p9 + p10p11),
     * m = tex2(((UV - (0, 0.04)) * p12p13 + (0, 0.04)) + p14p15).b, inverted
     * when p3 < 0 (p3 = |Noise1Strength|, the sign is InvertNoiseMask). */
    EFFECT_MOLTEN_FIST = 6,
    /* images/vfx/slash_shader_flat.tres: rgb p12..14, alpha smoothstep(p0, p1,
     * shape(UV).b * p2 * n1.r * n2.r * p3) * p4; n1 = tex1 at (UV + (0.275,
     * 0)) * p5p6 - (0.275, 0) + p7p8 (time panned), n2 = tex2 the same with
     * p9p10, p15p11; tex0 shape. */
    EFFECT_SLASH_FLAT = 7,
    /* shaders/texture_transition.gdshader: rgb COLOR's (only COLOR.a is
     * written), alpha (step(1 - tex0(UV).r, mix(-0.1, 1.1, p0)) + p0 * p1) *
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
    /* shaders/dissolve.gdshader (NMonsterDeathVfx, a SubViewport's picture,
     * premultiplied): TEXTURE at UV + o * o, o = max(UV.x - (p0 * 1.9 -
     * 0.55), 0) * (-200, 1024) * p2p3; blackened outside strength(UV, 0.05),
     * greyed as p0 falls, times strength(UV, 0). strength(uv, k): uv.x += k,
     * step(mix(n1, n2, 0.4), p0), ni = (tex_i(uv * (1 / 400) / p2p3 + (p1,
     * uv.x^3)).r + uv.x) / 2. p0 threshold, p1 -TIME / 4, p2p3
     * TEXTURE_PIXEL_SIZE; tex0, tex1 the noises (repeat). */
    EFFECT_DISSOLVE = 12,
    /* shaders/vfx/ui/card/exhaust/vfx_card_exhaust_dissolve_shader: a
     * SubViewport's card (TEXTURE as stored) eaten where the erosion (tex0
     * at UV + (p1, 0), red) is below p0, its edge (smoothstep(p0, p0 + p2))
     * through the bright lut (tex1, hue shifted by p3); rgb * vertex rgb,
     * alpha TEXTURE.a * vertex alpha. */
    EFFECT_EXHAUST_DISSOLVE = 13,
    /* shaders/vfx/ui/card/exhaust/vfx_card_exhaust_quick_erosion_shader (a
     * particle, its flipbook frame from Particles2D::draw): rgb the bright
     * lut (tex0) at smoothstep(p0, p0 + p2, TEXTURE.r), hue shifted by p3,
     * times the vertex colour; alpha TEXTURE.a * vertex alpha *
     * smoothstep(p0, p0 + p1, TEXTURE.r). p0: the erosion curve at the
     * particle's life. */
    EFFECT_EXHAUST_QUICK = 14,
    /* shaders/boss_map_point_placeholder.gdshader: rgb tint p0..2 * (0.7 +
     * 0.3 * grey(TEXTURE)), alpha TEXTURE.a * 0.75 (no vertex colour). */
    EFFECT_TINT_GRAY = 15,
    /* shaders/enchantment_tab_appear.gdshader: the tab drawn in left to
     * right as p0 (progress) rises, its leading edge squeezed and whitened
     * (+ (1, 0.75, 0.9)), fading out ahead of it (no vertex colour). */
    EFFECT_TAB_APPEAR = 16,
    /* shaders/vfx/ui/vfx_ui_low_hp_border_shader.gdshader: main colour
     * p10..12 times the vertex colour, alpha smoothstep(p8, p9, q * p0 +
     * noise.r / 4) * p1; q = 1 - (1 - |2 UV - 1| / p3) / (1 - p2); noise
     * (tex0) at the polar uv * p4p5 + p6p7. */
    EFFECT_LOW_HP_BORDER = 17,
    /* shaders/vfx/sleeping/vfx_sleeping_z_shader.gdshader (particles): the
     * texel at UV + distortion, distortion = (anim(UV + p3p4).b - 0.5) * 2 *
     * p2 (strength * curve at the life); rgb mix(outline p6..8, vertex rgb,
     * texel.r), alpha (outline p9 or vertex a) * texel.a * smoothstep(p0, p0
     * + p1, anim(UV + p5).g). tex0 the animation texture (repeat). */
    EFFECT_SLEEPING_Z = 18,
    /* shaders/vfx/heavy_blunt/vfx_heavy_blunt_erosion_shader.gdshader
     * (particles): rgb lut(tex0 at TEXTURE.r) * vertex rgb, alpha vertex a *
     * smoothstep(p0, p0 + p1, TEXTURE.r); p0 the curve at the life. */
    EFFECT_LUT_EROSION = 19,
    /* shaders/vfx/potion/vfx_potion_splash_shader.gdshader (particles): the
     * texel t at UV + TEXTURE(UV + p2p3).b * p4; rgb lut(tex0 at t.r) *
     * vertex rgb, alpha vertex a * smoothstep(p0, p0 + p1, t.a). */
    EFFECT_POTION_SPLASH = 20,
    /* shaders/blur/Blur.gdshader on a card's portrait (an unseen or locked
     * card): the game's 33-tap Gaussian along step, then its canvas group's
     * pass across it; here both passes at once, 7 x 7 taps over the same
     * radius (p0 * 32 texels; p1p2 the texel size). Opaque, as the game's
     * (COLOR.a = 1). */
    EFFECT_PORTRAIT_BLUR = 21,
    /* shaders/vfx/goopy_impact/vfx_goopy_impact_bits_shader.gdshader
     * (particles): the flipbook frame floor(p5) of a p6 x p7 sheet read at
     * its uv + d * p4, d = (TEXTURE(UV + p2p3).b - 0.5) * 2; (lut(tex0 at
     * (t.r, t.r)).rgb, smoothstep(p0, p0 + p1, t.g)) * vertex colour. p0 the
     * erosion curve at the life, p5 CUSTOM.z * frame_count * 0.99. */
    EFFECT_GOOPY_BITS = 22,
    /* shaders/vfx/ui/card/afflictions/bound/..._bound_main_shader: rgb
     * TEXTURE.r * vertex rgb * mix(1, p4, TEXTURE(UV * p0p1 + p2p3).g),
     * alpha TEXTURE.a * vertex a. */
    EFFECT_BOUND_MAIN = 23,
    /* ..._bound_border_shader: rgb TEXTURE(polar uv * p0p1 + p2p3).rgb *
     * vertex rgb, alpha smoothstep(p4, p4 + p5, vertex a * mask(UV).a)
     * (tex0 mask). Polar uv: fract(distance from the middle * 2, angle / 2
     * pi). */
    EFFECT_POLAR_BORDER = 24,
    /* shaders/vfx/scream/vfx_scream_squiggly_ring_polar_shader (particles):
     * dm = TEXTURE(polar(UV) * p2p3 + p4p5 + p6); the ring TEXTURE at
     * polar(UV + normalize(0.5 - UV) * dm.g * dm.b * p7) * p8p9 + (p10 +
     * p11, 0); rgb vertex rgb, alpha vertex a * TEXTURE.a * smoothstep(p0,
     * p0 + p1, ring.r). p0 the erosion curve at the life, p6 CUSTOM.x (the
     * rotation), p11 (x_offsets.y - x) * the offset curve at the life. */
    EFFECT_SQUIGGLY_RING = 25,
    /* shaders/vfx/fire/fire_shader_1.tres (the queen's flames): rgb
     * p13..15, alpha smoothstep(p11, p12, gradient * shape); gradient =
     * tex2(UV + (-1.065, -0.24) + (p6 + n1) * (n2 + p7)).r, n1 = tex0(UV *
     * p2 + p0p1).r, n2 = tex1(UV * p5 + p3p4).r; shape = tex4(UV +
     * tex3(UV + p8p9).rg * p10).r. No vertex colour. */
    EFFECT_FIRE_SHAPE = 26,
    /* shaders/vfx/simple_distortion_flat_shader.tres: t = TEXTURE at UV +
     * p2p3 * tex0(UV + p0p1).r; rgb p4..6 * t.rgb, alpha t.a (no vertex
     * colour). */
    EFFECT_SIMPLE_DISTORTION = 27,
    /* ..._affliction_entangled_main_shader (and tainted): d =
     * TEXTURE(UV * p0p1 + (p2, p3)).b * p4; t = TEXTURE at UV + (d *
     * TEXTURE(UV).g, 0); rgb vertex rgb * lut(tex0 at t.r), alpha vertex a *
     * t.a. */
    EFFECT_ENTANGLED = 28,
    /* shaders/vfx/2d_sphere_shader.tres (blend_add): t = TEXTURE at UV +
     * p5p6 + (0, -0.075 * tex1(UV).g) + tex0(UV * p2p3 + p0p1).rg * p4; rgb
     * p7..9 + t.rgb, alpha t.a * smoothstep(p10, p11, tex2(UV).r) *
     * smoothstep(p12, p13, tex3(UV + p14p15).r) (no vertex colour). */
    EFFECT_SPHERE = 29,
    /* shaders/vfx/scream/vfx_scream_ring_polar_shader (particles): rgb
     * vertex rgb, alpha vertex a * TEXTURE(polar(UV) * p0p1 + (p2 + p3,
     * 0)).r * TEXTURE.a; p3 (x_offsets.y - x) * the offset curve at the
     * life. */
    EFFECT_SCREAM_RING = 30,
    /* ..._affliction_hexed_main_shader: n = smoothstep(p4, p4 + p5,
     * TEXTURE(polar(UV) * p0p1 + p2p3).b); c = mix(border p12..14,
     * mix(main p6..8, bright p9..11, n), TEXTURE.r); rgb c * vertex rgb,
     * alpha TEXTURE.a * vertex a. */
    EFFECT_HEXED = 31,
    /* ..._affliction_ringing_main_shader: f = p0 (fract(TIME * speed)); a
     * = max(TEXTURE.rgb * tex0(f, f).rgb, TEXTURE(polar(UV, zoom 0.8) * 0.5
     * + (tex1(f, f).r, 0)).a * tex1(f, f).g); rgb vertex rgb (the vertex
     * alpha is not applied). */
    EFFECT_RINGING = 32,
    /* shaders/vfx/missile/vfx_missile_core_shader: the flipbook frame p0
     * of a p1 x p2 sheet; rgb lut(tex0 at (t.r, t.r)) * vertex rgb, alpha
     * t.a * vertex a. */
    EFFECT_LUT_FLIPBOOK = 33,
    /* shaders/vfx/vfx_noisy_dissolve_shader.tres: rgb COLOR's (vertex times
     * texel), alpha smoothstep(p0, p0 + p1, tex0(UV + p2p3).r) * TEXTURE.a
     * (the vertex alpha is not applied). */
    EFFECT_NOISY_DISSOLVE = 34,
    /* ..._affliction_smog_main_shader: r = rotate(UV, 0.5, PI) = (v, 1 -
     * u); s = mix(TEXTURE(r * p0p1 + p2p3).r, TEXTURE(r * p4p5 + p6p7).r,
     * p8) * mask.r; a = smoothstep(p9, p9 + p10, s); rgb vertex rgb *
     * lut(a, a), alpha a * vertex a + mask.g (tex0 lut, tex1 mask). */
    EFFECT_SMOG = 35,
    /* ..._entangled_leaf_shader (particles): the texture turned about p5p6
     * by p4 * (TEXTURE(UV * p0p1 + (p2, p2 + p3)).b - 0.5) * PI (rotate()
     * at PI / 2 + that); rgb vertex rgb * lut(t.r) (tex0), alpha vertex a *
     * t.a. p2 CUSTOM.z. */
    EFFECT_LEAF = 36,
    /* scenes/backgrounds/vantom_boss/oil.gdshader (no texture, no vertex
     * colour): the oil disc of radius p0 (min(1, max / rank advance mana) *
     * inset) with its noisy border (p8 wide, colour p10..13, noise tex0 at
     * the border time p2), the grooves (tex1 at UV + p3, lines p5 apart p6
     * wide, sin(TIME * speed) p4, opacity p7) and the mana (ratio p1, glow
     * p9; its colour 8-bit packed: p14 = r * 256 + g, p15 = b * 256 + a,
     * + 65536 when current_mana > 0). */
    EFFECT_OIL = 37,
    /* ..._affliction_galvanized_main_shader: blink = smoothstep(p8, p8 +
     * p9, TEXTURE(UV.x * p0 + p1, 0).b); dt = TEXTURE(polar(UV) * p2p3 +
     * p4p5); t = TEXTURE at UV + (UV + (dt.b - 0.5) * 2 * p6 - 0.5) * dt.b *
     * -p7; l = lut(blink, 0) (tex0); rgb l.rgb * vertex rgb, alpha vertex a *
     * max(t.r, t.g * l.a * blink) * smoothstep(p10, p10 + p11, TEXTURE.a). */
    EFFECT_GALVANIZED = 38,
    /* shaders/vfx/sweeping_beam/vfx_sweeping_beam_flipbook_shader
     * (particles): x, y = UV - 0.5; z = p2 x + p5 y + p8 (nothing behind
     * the eye); the projected uv = (p0p1 x + p3p4 y + p6p7) * p9 / z -
     * p10p11 + 0.5 (nothing outside 0..1); its flipbook frame floor(p12) of
     * a p13 x p14 sheet; (lut(t.r, t.r) (tex0) * vertex colour, alpha also
     * times t.a * mask(projected uv).r (tex1)). The particle quads grow by
     * 1 + tan(fov / 2) (Particles2D::draw). */
    EFFECT_SWEEP_FLIPBOOK = 39,
    /* shaders/scry_reveal.gdshader (the Crystal Sphere's ball and fog): the
     * 11 x 11 grid inset by p2 (uvMargin); each cell's alpha from tex2
     * (its red, 1 outside the grid) and the borders where it differs from
     * a neighbour's (p3..5 the border colour); the ball's noise (tex0 at
     * the polar uv + (0, p6), tex1 at + (0, p7)) through p0 circles
     * (extra: colours 0..8 then circle data 0..7) clipped at the grid's
     * corners (p1: the outer circle, < 0 the inside); times the vertex
     * colour. */
    EFFECT_SCRY = 40,
    /* shaders/crystal_sphere_item.gdshader: COLOR (vertex times texel) with
     * 0.75 added where |UV.x - UV.y - o| <= 0.17 and 0.5 where |UV.x - UV.y
     * - o + 0.3| <= 0.03, o = mix(-1, 1.5, p0) (val). */
    EFFECT_SHINE = 41,
    /* shaders/vfx/vfx_doom_flame_shader.tres (a VisualShader, blend_add):
     * x = (p12 * n1.b * n2.b + m) * TEXTURE(UV + d).b, n1 = tex0(UV * p0p1
     * + p2p3), n2 = tex1(UV * p4p5 + p6p7), m = tex2((UV - (0, 0.04)) *
     * p8p9 + (0, 0.04) + p10p11).b (1 - m when p13 != 0), d = tex3(UV *
     * x0.xy + x0.zw).xy * x2.x + tex4(UV * x1.xy + x1.zw).xy * x2.y (its
     * DistortionMask unset in the game: white); rgb x3.rgb + smoothstep(p14,
     * p15, x) * x4.rgb, alpha smoothstep(x2.z, x2.w, x). The vertex colour
     * is not applied. */
    EFFECT_DOOM_FLAME = 42,
    /* shaders/vfx/poison/vfx_poison_skull_shader.gdshader (particles):
     * t = TEXTURE(UV + (0, TEXTURE(UV + p0p1).b * p2)); (lut(t.r).rgb,
     * smoothstep(p3, p3 + p4, t.g) * t.a) * vertex colour; tex0 lut, p3
     * the erosion curve at the particle's life. */
    EFFECT_POISON_SKULL = 43,
    /* vfx_demon_form_idle_panning_fire_shader and
     * vfx_reaper_form_idle_panning_noise_shader: n = smoothstep(p8, p8 +
     * p9, ((TEXTURE(u * p0p1 + p2p3).r + TEXTURE(u * p4p5 + p6p7).r) / 2 +
     * tex1(UV).b) * tex2(UV).r); (lut(n).rgb, n * (p13 != 0 ? (1 - UV.x) *
     * p10 : 1)) * vertex colour; tex0 lut. u = UV, or with p11 != 0 the
     * polar coordinates of UV rotated by p12 about the centre (reaper). */
    EFFECT_FORM_PANNING = 44,
    /* vfx_serpent_form_snakes_shader: TEXTURE at UV + (g - 0.5) * 2 * p4 *
     * TEXTURE(UV).b * p5p6, g = TEXTURE(UV * p0p1 + p2p3).g; (lut(its
     * r).rgb, its a) * vertex colour; tex0 lut (clamped). */
    EFFECT_SERPENT_SNAKES = 45,
    /* vfx_echo_form_echo_lines_shader: white, alpha smoothstep(p3, p4,
     * TEXTURE((UV - p0p1) / p2 + p0p1).a - TEXTURE(UV).a) * tex0(UV).r
     * (white when unset), times the vertex colour. */
    EFFECT_ECHO_LINES = 46,
    /* vfx_round_smoke_shader (particles; the flipbook frame by CUSTOM.z in
     * Particles2D, so UV here is the frame's): t = TEXTURE(UV + TEXTURE(UV
     * + p0p1).b * p2); (lut(t.r).rgb, smoothstep(p3, p3 + p4, t.r)) *
     * vertex colour; p3 the erosion curve at the particle's life. */
    EFFECT_ROUND_SMOKE = 47,
    /* vfx_scream_squiggly_streaks_shader (particles): m = (TEXTURE(UV *
     * p0p1 + p2p3 + p5).b - 0.5) * 2 (p5 CUSTOM.x, the rotation); (vertex
     * rgb, TEXTURE(UV + (m * p4, 0)).a * vertex alpha). */
    EFFECT_SCREAM_STREAKS = 48,
    /* vfx_card_remove_erosion_shader (particles): d = TEXTURE(UV + p0).b -
     * 0.5 (p0: speed * TIME + CUSTOM.z), t = TEXTURE(UV + (d * p2, 0));
     * (lut(smoothstep(p3, p3 + p5, t.a)).rgb, smoothstep(p3, p3 + p4,
     * t.a)) * vertex colour; p3 the erosion curve at the life. */
    EFFECT_CARD_REMOVE = 49,
    /* vfx_minion_divebomb_rings_shader (particles): (lut(TEXTURE.r).rgb *
     * vertex rgb, vertex alpha * smoothstep(p0, p0 + p1, TEXTURE.a)); p0 the
     * erosion curve at the particle's life. */
    EFFECT_DIVEBOMB_RINGS = 50,
    /* vfx_hyperbeam_laser_line (fragment; its vertex() wobble along y is
     * not ported): t = tex0(UV + (p0, 0)) (main_texture, repeat); (lut(t.r
     * + tex1(UV).r).rgb * vertex rgb, t.a * vertex alpha); tex1 overlay,
     * tex2 lut. */
    EFFECT_HYPERBEAM_LINE = 51,
    /* vfx_hyperbeam_impact_spikes_shader (particles): t = TEXTURE(UV *
     * p0p1 + (p2, p4 + p3)) (p2 CUSTOM.z, p3 the offset curve at the life,
     * p4 initial_offset); (vertex rgb, smoothstep(0.35, 0.65, vertex alpha
     * * t.r * tex0(UV).r)). */
    EFFECT_HYPERBEAM_SPIKES = 52,
    /* vfx_grand_finale_spotlight_add / _darken (blend add / sub): a =
     * fract(atan2(UV - p0p1) / 2pi + p2), d = clamp(|a - p4| / p5, 0, 1),
     * mixed towards 1 - d by p3, smoothstep(p6, p6 + p7, d); (d, d, d, 1) *
     * vertex colour. */
    EFFECT_SPOTLIGHT = 53,
    EFFECT_COUNT
};

} /* namespace spire */

#endif

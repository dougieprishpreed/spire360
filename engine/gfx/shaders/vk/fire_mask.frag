#version 450
// fire.frag through a canvas group's mask (vk_renderer.cpp, begin_mask): an
// additive child of a group whose layer would hold light alone, drawn
// straight onto the screen times the mask item's texel alpha and colour
// alpha at the pixel, as compositing the layer gives. c42, c43: the screen
// (0..1) to the mask's uv, its colour alpha in c42.w; c44: its uv's box,
// outside of which the mask item draws nothing; g_group its texture. The
// Underdocks' layers: no pass of their own, nor the scene's way out of
// eDRAM and back that each one cost.
#include "common.glsl"

float spire_mask() {
    vec2 m = vec2(dot(P.c[42].xy, v_screen) + P.c[42].z, dot(P.c[43].xy, v_screen) + P.c[43].z);
    vec4 b = P.c[44];
    float inside = step(b.x, m.x) * step(m.x, b.z) * step(b.y, m.y) * step(m.y, b.w);
    return texture(g_group, m).a * P.c[42].w * inside;
}

void main() {
    vec4 uv = v_uv.xyxy;
    vec4 noise_uv = uv * P.c[7] + P.c[8];
    vec4 mask_uv = uv * P.c[9] + P.c[10];
    vec4 dist_uv = uv * P.c[11] + P.c[12];
    float n1 = textureLod(g_fn1, noise_uv.xy, 0.0).b;
    float n2 = textureLod(g_fn2, noise_uv.zw, 0.0).b;
    float m = textureLod(g_fmask, mask_uv.xy, 0.0).b;
    vec2 d1 = textureLod(g_fd1, mask_uv.zw, 0.0).xy;
    vec2 d2 = textureLod(g_fd2, dist_uv.xy, 0.0).xy;
    vec2 dm = textureLod(g_fdmask, dist_uv.zw, 0.0).xy;
    vec4 k = P.c[13];
    float a = n1 * n2 * k.x + (m * k.y + k.z);
    vec2 d = (d1 * P.c[14].xy + d2 * P.c[14].zw) * dm;
    float v = a * textureLod(g_tex, v_uv + d, 0.0).b;
    vec2 t = saturate(v * P.c[15].xy + P.c[15].zw);
    vec2 s = t * t * (3.0 - 2.0 * t);
    float fa = s.y * v_color.a * spire_mask();
    o_color = vec4((P.c[16].rgb + P.c[17].rgb * s.x) * fa, fa);
}

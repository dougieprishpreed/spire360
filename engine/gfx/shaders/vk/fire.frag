#version 450
// canvas.hlsl's stepped fire branch: the game's stepped fire shaders
// (shaders/vfx/vfx_stepped_shader_fire_*), the doom bar's noise in
// doom_bar.frag. vk_renderer.cpp folds the parameters (fire_params) so that
// xost makes few instructions of it: it runs every instruction of both
// sides of a branch, and moves a constant into a register to swizzle it or
// to use two in one instruction. The Underdocks' water reflections draw a
// third of the screen six times over with it.
//   c7, c8: noise 1's and noise 2's coordinates, scale and offset;
//   c9, c10: the noise mask's (its 0.04 shift in the offset) and
//            distortion 1's; c11, c12: distortion 2's and the distortion
//            mask's;
//   c13: noise 1 times noise 2's strength, the mask's scale and add (the
//        inverted mask 1 - m);
//   c14: distortion 1's strength (xy) and distortion 2's (zw);
//   c15: the colour's and the alpha's smoothsteps as v * xy + zw (the
//        dark variant's of 1 - v);
//   c16, c17: the colour, c16 + c17 times its smoothstep.
#include "common.glsl"

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
    float fa = s.y * v_color.a;
    o_color = vec4((P.c[16].rgb + P.c[17].rgb * s.x) * fa, fa);
}

#version 450
// canvas.hlsl's erosion branch: the game's erosion particle shaders
// (vfx_poof, vfx_common_particle, ...): the texture through a distortion of
// itself, coloured by the lut, its alpha eroded; in polar coordinates
// (vfx_ring_polar) vfx_polar.frag.
#include "common.glsl"

void main() {
    vec2 uv = v_uv;
    float shift = textureLod(g_tex, uv + g_vdist.xy, 0.0).b * g_vdist.z;
    uv += g_vdist.w > 0.5 ? shift : 0.0;
    vec4 e = textureLod(g_tex, uv, 0.0);
    vec3 lut = textureLod(g_lut, vec2(e.r, 0.5), 0.0).rgb;
    vec3 rgb = g_vfx.z > 0.5 ? lut : vec3(1.0);
    float ch = g_verosion.w > 0.5 ? e.a : e.r;
    float k = saturate((ch - g_verosion.x) / max(g_verosion.y, 0.0001));
    float ea = g_verosion.z > 0.5 ? k * k * (3.0 - 2.0 * k) : e.a;
    vec4 vc = vec4(rgb, ea) * v_color;
    o_color = vec4(vc.rgb * vc.a, vc.a);
}

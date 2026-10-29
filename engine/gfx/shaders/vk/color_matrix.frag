#version 450
// main.frag's colour matrix alone (the game's hsv.gdshader: the cards'
// frames and portraits), as xost runs every instruction of its other modes.
#include "common.glsl"

void main() {
    vec4 t = texture(g_tex, v_uv);
    vec3 straight = t.a > 0.0 ? spire_div(t.rgb, t.a) : vec3(0.0);
    t.rgb = g_premul.x > 0.5 ? straight : t.rgb;
    vec3 m = vec3(dot(g_cm0.xyz, t.rgb) + g_cm0.w, dot(g_cm1.xyz, t.rgb) + g_cm1.w,
                  dot(g_cm2.xyz, t.rgb) + g_cm2.w);
    vec4 c = vec4(m, t.a) * v_color;
    o_color = vec4(c.rgb * c.a, c.a);
}

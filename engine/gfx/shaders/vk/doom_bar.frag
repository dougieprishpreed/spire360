#version 450
// canvas.hlsl's stepped fire branch for the doom bar
// (scenes/combat/doom_bar.gdshader): the scene's noise texture, sampled in
// screen space, through the gradient. Apart from fire.frag, as xost would
// run both sides of a branch between them.
#include "common.glsl"

void main() {
    float tm = g_fstr.x;
    float scroll = textureLod(g_fn1, v_screen * 0.1 + tm * 0.0005 + 0.24, 0.0).r;
    float n1 = textureLod(g_fn1, v_screen + tm * 0.0005 + scroll, 0.0).r;
    float n2 = textureLod(g_fn1, v_screen - tm * 0.0007 + scroll, 0.0).r;
    float value = smoothstep(0.4, 0.6, n1 * 0.75 + n2 * 0.35);
    vec3 rgb = textureLod(g_lut, vec2(value, 0.0), 0.0).rgb;
    float alpha = texture(g_tex, v_uv).a * v_color.a;
    o_color = vec4(rgb * alpha, alpha);
}

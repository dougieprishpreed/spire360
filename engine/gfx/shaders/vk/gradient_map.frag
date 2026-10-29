#version 450
// main.frag's gradient map alone (the game's grayscale and flipbook VFX
// shaders: the red through the gradient, the alpha from the red or the
// texel), as xost runs every instruction of its other modes; the effects'
// particles draw with it.
#include "common.glsl"

void main() {
    vec4 t = texture(g_tex, v_uv);
    vec3 straight = t.a > 0.0 ? spire_div(t.rgb, t.a) : vec3(0.0);
    t.rgb = g_premul.x > 0.5 ? straight : t.rgb;
    vec4 l = textureLod(g_lut, vec2(t.r, 0.5), 0.0);
    vec4 c = vec4(l.rgb, g_gm.y > 0.5 ? t.r : t.a) * v_color;
    o_color = vec4(c.rgb * c.a, c.a);
}

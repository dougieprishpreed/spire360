#version 450
// canvas.hlsl's ps_plain: the texel times the vertex colour (a capture's
// texel made straight first). Every division here and in the variants is
// by a value kept off 0 (spire_div): xost's select computes both sides,
// and an infinity or NaN on the side not taken still comes out.
#include "common.glsl"

void main() {
    vec4 t = texture(g_tex, v_uv);
    vec3 straight = t.a > 0.0 ? spire_div(t.rgb, t.a) : vec3(0.0);
    t.rgb = g_premul.x > 0.5 ? straight : t.rgb;
    vec4 c = t * v_color;
    o_color = vec4(c.rgb * c.a, c.a);
}

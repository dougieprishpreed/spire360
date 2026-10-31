#version 450
// plain.frag through a canvas group's mask: an additive child (light
// particles, sprites) of a group whose layer would hold light alone, as
// fire_mask.frag (c42..c44, g_group).
#include "common.glsl"

float spire_mask() {
    vec2 m = vec2(dot(P.c[42].xy, v_screen) + P.c[42].z, dot(P.c[43].xy, v_screen) + P.c[43].z);
    vec4 b = P.c[44];
    float inside = step(b.x, m.x) * step(m.x, b.z) * step(b.y, m.y) * step(m.y, b.w);
    return texture(g_group, m).a * P.c[42].w * inside;
}

void main() {
    vec4 t = texture(g_tex, v_uv);
    vec3 straight = t.a > 0.0 ? spire_div(t.rgb, t.a) : vec3(0.0);
    t.rgb = g_premul.x > 0.5 ? straight : t.rgb;
    vec4 c = t * v_color;
    float a = c.a * spire_mask();
    o_color = vec4(c.rgb * a, a);
}

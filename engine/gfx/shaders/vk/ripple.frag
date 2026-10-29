#version 450
// main.frag's card ripple alone (Godot's card_ripple.gdshader, the alpha as
// a distance field: the highlight around a card that can be played or
// picked), as xost runs every instruction of its other modes.
#include "common.glsl"

void main() {
    vec4 t = texture(g_tex, v_uv);
    vec3 straight = t.a > 0.0 ? spire_div(t.rgb, t.a) : vec3(0.0);
    t.rgb = g_premul.x > 0.5 ? straight : t.rgb;
    vec4 c = t * v_color;
    float sdf = fmod(g_ripple.x + c.a, max(g_ripple.y, 1.0e-4));
    float b = c.a - sdf + smoothstep(0.0, g_ripple.z, sdf) * g_ripple.y;
    c.a = smoothstep(1.0 - g_ripple.w, 1.0, b);
    o_color = vec4(c.rgb * c.a, c.a);
}

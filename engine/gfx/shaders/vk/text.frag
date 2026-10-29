#version 450
// canvas.hlsl's ps_text: MSDF glyphs (Godot's canvas shader), filled or
// outlined, in the vertex colour.
#include "common.glsl"

void main() {
    vec4 t = texture(g_tex, v_uv);
    vec2 dest = 1.0 / fwidth(v_uv);
    float px_size = max(0.5 * dot(g_msdf.xx / g_msdf.zw, dest), 1.0);
    float d = max(min(t.r, t.g), min(max(t.r, t.g), t.b));
    float cr = clamp(g_msdf.y, 0.0, g_msdf.x / 2.0 - 1.0) / g_msdf.x;
    float outlined = saturate((min(d, t.a) - 0.5 + cr) * px_size);
    float filled = saturate((d - 0.5) * px_size + 0.5);
    float ca = (g_msdf.y > 0.0 ? outlined : filled) * v_color.a;
    o_color = vec4(v_color.rgb * ca, ca);
}

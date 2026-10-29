#version 450
// canvas.hlsl's ps_main past its screen, fire and erosion branches: the
// gradient map, then the canvas group's layer through the mask's alpha
// (Godot's clip_children), or MSDF glyphs, the colour matrix and the card
// ripple.
#include "common.glsl"

void main() {
    vec4 t = texture(g_tex, v_uv);
    vec3 straight = t.a > 0.0 ? spire_div(t.rgb, t.a) : vec3(0.0);
    t.rgb = g_premul.x > 0.5 ? straight : t.rgb;
    // the game's grayscale / flipbook VFX shaders: the red through the
    // gradient, the alpha from the red or the texel
    vec4 l = textureLod(g_lut, vec2(t.r, 0.5), 0.0);
    vec4 mapped = vec4(l.rgb, g_gm.y > 0.5 ? t.r : t.a);
    t = g_gm.x > 0.5 ? mapped : t;
    vec4 result;
    if (g_mode.z > 0.5) {
        result = texture(g_group, v_screen) * (t.a * v_color.a);
    } else {
        vec2 dest = 1.0 / max(fwidth(v_uv), vec2(1.0e-6));
        float px_size = max(0.5 * dot(g_msdf.xx / g_msdf.zw, dest), 1.0);
        float d = max(min(t.r, t.g), min(max(t.r, t.g), t.b));
        float cr = clamp(g_msdf.y, 0.0, g_msdf.x / 2.0 - 1.0) / g_msdf.x;
        float outlined = saturate((min(d, t.a) - 0.5 + cr) * px_size);
        float filled = saturate((d - 0.5) * px_size + 0.5);
        vec4 glyph = vec4(1.0, 1.0, 1.0, g_msdf.y > 0.0 ? outlined : filled);
        t = g_mode.y > 0.5 ? glyph : t;
        vec3 m = vec3(dot(g_cm0.xyz, t.rgb) + g_cm0.w, dot(g_cm1.xyz, t.rgb) + g_cm1.w,
                      dot(g_cm2.xyz, t.rgb) + g_cm2.w);
        t.rgb = g_mode.x > 0.5 ? m : t.rgb;
        vec4 c = t * v_color;
        // Godot's card_ripple.gdshader: the alpha as a distance field
        float sdf = fmod(g_ripple.x + c.a, max(g_ripple.y, 1.0e-4));
        float b = c.a - sdf + smoothstep(0.0, g_ripple.z, sdf) * g_ripple.y;
        c.a = g_mode.w > 0.5 ? smoothstep(1.0 - g_ripple.w, 1.0, b) : c.a;
        result = vec4(c.rgb * c.a, c.a);
    }
    o_color = result;
}

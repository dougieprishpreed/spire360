#version 450
// canvas.hlsl's screen-reading branch, one SCREEN_* mode a shader: the
// build compiles this file once per mode (vk_shaders.py: screen_<mode>.frag
// with MODE_<NAME> defined), as xost runs both arms of an if whose arm
// holds a loop. The vertex colour is not applied unless a mode says so.
#include "common.glsl"

void main() {
#if defined(MODE_AT_UV)
    // shaders/vfx/screencap.tres: the copy at the quad's own uv
    o_color = textureLod(g_scrA, v_uv, 0.0);
#elif defined(MODE_CHROMA)
    // vfx_screen_chromatic_aberration: red and blue read along the quad's
    // radius by the distance curve and the texel's red
    vec2 dir = vec2(0.5) - v_uv;
    float len = length(dir);
    vec2 nd = len > 1e-6 ? dir / len : vec2(0.0);
    float k = g_scr.w * textureLod(g_scrNoise, vec2(len, len), 0.0).r * textureLod(g_tex, v_uv, 0.0).r;
    float r = textureLod(g_scrA, v_screen + nd * 0.5 * k, 0.0).r;
    float g = textureLod(g_scrA, v_screen, 0.0).g;
    float b = textureLod(g_scrA, v_screen + nd * k, 0.0).b;
    o_color = vec4(r, g, b, 1.0);
#elif defined(MODE_DISTORT)
    // vfx_screen_distortion_outward, vfx_scream_distortion_polar: pushed
    // toward the middle of the quad by the texel's red (polar: its alpha
    // times its red at the polar uv), times the vertex alpha
    vec4 t = textureLod(g_tex, v_uv, 0.0);
    vec2 d = v_uv - 0.5;
    vec2 p = vec2(length(d) * 2.0, atan2(d.y, d.x) / 6.2831853) * g_scr2.zw + g_scr2.xy;
    float polar = t.a * textureLod(g_tex, p, 0.0).r;
    float m = g_scr.y > 0.5 ? polar : t.r;
    vec2 dir = vec2(0.5) - v_uv;
    float len = length(dir);
    float k = len > 1e-6 ? g_scr.w * v_color.a * m / len : 0.0;
    o_color = textureLod(g_scrA, v_screen + dir * k, 0.0);
#elif defined(MODE_DOOM)
    // doom_overlay: desaturated and darkened by the vertex alpha in YIQ, a
    // pulsing gradient rising from the bottom of the screen
    float a = v_color.a;
    float sf = mix(1.0, g_scr.y, a), vf = mix(1.0, g_scr.z, a);
    vec4 sc = textureLod(g_scrA, v_screen, 0.0);
    float y = dot(vec3(0.2989, 0.5870, 0.1140), sc.rgb) * vf;
    float ci = dot(vec3(0.5959, -0.2774, -0.3216), sc.rgb) * sf * vf;
    float cq = dot(vec3(0.2115, -0.5229, 0.3114), sc.rgb) * sf * vf;
    float y2 = v_screen.y * v_screen.y;
    float g = y2 * y2 * y2 * g_scr.w * a;
    vec3 rgb = vec3(y + 0.956 * ci + 0.621 * cq, y - 0.272 * ci - 0.647 * cq, y - 1.106 * ci + 1.703 * cq);
    o_color = saturate(vec4(rgb, sc.a) + g * g_scr2);
#elif defined(MODE_GLASS)
    // the Ancient cards' glass border: the screen behind, brighter, its
    // alpha the mask's, toward the corner colour by the mask's green
    vec4 m = textureLod(g_scrMask, v_uv, 0.0);
    vec4 sc = mix(textureLod(g_scrA, v_screen, 0.0), textureLod(g_scrB, v_screen, 0.0), g_scr.y);
    vec3 rgb = (sc.a > 0.0 ? spire_div(sc.rgb, sc.a) : vec3(0.0)) + g_scr.w;
    vec4 f = mix(vec4(rgb, m.a), g_scr2, m.g * g_scr.z) * v_color;
    float a = saturate(f.a * texture(g_tex, v_uv).a * m.a);
    o_color = vec4(saturate(f.rgb) * a, a);
#elif defined(MODE_RADIAL)
    // radial_blur: the mean of count reads toward the centre; the count is
    // the block's int, the one data loop xost counts exactly
    vec2 dir = v_screen - g_scr2.xy;
    vec3 acc = vec3(0.0);
    for (int r = 0; r < P.count; r += 1) acc += textureLod(g_scrA, v_screen - g_scr.z * dir * float(r), 0.0).rgb;
    o_color = vec4(acc / max(g_scr.y, 1.0), 1.0);
#elif defined(MODE_LEVEL)
    // a level of the screen copy: the 7-tap Gaussian across and down of
    // the bilinear reads of the level above (Godot's back buffer mipmaps)
    // (the weights by tap, as xost has no integer abs)
    const float k[7] = float[7](0.071303, 0.131514, 0.189879, 0.214607, 0.189879, 0.131514, 0.071303);
    vec4 s = vec4(0.0);
    for (int y = 0; y < 7; ++y)
        for (int x = 0; x < 7; ++x)
            s += textureLod(g_scrA, v_uv + vec2(x - 3, y - 3) * g_scr2.zw, 0.0) * (k[x] * k[y]);
    o_color = s;
#elif defined(MODE_DARK)
    // dark_blur: the copy at its lod (two levels blended), mixed toward
    // opaque black on the straight colour
    vec4 c = mix(textureLod(g_scrA, v_screen, 0.0), textureLod(g_scrB, v_screen, 0.0), g_scr.y);
    float na = c.a * (1.0 - g_scr.z) + g_scr.z;
    float ka = c.a > 0.0 ? spire_div((1.0 - g_scr.z) * na, c.a) : 0.0;
    o_color = vec4(c.rgb * ka, na);
#elif defined(MODE_NOISE)
    // vfx_water_reflection_post, the_insatiable_sand_fall_2: the screen
    // read through the noise, times the mask's alpha
    float n = textureLod(g_scrNoise, v_uv + g_scr2.xy, 0.0).r;
    float d = n * g_scr.w - g_scr.w * 0.5;
    vec4 sc = textureLod(g_scrA, v_screen + d, 0.0);
    o_color = sc * textureLod(g_scrMask, v_uv, 0.0).a;
#elif defined(MODE_COPY)
    // the scene's texels as they are: the copy's first level
    o_color = textureLod(g_tex, v_uv, 0.0);
#elif defined(MODE_PRESENT)
    // the frame on the display: each channel through the display ramp (a
    // 256x1 texture read nearest at its texels' centres), as the XDK
    // build's D3D gamma ramp did
    vec3 c = textureLod(g_tex, v_uv, 0.0).rgb * (255.0 / 256.0) + 0.5 / 256.0;
    float r = textureLod(g_lut, vec2(c.r, 0.5), 0.0).r;
    float g = textureLod(g_lut, vec2(c.g, 0.5), 0.0).r;
    float b = textureLod(g_lut, vec2(c.b, 0.5), 0.0).r;
    o_color = vec4(r, g, b, 1.0);
#endif
}

#version 450
// canvas.hlsl's stepped fire branch: the game's stepped fire shaders
// (shaders/vfx/vfx_stepped_shader_fire_*), or the doom bar's noise.
#include "common.glsl"

void main() {
    vec4 result;
    if (g_fstr.w > 1.5) {
        // the doom bar: the scene's noise texture, sampled in screen space
        float tm = g_fstr.x;
        float scroll = textureLod(g_fn1, v_screen * 0.1 + tm * 0.0005 + 0.24, 0.0).r;
        float n1 = textureLod(g_fn1, v_screen + tm * 0.0005 + scroll, 0.0).r;
        float n2 = textureLod(g_fn1, v_screen - tm * 0.0007 + scroll, 0.0).r;
        float value = smoothstep(0.4, 0.6, n1 * 0.75 + n2 * 0.35);
        vec3 rgb = textureLod(g_lut, vec2(value, 0.0), 0.0).rgb;
        float alpha = texture(g_tex, v_uv).a * v_color.a;
        result = vec4(rgb * alpha, alpha);
    } else {
        vec2 uv = v_uv;
        float n2 = g_fstr.y * textureLod(g_fn2, uv * g_fn2p.xy + g_fn2p.zw, 0.0).b;
        float n1 = g_fstr.x * textureLod(g_fn1, uv * g_fn1p.xy + g_fn1p.zw, 0.0).b;
        float m = textureLod(g_fmask, (uv - vec2(0.0, 0.04)) * g_fmaskp.xy + vec2(0.0, 0.04) + g_fmaskp.zw, 0.0).b;
        m = mix(m, 1.0 - m, g_fstr.z);
        float a = n1 * n2 + m;
        vec2 d = textureLod(g_fd2, uv * g_fd2p.xy + g_fd2p.zw, 0.0).xy * g_fdstr.y +
                 textureLod(g_fd1, uv * g_fd1p.xy + g_fd1p.zw, 0.0).xy * g_fdstr.x;
        vec2 dm = textureLod(g_fdmask, uv * g_fdmaskp.xy + g_fdmaskp.zw, 0.0).xy;
        float v = a * textureLod(g_tex, uv + d * dm, 0.0).b;
        vec3 dark = g_fouter.rgb * (smoothstep(g_fsteps.x, g_fsteps.y, 1.0 - v) + g_finner.rgb);
        vec3 bright = g_fouter.rgb + smoothstep(g_fsteps.x, g_fsteps.y, v) * g_finner.rgb;
        vec3 rgb = g_fstr.w > 0.5 ? dark : bright;
        float fa = saturate(smoothstep(g_fsteps.z, g_fsteps.w, v)) * v_color.a;
        result = vec4(rgb * fa, fa);
    }
    o_color = result;
}

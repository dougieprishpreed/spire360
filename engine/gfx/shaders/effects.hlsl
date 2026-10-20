// Effect shaders for the D3D9 renderer (engine/gfx/effects.h): one pixel
// shader per ported game shader, entry point ps_e<id>, used with the canvas
// shader's vertex shader. Output premultiplied, as the canvas shader's.
//
// s0: the draw's texture (TEXTURE); s3..s6: the effect's textures
// (EffectParams.tex); s9: the screen copy when the effect reads it.
// c24..c27: EffectParams.p[0..15].
//
// PC: compiled at startup with D3DCompile. Xbox 360: effects_build.cmd
// (tools/gen_shader_inc.py writes it) runs the XDK's fxc for each entry.

struct VsOut {
    float4 pos : POSITION;
    float2 uv : TEXCOORD0;
    float4 color : COLOR0;
    float2 screen : TEXCOORD1;
};

sampler2D g_tex : register(s0);
sampler2D g_e0 : register(s3);
sampler2D g_e1 : register(s4);
sampler2D g_e2 : register(s5);
sampler2D g_e3 : register(s6);
sampler2D g_screen : register(s9);
float4 g_p0 : register(c24);
float4 g_p1 : register(c25);
float4 g_p2 : register(c26);
float4 g_p3 : register(c27);

float4 premul(float4 c) { return float4(c.rgb * c.a, c.a); }

// 1: vfx_water_reflection: the texture where the mask is (nearly) clear,
// else read through the noise; no vertex colour.
float4 ps_e1(VsOut i) : COLOR {
    float n = tex2D(g_e0, i.uv + g_p0.xx).r;
    float m = tex2D(g_e1, i.uv).a;
    float2 uv = m <= 0.1 ? i.uv : i.uv - g_p0.y * 0.5 + n * g_p0.y * 0.5;
    return premul(tex2D(g_tex, uv));
}

// 2: scrolling_smoke: the smoke scrolling up, its alpha thresholded
// through the gradient; no vertex colour.
float4 ps_e2(VsOut i) : COLOR {
    float4 sm = tex2D(g_e0, i.uv + float2(0.0, g_p0.x));
    float a = saturate(smoothstep(g_p0.y, g_p0.z, sm.a * tex2D(g_e1, i.uv).r) * g_p0.w);
    return float4(g_p1.rgb * sm.rgb * a, a);
}

// 3: vfx_noise_scale_shader (fragment): the lut by the texture's red.
float4 ps_e3(VsOut i) : COLOR {
    float4 t = tex2D(g_tex, i.uv);
    float4 l = tex2D(g_e0, float2(t.r, 0.5));
    float a = saturate(i.color.a * lerp(t.a, t.r, g_p0.x));
    return float4(saturate(l.rgb * i.color.rgb) * a, a);
}

// 4: simple_noise_flat / simple_noise_add: the vertex colour, the texture's
// red through two panning noises, thresholded.
float4 ps_e4(VsOut i) : COLOR {
    float t = tex2D(g_tex, i.uv).r;
    float n1 = tex2D(g_e0, (i.uv + float2(0.275, 0.0)) * g_p0.zw - float2(0.275, 0.0) + g_p1.xy).r;
    float n2 = tex2D(g_e1, (i.uv + float2(0.275, 0.0)) * g_p1.zw - float2(0.275, 0.0) + g_p2.xy).r;
    float a = saturate(smoothstep(g_p0.x, g_p0.y, i.color.a * t * n1 * n2));
    return float4(i.color.rgb * a, a);
}

// 5: decimillipede_suck_shader: dark blue, its alpha from the pattern
// pulled through the gradient and the shape.
float4 ps_e5(VsOut i) : COLOR {
    float4 p = tex2D(g_e0, i.uv * 3.0 + g_p0.xy) * tex2D(g_e0, i.uv * 2.0 + g_p0.zw);
    float g = tex2D(g_e1, i.uv + float2(0.2, -0.05) + p.rg).r;
    float s = tex2D(g_e2, i.uv + float2(p.r * 0.03, 0.0)).r;
    float a = saturate(smoothstep(0.4, 0.6, g) * smoothstep(0.4, 0.7, s) * g_p1.x);
    return float4(float3(0.0153, 0.090072, 0.17) * a, a);
}

// 6: vfx_molten_fist: two noises over a mask, thresholded; the vertex
// colour (the shader writes only COLOR.a).
float4 ps_e6(VsOut i) : COLOR {
    float n1 = tex2D(g_e0, i.uv * g_p1.xy + g_p1.zw).b;
    float n2 = tex2D(g_e1, i.uv * g_p2.xy + g_p2.zw).b;
    float m = tex2D(g_e2, (i.uv - float2(0.0, 0.04)) * g_p3.xy + float2(0.0, 0.04) + g_p3.zw).b;
    if (g_p0.w < 0.0) m = 1.0 - m;
    float x = (g_p0.z * n2 * abs(g_p0.w) * n1 + m) * tex2D(g_e3, i.uv).a;
    float a = saturate(smoothstep(g_p0.x, g_p0.y, x));
    return float4(i.color.rgb * a, a);
}

// 7: slash_shader_flat: a flat colour, the shape through two noises.
float4 ps_e7(VsOut i) : COLOR {
    float sh = tex2D(g_e0, i.uv).b;
    float n1 = tex2D(g_e1, (i.uv + float2(0.275, 0.0)) * g_p1.yz - float2(0.275, 0.0) + float2(g_p1.w, g_p2.x)).r;
    float n2 = tex2D(g_e2, (i.uv + float2(0.275, 0.0)) * float2(g_p2.y, g_p2.z) - float2(0.275, 0.0) + float2(g_p3.w, g_p2.w)).r;
    float a = saturate(smoothstep(g_p0.x, g_p0.y, sh * g_p0.z * n1 * n2 * g_p0.w) * g_p1.x);
    return float4(g_p3.rgb * a, a);
}

// 8: texture_transition: the transition texture's step, faded; the
// vertex colour (the shader writes only COLOR.a).
float4 ps_e8(VsOut i) : COLOR {
    float r = tex2D(g_e0, i.uv).r;
    float a = saturate((((1.0 - r) <= lerp(-0.1, 1.1, g_p0.x) ? 1.0 : 0.0) + g_p0.x * g_p0.y) * g_p0.z);
    return float4(i.color.rgb * a, a);
}

// 9: vfx_panning_shader: the texture panning, coloured by the lut, eroded
// over the particle's life, masked.
float4 ps_e9(VsOut i) : COLOR {
    float4 t = tex2D(g_tex, i.uv * g_p0.xy + g_p0.zw);
    float4 l = tex2D(g_e0, float2(t.r, 0.5));
    float a = saturate(smoothstep(g_p1.x, g_p1.x + g_p1.y, lerp(t.a, t.r, g_p1.w)) * i.color.a * tex2D(g_e1, i.uv).r);
    return float4(saturate(l.rgb * i.color.rgb * g_p1.z) * a, a);
}

// 10: vfx_ray_shader (fragment): the lut, faded along the ray.
float4 ps_e10(VsOut i) : COLOR {
    float4 t = tex2D(g_tex, i.uv);
    float4 l = tex2D(g_e0, float2(t.r, 0.5));
    float a = saturate(i.color.a * t.a * tex2D(g_e1, i.uv.yx).r);
    return float4(saturate(l.rgb * i.color.rgb) * a, a);
}

// 11: vfx_dagger_spray_single_shader: the lut with its own alpha.
float4 ps_e11(VsOut i) : COLOR {
    float4 t = tex2D(g_tex, i.uv);
    float4 l = tex2D(g_e0, float2(t.r, 0.5));
    float a = saturate(l.a * t.a * i.color.a);
    return float4(saturate(l.rgb * i.color.rgb) * a, a);
}

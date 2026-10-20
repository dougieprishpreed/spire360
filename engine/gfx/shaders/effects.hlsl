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
    float4 t = tex2D(g_tex, i.uv);
    float n1 = tex2D(g_e0, (i.uv + float2(0.275, 0.0)) * g_p0.zw - float2(0.275, 0.0) + g_p1.xy).r;
    float n2 = tex2D(g_e1, (i.uv + float2(0.275, 0.0)) * g_p1.zw - float2(0.275, 0.0) + g_p2.xy).r;
    float a = saturate(smoothstep(g_p0.x, g_p0.y, i.color.a * t.a * t.r * n1 * n2));
    return float4(saturate(i.color.rgb * t.rgb) * a, a);
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

// 6: vfx_molten_fist: two noises over a mask, thresholded; COLOR's rgb
// (the shader writes only COLOR.a).
float4 ps_e6(VsOut i) : COLOR {
    float4 t = tex2D(g_tex, i.uv);
    float n1 = tex2D(g_e0, i.uv * g_p1.xy + g_p1.zw).b;
    float n2 = tex2D(g_e1, i.uv * g_p2.xy + g_p2.zw).b;
    float m = tex2D(g_e2, (i.uv - float2(0.0, 0.04)) * g_p3.xy + float2(0.0, 0.04) + g_p3.zw).b;
    if (g_p0.w < 0.0) m = 1.0 - m;
    float x = (g_p0.z * n2 * abs(g_p0.w) * n1 + m) * tex2D(g_e3, i.uv).a;
    float a = saturate(smoothstep(g_p0.x, g_p0.y, x));
    return float4(saturate(i.color.rgb * t.rgb) * a, a);
}

// 7: slash_shader_flat: a flat colour, the shape through two noises.
float4 ps_e7(VsOut i) : COLOR {
    float sh = tex2D(g_e0, i.uv).b;
    float n1 = tex2D(g_e1, (i.uv + float2(0.275, 0.0)) * g_p1.yz - float2(0.275, 0.0) + float2(g_p1.w, g_p2.x)).r;
    float n2 = tex2D(g_e2, (i.uv + float2(0.275, 0.0)) * float2(g_p2.y, g_p2.z) - float2(0.275, 0.0) + float2(g_p3.w, g_p2.w)).r;
    float a = saturate(smoothstep(g_p0.x, g_p0.y, sh * g_p0.z * n1 * n2 * g_p0.w) * g_p1.x);
    return float4(g_p3.rgb * a, a);
}

// 8: texture_transition: the transition texture's step, faded; COLOR's
// rgb (the shader writes only COLOR.a).
float4 ps_e8(VsOut i) : COLOR {
    float4 t = tex2D(g_tex, i.uv);
    float r = tex2D(g_e0, i.uv).r;
    float a = saturate((((1.0 - r) <= lerp(-0.1, 1.1, g_p0.x) ? 1.0 : 0.0) + g_p0.x * g_p0.y) * g_p0.z);
    return float4(saturate(i.color.rgb * t.rgb) * a, a);
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

// 12: dissolve (NMonsterDeathVfx): a capture (premultiplied), warped up and
// to the right as the threshold falls, eaten through two noises.
float dissolve_strength(float2 uv, float offset) {
    float2 scaling = (1.0 / 400.0) / g_p0.zw;
    uv.x += offset;
    float2 at = uv * scaling + float2(g_p0.y, uv.x * uv.x * uv.x);
    float s1 = (tex2D(g_e0, at).r + uv.x) * 0.5;
    float s2 = (tex2D(g_e1, at).r + uv.x) * 0.5;
    return g_p0.x >= lerp(s1, s2, 0.4) ? 1.0 : 0.0;
}

float4 ps_e12(VsOut i) : COLOR {
    float2 warp = float2(-200.0, 1024.0) * g_p0.zw;
    float2 o = max(i.uv.x - (g_p0.x * 1.9 - 0.55), 0.0) * warp;
    float4 c = tex2D(g_tex, i.uv + o * o);
    c.rgb *= dissolve_strength(i.uv, 0.05);
    c.rgb = lerp(dot(c.rgb, float3(0.299, 0.587, 0.114)).xxx, c.rgb, g_p0.x);
    return c * dissolve_strength(i.uv, 0.0);
}

// shift_hue (shaders/vfx/_util/hue_shift.gdshaderinc)
float3 shift_hue(float3 c, float shift) {
    float3 gray = float3(0.57735, 0.57735, 0.57735);
    float3 p = gray * dot(gray, c);
    float3 u = c - p;
    float3 v = cross(gray, u);
    return u * cos(shift * 6.2831853) + v * sin(shift * 6.2831853) + p;
}

// 13: vfx_card_exhaust_dissolve: the card's viewport (as stored) eaten by
// the erosion texture, its edge through the bright lut.
float4 ps_e13(VsOut i) : COLOR {
    float4 t = tex2D(g_tex, i.uv);
    float r = tex2D(g_e0, i.uv + float2(g_p0.y, 0.0)).r;
    float erosion = r >= g_p0.x ? 1.0 : 0.0;
    float bright = smoothstep(g_p0.x, g_p0.x + g_p0.z, r);
    float3 bc = shift_hue(tex2D(g_e1, float2(bright, 0.5)).rgb, g_p0.w);
    float3 rgb = lerp(t.rgb, bc, 1.0 - bright);
    float a = saturate(t.a * i.color.a * erosion);
    return float4(saturate(rgb * i.color.rgb) * a, a);
}

// 14: vfx_card_exhaust_quick_erosion: the texture's red eroded, coloured by
// the bright lut.
float4 ps_e14(VsOut i) : COLOR {
    float4 t = tex2D(g_tex, i.uv);
    float erosion = smoothstep(g_p0.x, g_p0.x + g_p0.y, t.r);
    float bright = smoothstep(g_p0.x, g_p0.x + g_p0.z, t.r);
    float3 bc = shift_hue(tex2D(g_e0, float2(bright, 0.5)).rgb, g_p0.w);
    float a = saturate(t.a * i.color.a * erosion);
    return float4(saturate(bc * i.color.rgb) * a, a);
}

// 15: boss_map_point_placeholder: grey, tinted, three quarters opaque.
float4 ps_e15(VsOut i) : COLOR {
    float4 t = tex2D(g_tex, i.uv);
    float g = dot(t.rgb, float3(0.299, 0.587, 0.114));
    float a = saturate(t.a * 0.75);
    return float4(saturate(g_p0.rgb * (0.7 + 0.3 * g)) * a, a);
}

// 16: enchantment_tab_appear: drawn in from the left as progress rises.
float4 ps_e16(VsOut i) : COLOR {
    float p = g_p0.x;
    float s1 = smoothstep(p, p + 0.2, i.uv.x), s2 = smoothstep(p + 0.2, p + 0.25, i.uv.x);
    float2 uv = float2(i.uv.x, lerp(i.uv.y, (i.uv.y - 0.5) * 0.75 + 0.5, s1));
    float4 t = tex2D(g_tex, uv);
    float3 add = float3(1.0, 0.75, 0.9);
    float4 c = lerp(t, float4(t.rgb + add, 1.0), s1);
    c = lerp(c, float4(add, 0.0), s2);
    float a = saturate(c.a * t.a);
    return float4(saturate(c.rgb) * a, a);
}

// 17: vfx_ui_low_hp_border: a vignette of the main colour through noise.
float4 ps_e17(VsOut i) : COLOR {
    float2 d = i.uv - 0.5;
    float2 polar = frac(float2(length(d) * 2.0, atan2(d.y, d.x) / 6.2832));
    float n = tex2D(g_e0, polar * g_p1.xy + g_p1.zw).r;
    float x = abs(i.uv.x - 0.5) * 2.0, y = abs(i.uv.y - 0.5) * 2.0;
    float q = 1.0 - (1.0 - sqrt(x * x + y * y) / g_p0.w) / (1.0 - g_p0.z);
    float fa = smoothstep(g_p2.x, g_p2.y, q * g_p0.x + n * 0.25) * g_p0.y;
    float a = saturate(fa * i.color.a);
    return float4(saturate(float3(g_p2.z, g_p2.w, g_p3.x) * i.color.rgb) * a, a);
}

// 18: vfx_sleeping_z: the Z wobbled and eroded through its animation
// texture, its outline coloured.
float4 ps_e18(VsOut i) : COLOR {
    float db = tex2D(g_e0, i.uv + float2(g_p0.w, g_p1.x)).b;
    float off = (db - 0.5) * 2.0 * g_p0.z;
    float4 t = tex2D(g_tex, i.uv + off);
    float eg = tex2D(g_e0, i.uv + g_p1.yy).g;
    float erosion = smoothstep(g_p0.x, g_p0.x + g_p0.y, eg);
    float4 c = lerp(float4(g_p1.zw, g_p2.xy), i.color, t.r);
    float a = saturate(c.a * erosion * t.a);
    return float4(saturate(c.rgb) * a, a);
}

// 19: vfx_heavy_blunt_erosion: the lut by the red, eroded over the life.
float4 ps_e19(VsOut i) : COLOR {
    float4 t = tex2D(g_tex, i.uv);
    float4 l = tex2D(g_e0, float2(t.r, 0.5));
    float a = saturate(i.color.a * smoothstep(g_p0.x, g_p0.x + g_p0.y, t.r));
    return float4(saturate(l.rgb * i.color.rgb) * a, a);
}

// 20: vfx_potion_splash: the texture wobbled by its own blue, eroded by
// its alpha over the life, coloured by the lut.
float4 ps_e20(VsOut i) : COLOR {
    float db = tex2D(g_tex, i.uv + g_p0.zw).b;
    float4 t = tex2D(g_tex, i.uv + db * g_p1.x);
    float4 l = tex2D(g_e0, float2(t.r, 0.5));
    float a = saturate(i.color.a * smoothstep(g_p0.x, g_p0.x + g_p0.y, t.a));
    return float4(saturate(l.rgb * i.color.rgb) * a, a);
}

// 21: Blur (a card portrait), both passes at once: 7 x 7 Gaussian taps.
float4 ps_e21(VsOut i) : COLOR {
    float w[7] = { 0.0702, 0.1311, 0.1907, 0.2160, 0.1907, 0.1311, 0.0702 };
    float2 r = g_p0.x * 32.0 / 3.0 * g_p0.yz;
    float3 acc = float3(0.0, 0.0, 0.0);
    for (int y = 0; y < 7; ++y)
        for (int x = 0; x < 7; ++x)
            acc += w[x] * w[y] * tex2D(g_tex, i.uv + float2(x - 3, y - 3) * r).rgb;
    return float4(saturate(acc * i.color.rgb) * i.color.a, i.color.a);
}

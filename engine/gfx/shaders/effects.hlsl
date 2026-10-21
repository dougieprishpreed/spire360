// Effect shaders for the D3D9 renderer (engine/gfx/effects.h): one pixel
// shader per ported game shader, entry point ps_e<id>, used with the canvas
// shader's vertex shader. Output premultiplied, as the canvas shader's.
//
// s0: the draw's texture (TEXTURE); s3..s7: the effect's textures
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
sampler2D g_e4 : register(s7);
sampler2D g_screen : register(s9);
float4 g_p0 : register(c24);
float4 g_p1 : register(c25);
float4 g_p2 : register(c26);
float4 g_p3 : register(c27);
float4 g_x[17] : register(c29); // EffectParams.extra

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

// fract polar coordinates about the middle: (distance * 2, angle / 2 pi).
float2 polar_uv(float2 uv) {
    float2 d = uv - 0.5;
    return frac(float2(length(d) * 2.0, atan2(d.y, d.x) / 6.2831853));
}

// 22: vfx_goopy_impact_bits: a flipbook frame wobbled by the texture's
// blue, coloured by the lut, eroded by its green over the life.
float4 ps_e22(VsOut i) : COLOR {
    float off = (tex2D(g_tex, i.uv + g_p0.zw).b - 0.5) * 2.0 * g_p1.x;
    float f = floor(g_p1.y);
    float2 fuv = (i.uv + float2(fmod(f, g_p1.z), floor(f / g_p1.z))) / g_p1.zw;
    float4 t = tex2D(g_tex, fuv + off);
    float4 l = tex2D(g_e0, t.rr);
    float a = saturate(smoothstep(g_p0.x, g_p0.x + g_p0.y, t.g) * i.color.a);
    return float4(saturate(l.rgb * i.color.rgb) * a, a);
}

// 23: the Bound affliction's card: the texture's red brightened where its
// tiled panning copy is green.
float4 ps_e23(VsOut i) : COLOR {
    float4 t = tex2D(g_tex, i.uv);
    float bg = tex2D(g_tex, i.uv * g_p0.xy + g_p0.zw).g;
    float a = saturate(t.a * i.color.a);
    return float4(saturate(t.r * i.color.rgb * lerp(1.0, g_p1.x, bg)) * a, a);
}

// 24: the Bound affliction's border: the texture in panning polar
// coordinates, its alpha the mask's eroded.
float4 ps_e24(VsOut i) : COLOR {
    float4 t = tex2D(g_tex, polar_uv(i.uv) * g_p0.xy + g_p0.zw);
    float a = saturate(smoothstep(g_p1.x, g_p1.x + g_p1.y, i.color.a * tex2D(g_e0, i.uv).a));
    return float4(saturate(t.rgb * i.color.rgb) * a, a);
}

// 25: vfx_scream_squiggly_ring_polar: a polar ring pushed about by its own
// polar distortion, eroded over the life.
float4 ps_e25(VsOut i) : COLOR {
    float4 dm = tex2D(g_tex, polar_uv(i.uv) * g_p0.zw + g_p1.xy + g_p1.z);
    float2 dir = 0.5 - i.uv;
    float len = length(dir);
    float k = len > 1e-6 ? dm.g * dm.b * g_p1.w / len : 0.0;
    float2 q = polar_uv(i.uv + dir * k) * g_p2.xy + float2(g_p2.z + g_p2.w, 0.0);
    float ring = tex2D(g_tex, q).r;
    float a = saturate(i.color.a * tex2D(g_tex, i.uv).a * smoothstep(g_p0.x, g_p0.x + g_p0.y, ring));
    return float4(saturate(i.color.rgb) * a, a);
}

// 26: fire_shader_1 (the queen's flames): two noises bend a gradient, the
// shape mask wobbled by a third; a flat colour.
float4 ps_e26(VsOut i) : COLOR {
    float n1 = tex2D(g_e0, i.uv * g_p0.z + g_p0.xy).r;
    float n2 = tex2D(g_e1, i.uv * g_p1.y + float2(g_p0.w, g_p1.x)).r;
    float w = (g_p1.z + n1) * (n2 + g_p1.w);
    float g = tex2D(g_e2, i.uv + float2(-1.065, -0.24) + w).r;
    float2 mn = tex2D(g_e3, i.uv + g_p2.xy).rg * g_p2.z;
    float sh = tex2D(g_e4, i.uv + mn).r;
    float a = saturate(smoothstep(g_p2.w, g_p3.x, g * sh));
    return float4(saturate(g_p3.yzw) * a, a);
}

// polar_coordinates with a zoom.
float2 polar_zoom(float2 uv, float zoom) {
    float2 d = uv - 0.5;
    return frac(float2(length(d) * 2.0 * zoom, atan2(d.y, d.x) / 6.2832));
}

// 27: simple_distortion_flat: the texture read through a panning noise,
// tinted (no vertex colour).
float4 ps_e27(VsOut i) : COLOR {
    float n = tex2D(g_e0, i.uv + g_p0.xy).r;
    float4 t = tex2D(g_tex, i.uv + g_p0.zw * n);
    float a = saturate(t.a);
    return float4(saturate(g_p1.xyz * t.rgb) * a, a);
}

// 28: the Entangled (and Tainted) affliction: vines wobbled sideways,
// coloured by the lut.
float4 ps_e28(VsOut i) : COLOR {
    float d = tex2D(g_tex, i.uv * g_p0.xy + g_p0.zw).b * g_p1.x;
    float g = tex2D(g_tex, i.uv).g;
    float4 t = tex2D(g_tex, i.uv + float2(d * g, 0.0));
    float4 l = tex2D(g_e0, t.rr);
    float a = saturate(i.color.a * t.a);
    return float4(saturate(i.color.rgb * l.rgb) * a, a);
}

// 29: 2d_sphere_shader (additive): the texture panning round the sphere,
// masked at the sides and top / bottom.
float4 ps_e29(VsOut i) : COLOR {
    float2 n = tex2D(g_e0, i.uv * g_p0.zw + g_p0.xy).rg * g_p1.x;
    float g = tex2D(g_e1, i.uv).g;
    float4 t = tex2D(g_tex, i.uv + g_p1.yz + float2(0.0, -0.075 * g) + n);
    float a = t.a * smoothstep(g_p2.z, g_p2.w, tex2D(g_e2, i.uv).r) * smoothstep(g_p3.x, g_p3.y, tex2D(g_e3, i.uv + g_p3.zw).r);
    a = saturate(a);
    return float4(saturate(float3(g_p1.w, g_p2.x, g_p2.y) + t.rgb) * a, a);
}

// 30: vfx_scream_ring_polar: a ring of the texture's red in polar
// coordinates, sliding over the life.
float4 ps_e30(VsOut i) : COLOR {
    float ring = tex2D(g_tex, polar_uv(i.uv) * g_p0.xy + float2(g_p0.z + g_p0.w, 0.0)).r;
    float a = saturate(i.color.a * ring * tex2D(g_tex, i.uv).a);
    return float4(saturate(i.color.rgb) * a, a);
}

// 31: the Hexed affliction: border, main and bright colours by the
// texture's red and a polar noise.
float4 ps_e31(VsOut i) : COLOR {
    float n = smoothstep(g_p1.x, g_p1.x + g_p1.y, tex2D(g_tex, polar_uv(i.uv) * g_p0.xy + g_p0.zw).b);
    float4 t = tex2D(g_tex, i.uv);
    float3 inner = lerp(float3(g_p1.z, g_p1.w, g_p2.x), float3(g_p2.y, g_p2.z, g_p2.w), n);
    float3 c = lerp(g_p3.xyz, inner, t.r);
    float a = saturate(t.a * i.color.a);
    return float4(saturate(c * i.color.rgb) * a, a);
}

// 32: the Ringing affliction: the rings blinking by the base alpha
// animation, a thin polar ring running round.
float4 ps_e32(VsOut i) : COLOR {
    float f = g_p0.x;
    float4 tr = tex2D(g_e1, float2(f, f));
    float4 rb = tex2D(g_e0, float2(f, f));
    float4 t = tex2D(g_tex, i.uv);
    float4 mp = tex2D(g_tex, polar_zoom(i.uv, 0.8) * 0.5 + float2(tr.r, 0.0));
    float a = max(max(t.r * rb.r, t.g * rb.g), max(t.b * rb.b, mp.a * tr.g));
    a = saturate(a);
    return float4(saturate(i.color.rgb) * a, a);
}

// 33: vfx_missile_core: a flipbook frame by time, coloured by the lut.
float4 ps_e33(VsOut i) : COLOR {
    float total = g_p0.y * g_p0.z;
    float f = floor(g_p0.x);
    f -= total * floor(f / total);
    float2 fuv = i.uv / g_p0.yz + float2(fmod(f, g_p0.y) / g_p0.y, floor((f + 0.5) / g_p0.y) / g_p0.z);
    float4 t = tex2D(g_tex, fuv);
    float4 l = tex2D(g_e0, t.rr);
    float a = saturate(t.a * i.color.a);
    return float4(saturate(l.rgb * i.color.rgb) * a, a);
}

// 34: vfx_noisy_dissolve: the sprite eaten by a panning noise.
float4 ps_e34(VsOut i) : COLOR {
    float n = tex2D(g_e0, i.uv + g_p0.zw).r;
    float4 t = tex2D(g_tex, i.uv);
    float a = saturate(smoothstep(g_p0.x, g_p0.x + g_p0.y, n) * t.a);
    return float4(saturate(i.color.rgb * t.rgb) * a, a);
}

// 35: the Smog affliction: two smoke layers (the uv turned a quarter),
// masked, eroded and coloured by the lut.
float4 ps_e35(VsOut i) : COLOR {
    float2 r = float2(i.uv.y, 1.0 - i.uv.x);
    float m1 = tex2D(g_tex, r * g_p0.xy + g_p0.zw).r;
    float m2 = tex2D(g_tex, r * g_p1.xy + g_p1.zw).r;
    float4 mk = tex2D(g_e1, i.uv);
    float a = smoothstep(g_p2.y, g_p2.y + g_p2.z, lerp(m1, m2, g_p2.x) * mk.r);
    float4 l = tex2D(g_e0, float2(a, a));
    float alpha = saturate(a * i.color.a + mk.g);
    return float4(saturate(i.color.rgb * l.rgb) * alpha, alpha);
}

// 36: the Entangled leaf: the texture swaying about its pivot.
float4 ps_e36(VsOut i) : COLOR {
    float b = tex2D(g_tex, i.uv * g_p0.xy + float2(g_p0.z, g_p0.z + g_p0.w)).b;
    float ang = 1.5707963 + g_p1.x * (b - 0.5) * 2.0 * 1.5707963;
    float s = sin(ang), c = cos(ang);
    float2 d = i.uv - g_p1.yz;
    float4 t = tex2D(g_tex, float2(d.x * s - d.y * c, d.x * c + d.y * s) + g_p1.yz);
    float4 l = tex2D(g_e0, t.rr);
    float a = saturate(i.color.a * t.a);
    return float4(saturate(i.color.rgb * l.rgb) * a, a);
}

// 37: the Vantom fight's oil: a noisy disc with its border, grooves and
// mana (effects.h).
float4 ps_e37(VsOut i) : COLOR {
    float2 st = (i.uv - 0.5) * 2.0;
    float d = length(st);
    float ang = atan2(st.x, st.y) / 6.2831853;
    float tb = g_p0.z, tb5 = frac(5.0 * tb);
    float nv1 = (tex2D(g_e0, float2(tb, frac(ang - tb))).r - 0.5) / 10.0;
    float nv = nv1 + (tex2D(g_e0, float2(frac(ang + tb5), tb)).r - 0.5) / 12.0;
    float bw = g_p2.x, R = g_p0.x + nv;
    float mm = 0.0;
    if (bw > 0.0) {
        float L = (d - R) / bw;
        mm = (step(d, R + bw) - step(d, R)) * L * L;
    }
    float gm = step(d, R + bw) - step(d, 0.0);
    float gn = tex2D(g_e1, frac(i.uv + g_p0.w)).r;
    float ga = gn * 3.0;
    float ry = -sin(ga) * st.x + cos(ga) * (st.y + g_p1.x);
    float md = g_p1.y != 0.0 ? ry - g_p1.y * floor(ry / g_p1.y) : 0.0;
    gm *= md < g_p1.z ? 1.0 : 0.0;
    float mr = g_p0.y, x = d - nv1 * d * 2.0;
    float mana = (smoothstep(-0.1, -0.05, x) - smoothstep(mr + 0.05, mr + 0.1, x)) * (d < R + bw / 1.5 ? 1.0 : 0.0);
    float q0 = g_p3.z, q1 = g_p3.w;
    float on = q1 >= 65536.0 ? 1.0 : 0.0;
    q1 -= 65536.0 * on;
    float4 mc = float4(floor(q0 / 256.0), q0 - 256.0 * floor(q0 / 256.0), floor(q1 / 256.0), q1 - 256.0 * floor(q1 / 256.0)) / 255.0;
    float4 border = float4(g_p2.zw, g_p3.xy);
    float4 c = 0.0;
    if (mm > 0.0) c = float4(border.rgb, border.a * mm);
    if (mana > 0.0 && on > 0.5) {
        float g = mr != 0.0 ? d / mr * g_p2.y : 0.0;
        c = float4(mc.rgb + mc.rgb * g, mana * mc.a);
    }
    if (gm > 0.0) c = lerp(c * max(mana, mm), border, g_p1.w);
    float a = saturate(c.a);
    return float4(saturate(c.rgb) * a, a);
}

// 38: the Galvanized affliction: lines wobbled about the middle by a
// polar distortion, glowing as the blink runs.
float4 ps_e38(VsOut i) : COLOR {
    float blink = smoothstep(g_p2.x, g_p2.x + g_p2.y, tex2D(g_tex, float2(i.uv.x * g_p0.x + g_p0.y, 0.0)).b);
    float4 dt = tex2D(g_tex, polar_uv(i.uv) * g_p0.zw + g_p1.xy);
    float2 dist = (i.uv + (dt.b - 0.5) * 2.0 * g_p1.z - 0.5) * dt.b * -g_p1.w;
    float4 t = tex2D(g_tex, i.uv + dist);
    float mask = smoothstep(g_p2.z, g_p2.z + g_p2.w, tex2D(g_tex, i.uv).a);
    float4 l = tex2D(g_e0, float2(blink, 0.0));
    float a = saturate(i.color.a * max(t.r, t.g * l.a * blink) * mask);
    return float4(saturate(l.rgb * i.color.rgb) * a, a);
}

// 39: vfx_sweeping_beam_flipbook: the flipbook on a quad tilted away in
// 3D, masked, coloured by the lut.
float4 ps_e39(VsOut i) : COLOR {
    float2 q = i.uv - 0.5;
    float wz = g_p0.z * q.x + g_p1.y * q.y + g_p2.x;
    float2 p = (float2(g_p0.x, g_p0.y) * q.x + float2(g_p0.w, g_p1.x) * q.y + float2(g_p1.z, g_p1.w)) * g_p2.y / max(wz, 1e-6)
             - g_p2.zw + 0.5;
    float inside = (wz > 0.0 && p.x >= 0.0 && p.x <= 1.0 && p.y >= 0.0 && p.y <= 1.0) ? 1.0 : 0.0;
    float total = g_p3.y * g_p3.z;
    float f = floor(g_p3.x);
    f -= total * floor(f / total);
    float2 fuv = p / g_p3.yz + float2(fmod(f, g_p3.y) / g_p3.y, floor((f + 0.5) / g_p3.y) / g_p3.z);
    float4 t = tex2D(g_tex, fuv);
    float4 l = tex2D(g_e0, t.rr);
    float a = saturate(l.a * i.color.a * t.a * tex2D(g_e1, p).r) * inside;
    return float4(saturate(l.rgb * i.color.rgb) * a, a);
}

// 40: scry_reveal (the Crystal Sphere): the ball's noisy circles and the
// fog's cells (their alphas in tex2) with borders between them.
float4 ps_e40(VsOut i) : COLOR {
    const float g = 11.0;
    float m = g_p0.z;
    float2 gp = ((i.uv * g - g * 0.5) * (1.0 + m * 2.0) - m) + g * 0.5;
    float2 f = floor(gp);
    float valid = (f.x >= 0.0 && f.y >= 0.0 && f.x < g && f.y < g) ? 1.0 : 0.0;
    float alpha = valid > 0.5 ? tex2Dlod(g_e2, float4((f + 0.5) / g, 0.0, 0.0)).r : 1.0;
    float border = 0.0;
    [unroll] for (int dx = -1; dx <= 1; ++dx) {
        [unroll] for (int dy = -1; dy <= 1; ++dy) {
            if (dx != 0 || dy != 0) {
                float2 n = f + float2(dx, dy);
                float inside = (n.x >= 0.0 && n.y >= 0.0 && n.x < g && n.y < g) ? 1.0 : 0.0;
                float an = tex2Dlod(g_e2, float4((n + 0.5) / g, 0.0, 0.0)).r;
                float2 tb = max(max(n - gp, 0.0), gp - (n + 1.0));
                border = max(border, inside * smoothstep(0.05, 0.02, length(tb)) * abs(alpha - an));
            }
        }
    }
    float2 c = i.uv - 0.5;
    float dist = length(c);
    float ang = (atan2(c.y, c.x) + 3.14159265) / 6.2831853;
    float n1 = pow(tex2D(g_e0, float2(dist * 2.0, ang + g_p1.z)).r, 0.8);
    float n2 = tex2D(g_e1, float2(dist * 0.5, ang + g_p1.w)).r + 0.4;
    float ns = n1 * n2 * 2.0;
    float3 col = g_x[0].rgb;
    float lastmask = 0.0;
    float2 h = 5.0 - abs(5.0 - f);
    float corner = (h.x + h.y < 3.0 || valid < 0.5) ? 1.0 : 0.0;
    [unroll] for (int k = 0; k < 8; ++k) {
        float4 d = g_x[9 + k];
        float circle = smoothstep(d.x * 0.5, (d.x + d.y) * 0.5, dist);
        float mask = lerp(1.0, ns, pow(max(circle, 1e-6), d.z)) * (1.0 - circle);
        float hp = fwidth(mask) * 0.5;
        mask = saturate((mask - (d.w - hp)) / max(hp * 2.0, 1e-6));
        mask = (abs(k - g_p0.y) < 0.5 && corner > 0.5) ? 0.0 : mask;
        float use = k < g_p0.x ? 1.0 : 0.0;
        lastmask = lerp(lastmask, mask, use);
        col = lerp(col, lerp(g_x[k + 1].rgb, col, mask), use);
    }
    lastmask = g_p0.y < 0.0 ? 1.0 - corner : lastmask;
    float a = saturate(max(alpha, border) * lastmask * i.color.a);
    return float4(saturate((col + float3(g_p0.w, g_p1.x, g_p1.y) * border) * i.color.rgb) * a, a);
}

// 41: crystal_sphere_item: a shine sweeping across as val goes 0 to 1.
float4 ps_e41(VsOut i) : COLOR {
    float4 t = tex2D(g_tex, i.uv) * i.color;
    float ang = i.uv.x - i.uv.y, o = lerp(-1.0, 1.5, g_p0.x);
    float add = (abs(ang - o) <= 0.17 ? 0.75 : 0.0) + (abs(ang - o + 0.3) <= 0.03 ? 0.5 : 0.0);
    float a = saturate(t.a);
    return float4(saturate(t.rgb + add) * a, a);
}

// 42: vfx_doom_flame_shader: the flame's blue through two panning noises
// over the inverted mask, read through two distortions; outer colour plus
// the stepped inner colour, the outer step as alpha (drawn additive).
float4 ps_e42(VsOut i) : COLOR {
    float n1 = tex2D(g_e0, i.uv * g_p0.xy + g_p0.zw).b;
    float n2 = tex2D(g_e1, i.uv * g_p1.xy + g_p1.zw).b;
    float m = tex2D(g_e2, (i.uv - float2(0.0, 0.04)) * g_p2.xy + float2(0.0, 0.04) + g_p2.zw).b;
    if (g_p3.y != 0.0) m = 1.0 - m;
    float2 d = tex2D(g_e3, i.uv * g_x[0].xy + g_x[0].zw).xy * g_x[2].x + tex2D(g_e4, i.uv * g_x[1].xy + g_x[1].zw).xy * g_x[2].y;
    float x = (g_p3.x * n1 * n2 + m) * tex2D(g_tex, i.uv + d).b;
    float a = saturate(smoothstep(g_x[2].z, g_x[2].w, x));
    return float4(saturate(g_x[3].rgb + smoothstep(g_p3.z, g_p3.w, x) * g_x[4].rgb) * a, a);
}

// 43: vfx_poison_skull_shader: the texture read through its own panning
// blue, coloured by the lut at its red, eroded by its green at the
// particle's life.
float4 ps_e43(VsOut i) : COLOR {
    float db = tex2D(g_tex, i.uv + g_p0.xy).b;
    float4 t = tex2D(g_tex, i.uv + float2(0.0, db * g_p0.z));
    float4 l = tex2D(g_e0, t.rr);
    float a = saturate(smoothstep(g_p0.w, g_p0.w + g_p1.x, t.g) * t.a * i.color.a);
    return float4(saturate(l.rgb * i.color.rgb) * a, a);
}

// 44: the demon / reaper form's panning fire: two panning noises plus the
// overlay through the mask, eroded, coloured by the lut (the reaper reads
// the noise in polar coordinates of the rotated uv).
float4 ps_e44(VsOut i) : COLOR {
    float2 u = i.uv;
    if (g_p2.w != 0.0) {
        float sa = sin(g_p3.x), ca = cos(g_p3.x);
        float2 d = i.uv - 0.5;
        float2 r = float2(d.x * sa - d.y * ca, d.x * ca + d.y * sa);
        float2 pc = float2(length(r) * 2.0, atan2(r.y, r.x) / (3.1416 * 2.0));
        u = pc - floor(pc);
    }
    float na = tex2D(g_tex, u * g_p0.xy + g_p0.zw).r;
    float nb = tex2D(g_tex, u * g_p1.xy + g_p1.zw).r;
    float n = smoothstep(g_p2.x, g_p2.x + g_p2.y, ((na + nb) * 0.5 + tex2D(g_e1, i.uv).b) * tex2D(g_e2, i.uv).r);
    float4 l = tex2D(g_e0, float2(n, n));
    float a = saturate(n * (g_p3.y != 0.0 ? (1.0 - i.uv.x) * g_p2.z : 1.0) * i.color.a);
    return float4(saturate(l.rgb * i.color.rgb) * a, a);
}

// 45: the serpent form's snakes: the texture pushed along a direction by
// its own panning green, coloured by the lut.
float4 ps_e45(VsOut i) : COLOR {
    float b = tex2D(g_tex, i.uv).b;
    float g = tex2D(g_tex, i.uv * g_p0.xy + g_p0.zw).g;
    float k = (g - 0.5) * 2.0 * g_p1.x * b;
    float4 t = tex2D(g_tex, i.uv + k * g_p1.yz);
    float4 l = tex2D(g_e0, t.rr);
    float a = saturate(t.a * i.color.a);
    return float4(saturate(l.rgb * i.color.rgb) * a, a);
}

// 46: the echo form's lines: a scaled copy's alpha less the texture's own.
float4 ps_e46(VsOut i) : COLOR {
    float s1 = tex2D(g_tex, (i.uv - g_p0.xy) / g_p0.z + g_p0.xy).a;
    float s0 = tex2D(g_tex, i.uv).a;
    float a = saturate(smoothstep(g_p0.w, g_p1.x, s1 - s0) * tex2D(g_e0, i.uv).r * i.color.a);
    return float4(saturate(i.color.rgb) * a, a);
}

// 47: vfx_round_smoke_shader: the frame read through its own panning blue,
// coloured by the lut at its red, eroded by its red at the particle's life.
float4 ps_e47(VsOut i) : COLOR {
    float db = tex2D(g_tex, i.uv + g_p0.xy).b;
    float4 t = tex2D(g_tex, i.uv + db * g_p0.z);
    float4 l = tex2D(g_e0, t.rr);
    float a = saturate(smoothstep(g_p0.w, g_p0.w + g_p1.x, t.r) * i.color.a);
    return float4(saturate(l.rgb * i.color.rgb) * a, a);
}

// 48: vfx_scream_squiggly_streaks: the streak pushed sideways by its own
// panning blue (offset by the particle's rotation), in the vertex colour.
float4 ps_e48(VsOut i) : COLOR {
    float m = (tex2D(g_tex, i.uv * g_p0.xy + g_p0.zw + g_p1.y).b - 0.5) * 2.0;
    float a = saturate(tex2D(g_tex, i.uv + float2(m * g_p1.x, 0.0)).a * i.color.a);
    return float4(saturate(i.color.rgb) * a, a);
}

// 49: vfx_card_remove_erosion: the card pushed sideways by its panning
// blue, eroded at the particle's life, its edge brightened by the lut.
float4 ps_e49(VsOut i) : COLOR {
    float d = tex2D(g_tex, i.uv + g_p0.x).b - 0.5;
    float4 t = tex2D(g_tex, i.uv + float2(d * g_p0.z, 0.0));
    float b = smoothstep(g_p0.w, g_p0.w + g_p1.y, t.a);
    float4 l = tex2D(g_e0, float2(b, b));
    float a = saturate(smoothstep(g_p0.w, g_p0.w + g_p1.x, t.a) * i.color.a);
    return float4(saturate(l.rgb * i.color.rgb) * a, a);
}

// 50: vfx_minion_divebomb_rings: the lut at the red, eroded by the alpha.
float4 ps_e50(VsOut i) : COLOR {
    float4 t = tex2D(g_tex, i.uv);
    float4 l = tex2D(g_e0, t.rr);
    float a = saturate(i.color.a * smoothstep(g_p0.x, g_p0.x + g_p0.y, t.a));
    return float4(saturate(l.rgb * i.color.rgb) * a, a);
}

// 51: vfx_hyperbeam_laser_line (fragment): the panning texture plus the
// overlay through the lut.
float4 ps_e51(VsOut i) : COLOR {
    float4 t = tex2D(g_e0, i.uv + float2(g_p0.x, 0.0));
    float o = tex2D(g_e1, i.uv).r;
    float4 l = tex2D(g_e2, float2(t.r + o, t.r + o));
    float a = saturate(t.a * i.color.a);
    return float4(saturate(l.rgb * i.color.rgb) * a, a);
}

// 52: vfx_hyperbeam_impact_spikes: the spikes sliding with the particle's
// life, masked, thresholded.
float4 ps_e52(VsOut i) : COLOR {
    float t = tex2D(g_tex, i.uv * g_p0.xy + float2(g_p0.z, g_p1.x + g_p0.w)).r;
    float a = saturate(smoothstep(0.35, 0.65, i.color.a * t * tex2D(g_e0, i.uv).r));
    return float4(saturate(i.color.rgb) * a, a);
}

// 53: vfx_grand_finale_spotlight (add / darken): grey by the angle about
// the source.
float4 ps_e53(VsOut i) : COLOR {
    float2 dl = i.uv - g_p0.xy;
    float an = atan2(dl.y, dl.x) / (2.0 * 3.14159265) + g_p0.z;
    an = frac(an);
    float d = g_p1.y != 0.0 ? saturate(abs(an - g_p1.x) / g_p1.y) : 1.0;
    d = lerp(d, 1.0 - d, g_p0.w);
    d = smoothstep(g_p1.z, g_p1.z + g_p1.w, d);
    float a = saturate(i.color.a);
    return float4(saturate(d * i.color.rgb) * a, a);
}

// 54: aeonglass_ray_shader: the ray's core and blur colours, edge masked.
float4 ps_e54(VsOut i) : COLOR {
    float4 r = tex2D(g_e0, i.uv + float2(g_p0.x, 0.0));
    float a = saturate(r.g * tex2D(g_e1, i.uv * float2(g_p1.w, g_p2.x)).g);
    return float4(saturate(g_p0.yzw * r.r + g_p1.xyz * r.g) * a, a);
}

// 55: soul_fysh_soundwave_shader: the scrolling wave through the edge mask.
float4 ps_e55(VsOut i) : COLOR {
    float x = tex2D(g_e0, i.uv).r * tex2D(g_e1, i.uv + float2(g_p0.x, 0.0)).r * g_p0.y;
    float a = saturate(smoothstep(g_p0.z, g_p0.w, x));
    return float4(saturate(g_p1.xyz) * a, a);
}

// 56: waterfall_giant_water_shader: the scrolling tint over the water.
float4 ps_e56(VsOut i) : COLOR {
    float3 mv = tex2D(g_e0, (i.uv + g_p0.xy) * g_p0.zw + g_p1.xy).rgb;
    float4 w = tex2D(g_e1, i.uv);
    float a = saturate(w.a);
    return float4(saturate(float3(g_p1.z, g_p1.w, g_p2.x) * mv + w.rgb) * a, a);
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

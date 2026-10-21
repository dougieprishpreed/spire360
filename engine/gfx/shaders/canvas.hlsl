// Canvas shader for the D3D9 renderer (PC D3D9 and Xbox 360). One shader
// for everything the canvas draws; textures hold straight alpha, the output
// is premultiplied (blend states assume it). The effect modes branch on
// their constants ([branch], tex2Dlod: the textures have one level), so a
// plain draw pays nothing for them. Mirrors the software renderer
// (engine/gfx/sw_renderer.cpp) and Godot's canvas shader for MSDF text.
//
// PC: compiled at startup with D3DCompile. Xbox 360: compiled by the XDK's
// fxc in the build (xdk/spire_view, /Fh headers).

float4 g_viewport : register(c0);     // 2/width, -2/height, -1 - offset.x, 1 + offset.y
float4 g_screen : register(c1);       // 1/width, 1/height

struct VsIn {
    float2 pos : POSITION;
    float2 uv : TEXCOORD0;
    float4 color : COLOR0;
};

struct VsOut {
    float4 pos : POSITION;
    float2 uv : TEXCOORD0;
    float4 color : COLOR0;
    float2 screen : TEXCOORD1;        // 0..1 over the render target
};

VsOut vs_main(VsIn i) {
    VsOut o;
    o.pos = float4(i.pos.x * g_viewport.x + g_viewport.z, i.pos.y * g_viewport.y + g_viewport.w, 0.0, 1.0);
    o.uv = i.uv;
    o.color = i.color;
    o.screen = i.pos * g_screen.xy;
    return o;
}

sampler2D g_tex : register(s0);
sampler2D g_group : register(s1);     // the canvas group layer (premultiplied)
sampler2D g_lut : register(s2);       // gradient map (256x1, straight alpha)
sampler2D g_fn1 : register(s3);       // stepped fire: noise 1, noise 2, noise mask,
sampler2D g_fn2 : register(s4);       // distortion 1, distortion 2, distortion mask
sampler2D g_fmask : register(s5);
sampler2D g_fd1 : register(s6);
sampler2D g_fd2 : register(s7);
sampler2D g_fdmask : register(s8);
float4 g_mode : register(c0);         // x: colour matrix on, y: MSDF on, z: group mask, w: ripple
float4 g_msdf : register(c1);         // x: px_range, y: outline, zw: texture size
float4 g_cm0 : register(c2);          // colour matrix rows: rgb' = M * rgb + offset * a
float4 g_cm1 : register(c3);
float4 g_cm2 : register(c4);
float4 g_ripple : register(c5);       // phase, modulo, ease, width (card_ripple.gdshader)
float4 g_gm : register(c6);           // x: gradient map on, y: alpha from the red
float4 g_fire : register(c7);         // x: stepped fire on
float4 g_fn1p : register(c8);         // each texture's scale.xy, offset.zw
float4 g_fn2p : register(c9);
float4 g_fmaskp : register(c10);
float4 g_fd1p : register(c11);
float4 g_fd2p : register(c12);
float4 g_fdmaskp : register(c13);
float4 g_fstr : register(c14);        // noise 1, noise 2 strength, invert mask, dark variant
float4 g_fdstr : register(c15);
float4 g_premul : register(c28);      // x: the texture is premultiplied (a capture)       // distortion 1, 2 strength
float4 g_fsteps : register(c16);      // inner step xy, outer step zw
float4 g_finner : register(c17);
float4 g_fouter : register(c18);
float4 g_vfx : register(c19);         // erosion particles: x on, y polar, z lut
float4 g_verosion : register(c20);    // threshold, width, on, from the alpha
float4 g_vdist : register(c21);       // distortion offset.xy, strength, on
sampler2D g_scrA : register(s9);      // the screen copy: the level at floor(lod)
sampler2D g_scrB : register(s10);     // and the next (level passes: the level above in s9)
sampler2D g_scrNoise : register(s11); // noise (repeats), mask (clamped); unset: white
sampler2D g_scrMask : register(s12);
float4 g_scr : register(c22);         // x: 1 dark_blur, 2 noise distortion, 3 level pass, 4 radial blur,
                                      // 5 glass border (z: corner weight, w: add, c23: corner colour),
                                      // 6 doom overlay (y: saturation, z: value, w: pulse, c23: gradient),
                                      // 7 distortion (y: polar, w: strength, c23: polar offset, tiling),
                                      // 8 chromatic aberration (w: strength, s11: distance curve);
                                      // (y: count, z: power, c23.xy: centre);
                                      // y: lod fraction, z: mix, w: strength
float4 g_scr2 : register(c23);        // xy: noise offset, zw: the level pass's texel size

float median3(float r, float g, float b) {
    return max(min(r, g), min(max(r, g), b));
}

float4 ps_main(VsOut i) : COLOR {
    [branch] if (g_scr.x > 0.5) {
        // Screen-reading shaders (ScreenParams); the vertex colour is not applied.
        [branch] if (g_scr.x > 8.5) {
            // screencap: the copy at the quad's own uv.
            return tex2Dlod(g_scrA, float4(i.uv, 0.0, 0.0));
        }
        [branch] if (g_scr.x > 7.5) {
            // Chromatic aberration: red and blue read along the quad's
            // radius by the distance curve and the texel's red.
            float2 dir = 0.5 - i.uv;
            float len = length(dir);
            float2 nd = len > 1e-6 ? dir / len : float2(0.0, 0.0);
            float k = g_scr.w * tex2Dlod(g_scrNoise, float4(len, len, 0.0, 0.0)).r * tex2Dlod(g_tex, float4(i.uv, 0.0, 0.0)).r;
            float r = tex2Dlod(g_scrA, float4(i.screen + nd * 0.5 * k, 0.0, 0.0)).r;
            float g = tex2Dlod(g_scrA, float4(i.screen, 0.0, 0.0)).g;
            float b = tex2Dlod(g_scrA, float4(i.screen + nd * k, 0.0, 0.0)).b;
            return float4(r, g, b, 1.0);
        }
        [branch] if (g_scr.x > 6.5) {
            // The screen distortions: pushed toward the middle of the quad by
            // the texel's red (polar: its alpha times its red at the polar uv),
            // times the vertex alpha.
            float4 t = tex2Dlod(g_tex, float4(i.uv, 0.0, 0.0));
            float m = t.r;
            [branch] if (g_scr.y > 0.5) {
                float2 d = i.uv - 0.5;
                float2 p = float2(length(d) * 2.0, atan2(d.y, d.x) / 6.2831853) * g_scr2.zw + g_scr2.xy;
                m = t.a * tex2Dlod(g_tex, float4(p, 0.0, 0.0)).r;
            }
            float2 dir = 0.5 - i.uv;
            float len = length(dir);
            float k = len > 1e-6 ? g_scr.w * i.color.a * m / len : 0.0;
            return tex2Dlod(g_scrA, float4(i.screen + dir * k, 0.0, 0.0));
        }
        [branch] if (g_scr.x > 5.5) {
            // doom_overlay: desaturated and darkened by the vertex alpha in
            // YIQ, a pulsing gradient rising from the bottom of the screen.
            float a = i.color.a;
            float sf = lerp(1.0, g_scr.y, a), vf = lerp(1.0, g_scr.z, a);
            float4 sc = tex2Dlod(g_scrA, float4(i.screen, 0.0, 0.0));
            float y = dot(float3(0.2989, 0.5870, 0.1140), sc.rgb) * vf;
            float ci = dot(float3(0.5959, -0.2774, -0.3216), sc.rgb) * sf * vf;
            float cq = dot(float3(0.2115, -0.5229, 0.3114), sc.rgb) * sf * vf;
            float g = pow(i.screen.y, 6.0) * g_scr.w * a;
            float3 rgb = float3(y + 0.956 * ci + 0.621 * cq, y - 0.272 * ci - 0.647 * cq, y - 1.106 * ci + 1.703 * cq);
            return saturate(float4(rgb, sc.a) + g * g_scr2);
        }
        [branch] if (g_scr.x > 4.5) {
            // The Ancient cards' glass border: the screen behind, brighter,
            // its alpha the mask's, toward the corner colour by the mask's green.
            float4 m = tex2Dlod(g_scrMask, float4(i.uv, 0.0, 0.0));
            float4 sc = lerp(tex2Dlod(g_scrA, float4(i.screen, 0.0, 0.0)), tex2Dlod(g_scrB, float4(i.screen, 0.0, 0.0)), g_scr.y);
            float3 rgb = (sc.a > 0.0 ? sc.rgb / sc.a : float3(0.0, 0.0, 0.0)) + g_scr.w;
            float4 f = lerp(float4(rgb, m.a), g_scr2, m.g * g_scr.z) * i.color;
            float a = saturate(f.a * tex2D(g_tex, i.uv).a * m.a);
            return float4(saturate(f.rgb) * a, a);
        }
        [branch] if (g_scr.x > 3.5) {
            // radial_blur: the mean of count reads toward the centre.
            float2 dir = i.screen - g_scr2.xy;
            float3 acc = 0;
            for (int r = 0; r < 16; ++r)
                if (r < g_scr.y) acc += tex2Dlod(g_scrA, float4(i.screen - g_scr.z * dir * r, 0.0, 0.0)).rgb;
            return float4(acc / g_scr.y, 1.0);
        }
        [branch] if (g_scr.x > 2.5) {
            // A level of the screen copy: the 7-tap Gaussian across and down
            // of the bilinear reads of the level above (Godot's back buffer
            // mipmaps).
            const float k[4] = { 0.214607, 0.189879, 0.131514, 0.071303 };
            float4 s = 0;
            for (int y = -3; y <= 3; ++y)
                for (int x = -3; x <= 3; ++x)
                    s += tex2Dlod(g_scrA, float4(i.uv + float2(x, y) * g_scr2.zw, 0.0, 0.0)) * (k[abs(x)] * k[abs(y)]);
            return s;
        }
        [branch] if (g_scr.x < 1.5) {
            // dark_blur: the copy at lod (two levels blended), mixed toward
            // opaque black on the straight colour.
            float4 c = lerp(tex2Dlod(g_scrA, float4(i.screen, 0.0, 0.0)), tex2Dlod(g_scrB, float4(i.screen, 0.0, 0.0)),
                            g_scr.y);
            float na = c.a * (1.0 - g_scr.z) + g_scr.z;
            float ka = c.a > 0.0 ? (1.0 - g_scr.z) * na / c.a : 0.0;
            return float4(c.rgb * ka, na);
        }
        // vfx_water_reflection_post / the_insatiable_sand_fall_2.
        float n = tex2Dlod(g_scrNoise, float4(i.uv + g_scr2.xy, 0.0, 0.0)).r;
        float d = n * g_scr.w - g_scr.w * 0.5;
        float4 sc = tex2Dlod(g_scrA, float4(i.screen + d, 0.0, 0.0));
        return sc * tex2Dlod(g_scrMask, float4(i.uv, 0.0, 0.0)).a;
    }
    [branch] if (g_fire.x > 0.5) {
        [branch] if (g_fstr.w > 1.5) {
            // Doom bar: the scene's noise texture, sampled in screen space.
            float tm = g_fstr.x;
            float scroll = tex2Dlod(g_fn1, float4(i.screen * 0.1 + tm * 0.0005 + 0.24, 0.0, 0.0)).r;
            float n1 = tex2Dlod(g_fn1, float4(i.screen + tm * 0.0005 + scroll, 0.0, 0.0)).r;
            float n2 = tex2Dlod(g_fn1, float4(i.screen - tm * 0.0007 + scroll, 0.0, 0.0)).r;
            float value = smoothstep(0.4, 0.6, n1 * 0.75 + n2 * 0.35);
            float3 rgb = tex2Dlod(g_lut, float4(value, 0.0, 0.0, 0.0)).rgb;
            float alpha = tex2D(g_tex, i.uv).a * i.color.a;
            return float4(rgb * alpha, alpha);
        }
        // The game's stepped fire shaders (shaders/vfx/vfx_stepped_shader_fire_*).
        float2 uv = i.uv;
        float n2 = g_fstr.y * tex2Dlod(g_fn2, float4(uv * g_fn2p.xy + g_fn2p.zw, 0.0, 0.0)).b;
        float n1 = g_fstr.x * tex2Dlod(g_fn1, float4(uv * g_fn1p.xy + g_fn1p.zw, 0.0, 0.0)).b;
        float m = tex2Dlod(g_fmask, float4((uv - float2(0.0, 0.04)) * g_fmaskp.xy + float2(0.0, 0.04) + g_fmaskp.zw, 0.0, 0.0)).b;
        m = lerp(m, 1.0 - m, g_fstr.z);
        float a = n1 * n2 + m;
        float2 d = tex2Dlod(g_fd2, float4(uv * g_fd2p.xy + g_fd2p.zw, 0.0, 0.0)).xy * g_fdstr.y +
                   tex2Dlod(g_fd1, float4(uv * g_fd1p.xy + g_fd1p.zw, 0.0, 0.0)).xy * g_fdstr.x;
        float2 dm = tex2Dlod(g_fdmask, float4(uv * g_fdmaskp.xy + g_fdmaskp.zw, 0.0, 0.0)).xy;
        float v = a * tex2Dlod(g_tex, float4(uv + d * dm, 0.0, 0.0)).b;
        float3 rgb;
        if (g_fstr.w > 0.5)
            rgb = g_fouter.rgb * (smoothstep(g_fsteps.x, g_fsteps.y, 1.0 - v) + g_finner.rgb);
        else
            rgb = g_fouter.rgb + smoothstep(g_fsteps.x, g_fsteps.y, v) * g_finner.rgb;
        float fa = saturate(smoothstep(g_fsteps.z, g_fsteps.w, v)) * i.color.a;
        return float4(rgb * fa, fa);
    }
    [branch] if (g_vfx.x > 0.5) {
        // The game's erosion particle shaders (vfx_ring_polar, vfx_poof, ...).
        float2 uv = i.uv;
        if (g_vdist.w > 0.5) uv += tex2Dlod(g_tex, float4(uv + g_vdist.xy, 0.0, 0.0)).b * g_vdist.z;
        if (g_vfx.y > 0.5) {
            float2 dir = uv - float2(0.5, 0.5);
            uv = frac(float2(length(dir) * 2.0, atan2(dir.y, dir.x) / 6.2831853));
        }
        float4 e = tex2Dlod(g_tex, float4(uv, 0.0, 0.0));
        float3 rgb = g_vfx.z > 0.5 ? tex2Dlod(g_lut, float4(e.r, 0.5, 0.0, 0.0)).rgb : float3(1.0, 1.0, 1.0);
        float ea = e.a;
        if (g_verosion.z > 0.5) {
            float ch = g_verosion.w > 0.5 ? e.a : e.r;
            float k = saturate((ch - g_verosion.x) / max(g_verosion.y, 0.0001));
            ea = k * k * (3.0 - 2.0 * k);
        }
        float4 vc = float4(rgb, ea) * i.color;
        return float4(vc.rgb * vc.a, vc.a);
    }
    float4 t = tex2D(g_tex, i.uv);
    if (g_premul.x > 0.5) t.rgb = t.a > 0.0 ? t.rgb / t.a : float3(0.0, 0.0, 0.0);
    [branch] if (g_gm.x > 0.5) {
        // The game's grayscale / flipbook VFX shaders: the red through the
        // gradient, the alpha from the red or the texel.
        float4 l = tex2Dlod(g_lut, float4(t.r, 0.5, 0.0, 0.0));
        t = float4(l.rgb, g_gm.y > 0.5 ? t.r : t.a);
    }
    if (g_mode.z > 0.5) {
        // Godot's clip_children: the group layer through the mask's alpha.
        return tex2D(g_group, i.screen) * (t.a * i.color.a);
    }
    if (g_mode.y > 0.5) {
        float2 dest = 1.0 / fwidth(i.uv);
        float px_size = max(0.5 * dot(g_msdf.xx / g_msdf.zw, dest), 1.0);
        float d = median3(t.r, t.g, t.b);
        float a;
        if (g_msdf.y > 0.0) {
            float cr = clamp(g_msdf.y, 0.0, g_msdf.x / 2.0 - 1.0) / g_msdf.x;
            d = min(d, t.a);
            a = saturate((d - 0.5 + cr) * px_size);
        } else {
            a = saturate((d - 0.5) * px_size + 0.5);
        }
        t = float4(1.0, 1.0, 1.0, a);
    }
    if (g_mode.x > 0.5) {
        float3 rgb = t.rgb;
        t.rgb = float3(dot(g_cm0.xyz, rgb) + g_cm0.w, dot(g_cm1.xyz, rgb) + g_cm1.w, dot(g_cm2.xyz, rgb) + g_cm2.w);
    }
    float4 c = t * i.color;
    if (g_mode.w > 0.5) {
        // Godot's card_ripple.gdshader: the alpha as a distance field.
        float sdf = fmod(g_ripple.x + c.a, g_ripple.y);
        float b = c.a - sdf + smoothstep(0.0, g_ripple.z, sdf) * g_ripple.y;
        c.a = smoothstep(1.0 - g_ripple.w, 1.0, b);
    }
    return float4(c.rgb * c.a, c.a);
}

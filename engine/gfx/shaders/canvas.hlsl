// Canvas shader for the D3D9 renderer (PC D3D9 and Xbox 360). One shader
// for everything the canvas draws; textures hold straight alpha, the output
// is premultiplied (blend states assume it). Mirrors the software renderer
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
float4 g_fdstr : register(c15);       // distortion 1, 2 strength
float4 g_fsteps : register(c16);      // inner step xy, outer step zw
float4 g_finner : register(c17);
float4 g_fouter : register(c18);

float median3(float r, float g, float b) {
    return max(min(r, g), min(max(r, g), b));
}

float4 ps_main(VsOut i) : COLOR {
    if (g_fire.x > 0.5) {
        // The game's stepped fire shaders (shaders/vfx/vfx_stepped_shader_fire_*).
        float2 uv = i.uv;
        float n2 = g_fstr.y * tex2D(g_fn2, uv * g_fn2p.xy + g_fn2p.zw).b;
        float n1 = g_fstr.x * tex2D(g_fn1, uv * g_fn1p.xy + g_fn1p.zw).b;
        float m = tex2D(g_fmask, (uv - float2(0.0, 0.04)) * g_fmaskp.xy + float2(0.0, 0.04) + g_fmaskp.zw).b;
        m = lerp(m, 1.0 - m, g_fstr.z);
        float a = n1 * n2 + m;
        float2 d = tex2D(g_fd2, uv * g_fd2p.xy + g_fd2p.zw).xy * g_fdstr.y +
                   tex2D(g_fd1, uv * g_fd1p.xy + g_fd1p.zw).xy * g_fdstr.x;
        float2 dm = tex2D(g_fdmask, uv * g_fdmaskp.xy + g_fdmaskp.zw).xy;
        float v = a * tex2D(g_tex, uv + d * dm).b;
        float3 rgb;
        if (g_fstr.w > 0.5)
            rgb = g_fouter.rgb * (smoothstep(g_fsteps.x, g_fsteps.y, 1.0 - v) + g_finner.rgb);
        else
            rgb = g_fouter.rgb + smoothstep(g_fsteps.x, g_fsteps.y, v) * g_finner.rgb;
        float fa = saturate(smoothstep(g_fsteps.z, g_fsteps.w, v)) * i.color.a;
        return float4(rgb * fa, fa);
    }
    float4 t = tex2D(g_tex, i.uv);
    if (g_gm.x > 0.5) {
        // The game's grayscale / flipbook VFX shaders: the red through the
        // gradient, the alpha from the red or the texel.
        float4 l = tex2D(g_lut, float2(t.r, 0.5));
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

// Canvas shader for the D3D9 renderer (PC D3D9 and Xbox 360). One shader
// for everything the canvas draws; textures hold straight alpha, the output
// is premultiplied (blend states assume it). Mirrors the software renderer
// (engine/gfx/sw_renderer.cpp) and Godot's canvas shader for MSDF text.
//
// PC: compiled at startup with D3DCompile. Xbox 360: compiled by the XDK's
// fxc in the build (xdk/spire_view, /Fh headers).

float4 g_viewport : register(c0);     // 2/width, -2/height, -1 - offset.x, 1 + offset.y

struct VsIn {
    float2 pos : POSITION;
    float2 uv : TEXCOORD0;
    float4 color : COLOR0;
};

struct VsOut {
    float4 pos : POSITION;
    float2 uv : TEXCOORD0;
    float4 color : COLOR0;
};

VsOut vs_main(VsIn i) {
    VsOut o;
    o.pos = float4(i.pos.x * g_viewport.x + g_viewport.z, i.pos.y * g_viewport.y + g_viewport.w, 0.0, 1.0);
    o.uv = i.uv;
    o.color = i.color;
    return o;
}

sampler2D g_tex : register(s0);
float4 g_mode : register(c0);         // x: colour matrix on, y: MSDF on
float4 g_msdf : register(c1);         // x: px_range, y: outline, zw: texture size
float4 g_cm0 : register(c2);          // colour matrix rows: rgb' = M * rgb + offset * a
float4 g_cm1 : register(c3);
float4 g_cm2 : register(c4);

float median3(float r, float g, float b) {
    return max(min(r, g), min(max(r, g), b));
}

float4 ps_main(VsOut i) : COLOR {
    float4 t = tex2D(g_tex, i.uv);
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
    return float4(c.rgb * c.a, c.a);
}

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

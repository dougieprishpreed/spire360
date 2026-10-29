// What the Vulkan renderer's fragment shaders share (engine/gfx/vk_renderer.cpp).
// tools/xsdk/vk_shaders.py puts it where a shader includes it, with the
// samplers the shader names after it. The draw's parameters are the D3D9
// renderer's pixel constants (shaders/canvas.hlsl, effects.hlsl), c0 to
// c45, in one std140 block, the radial blur's count after them; the HLSL
// names below let effects.hlsl's shaders compile as written.
//
// xost, xsdk's shader compiler, differs from Microsoft's (xsdk's
// docs/PITFALLS.md): sin and cos are right only in -pi..pi, a second
// length() in a shader can come out wrong, and there is no atan2 (atan is
// wrong past -1..1). The helpers below stand in for them; a helper must
// hold no if and return once.

layout(location = 0) in vec4 v_color;
layout(location = 1) in vec2 v_uv;
layout(location = 2) in vec2 v_screen; // 0..1 over the target

layout(location = 0) out vec4 o_color;

layout(std140, set = 1, binding = 0) uniform Params {
    vec4 c[46];
    int count;
} P;

float spire_sin(float x) { return sin(x - 6.2831853 * floor((x + 3.14159265) / 6.2831853)); }
float spire_cos(float x) { return cos(x - 6.2831853 * floor((x + 3.14159265) / 6.2831853)); }

// atan2(y, x) in -pi..pi: a 7th-order fit of atan on 0..1 (within 1e-5),
// moved to its octant; 0 at the origin, as HLSL's.
float spire_atan2(float y, float x) {
    float ax = abs(x), ay = abs(y);
    float a = min(ax, ay) / max(max(ax, ay), 1e-30);
    float s = a * a;
    float r = ((-0.0464964749 * s + 0.15931422) * s - 0.327622764) * s * a + a;
    r = mix(r, 1.57079637 - r, step(ax, ay) * step(1e-30, ay));
    r = mix(r, 3.14159274 - r, 1.0 - step(0.0, x));
    return mix(r, -r, 1.0 - step(0.0, y));
}

// cond ? a : b, both evaluated and one chosen (OpSelect): vk_shaders.py
// writes every ?: so, as glslang makes a branch of most, and xost gives up
// (its recursion's limit) on a branch after a long chain such as atan2's.
float spire_sel(bool c, float a, float b) { return mix(b, a, c); }
vec2 spire_sel(bool c, vec2 a, vec2 b) { return mix(b, a, bvec2(c)); }
vec3 spire_sel(bool c, vec3 a, vec3 b) { return mix(b, a, bvec3(c)); }
vec4 spire_sel(bool c, vec4 a, vec4 b) { return mix(b, a, bvec4(c)); }
int spire_sel(bool c, int a, int b) { return mix(b, a, c); }

#define sin spire_sin
#define cos spire_cos
#define atan2 spire_atan2
#define length(v) sqrt(dot((v), (v)))

#define float2 vec2
#define float3 vec3
#define float4 vec4
#define lerp mix
#define frac fract
#define saturate(x) clamp((x), 0.0, 1.0)
#define tex2D texture
#define tex2Dlod(s, c) textureLod(s, (c).xy, (c).w)
#define fmod(x, y) ((x) - (y) * trunc((x) / (y)))

#define g_mode P.c[0]
#define g_msdf P.c[1]
#define g_cm0 P.c[2]
#define g_cm1 P.c[3]
#define g_cm2 P.c[4]
#define g_ripple P.c[5]
#define g_gm P.c[6]
#define g_fire P.c[7]
#define g_fn1p P.c[8]
#define g_fn2p P.c[9]
#define g_fmaskp P.c[10]
#define g_fd1p P.c[11]
#define g_fd2p P.c[12]
#define g_fdmaskp P.c[13]
#define g_fstr P.c[14]
#define g_fdstr P.c[15]
#define g_fsteps P.c[16]
#define g_finner P.c[17]
#define g_fouter P.c[18]
#define g_vfx P.c[19]
#define g_verosion P.c[20]
#define g_vdist P.c[21]
#define g_scr P.c[22]
#define g_scr2 P.c[23]
#define g_p0 P.c[24]
#define g_p1 P.c[25]
#define g_p2 P.c[26]
#define g_p3 P.c[27]
#define g_premul P.c[28]
// g_x[k] (effects.hlsl's c29 on) is P.c[29 + k]: vk_shaders.py writes it so.

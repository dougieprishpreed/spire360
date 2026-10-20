#include "shaders.h"

#include "canvas.h"
#include "controls.h"
#include "../gfx/effects.h"
#include "node.h"
#include "particles.h"
#include "scene.h"
#include "value.h"

#include <math.h>
#include <map>
#include <string.h>

namespace spire {

double g_shader_time = 0.0;

static const CanvasItem* g_item = NULL; /* the item being drawn */

static float param(const MaterialRes* m, const char* name, float fallback) {
    if (g_item) {
        std::map<std::string, float>::const_iterator it = g_item->shader_params.find(name);
        if (it != g_item->shader_params.end()) return it->second;
    }
    if (!m->props) return fallback;
    std::string k = std::string("shader_parameter/") + name;
    return m->props->has(k.c_str()) ? jv_float((*m->props)[k.c_str()], fallback) : fallback;
}

/* A colour parameter: the item's "<name>.r/g/b" overrides, else the
 * material's. */
static Color param_color(const MaterialRes* m, const char* name, const Color& fallback) {
    Color c = fallback;
    if (m->props) {
        std::string k = std::string("shader_parameter/") + name;
        if (m->props->has(k.c_str())) jv_color((*m->props)[k.c_str()], c);
    }
    std::string n = name;
    c.r = param(m, (n + ".r").c_str(), c.r);
    c.g = param(m, (n + ".g").c_str(), c.g);
    c.b = param(m, (n + ".b").c_str(), c.b);
    return c;
}

/* A sampler2D parameter's resource (NULL: unset). */
Resource* shader_param_resource(const MaterialRes* m, const char* name) {
    if (!m || !m->props) return NULL;
    std::string k = std::string("shader_parameter/") + name;
    if (!m->props->has(k.c_str())) return NULL;
    SceneLibrary* lib = SceneLibrary::current();
    return lib ? lib->resolve((*m->props)[k.c_str()], m->file) : NULL;
}

float shader_param(const MaterialRes* m, const char* name, float fallback) { return param(m, name, fallback); }

Vec2 shader_param_vec2(const MaterialRes* m, const char* name, const Vec2& fallback) {
    if (!m || !m->props) return fallback;
    std::string k = std::string("shader_parameter/") + name;
    if (!m->props->has(k.c_str())) return fallback;
    const Json& v = (*m->props)[k.c_str()]["v"];
    return v.size() >= 2 ? Vec2((float)v[(size_t)0].num(), (float)v[(size_t)1].num()) : fallback;
}

/* A gradient (GradientTexture1D) as a 256x1 lut for the renderer's
 * gradient map, made once per gradient (and renderer). Unset: white
 * (Godot's default for a sampler2D). */
static Texture* gradient_lut(Canvas& c, Resource* gradient) {
    std::string key = "gradient:" + (gradient ? gradient->path : std::string("white"));
    if (gradient && gradient->path.empty()) key += format("%p", (void*)gradient);
    Texture* cached = c.cached_texture(key);
    if (cached) return cached;
    GradientData g;
    bool ok = gradient && GradientData::from(gradient, g) && g.valid();
    TextureData td;
    td.format = TEX_ARGB8;
    td.flags = TEXF_DATA; /* straight alpha */
    td.width = td.orig_width = 256;
    td.height = td.orig_height = 1;
    td.levels.resize(1);
    td.levels[0].resize(256 * 4);
    for (int i = 0; i < 256; ++i) {
        Color col = ok ? g.at((float)i / 255.0f) : Color(1, 1, 1, 1);
        u8* px = &td.levels[0][(size_t)i * 4]; /* B G R A */
        px[0] = (u8)(clampf(col.b, 0, 1) * 255 + 0.5f);
        px[1] = (u8)(clampf(col.g, 0, 1) * 255 + 0.5f);
        px[2] = (u8)(clampf(col.r, 0, 1) * 255 + 0.5f);
        px[3] = (u8)(clampf(col.a, 0, 1) * 255 + 0.5f);
    }
    return c.cache_texture(key, td);
}

/* A sampler2D parameter as a renderer texture (NULL: unset, white). */
static Texture* param_texture(Canvas& c, const MaterialRes* m, const char* name) {
    Resource* r = shader_param_resource(m, name);
    if (!r || !r->is_texture()) return NULL;
    ImageTextureRes* img = NULL;
    if (r->kind == Resource::RES_IMAGE_TEXTURE) {
        img = (ImageTextureRes*)r;
    } else {
        Texture2DRes* t = (Texture2DRes*)r;
        Rect2 dst, src;
        t->get_rect_region(Rect2(Vec2(), t->size()), Rect2(Vec2(), t->size()), dst, src, &img);
    }
    return img ? c.gpu_texture(img) : NULL;
}

/* A vec4 parameter (fallback when unset). */
static void param_vec4(const MaterialRes* m, const char* name, const float fallback[4], float out[4]) {
    for (int i = 0; i < 4; ++i) out[i] = fallback[i];
    if (!m || !m->props) return;
    std::string k = std::string("shader_parameter/") + name;
    if (!m->props->has(k.c_str())) return;
    const Json& v = (*m->props)[k.c_str()]["v"];
    for (size_t i = 0; i < 4 && i < v.size(); ++i) out[i] = (float)v[i].num();
}

/* A GradientTexture2D as Godot fills it (GradientTexture2D::
 * _get_gradient_offset_at), once per resource. */
static Texture* gradient_2d(Canvas& c, Resource* r) {
    std::string key = "gradient2d:" + r->path + format("%p", (void*)r);
    Texture* cached = c.cached_texture(key);
    if (cached) return cached;
    GradientData g;
    bool ok = GradientData::from(r, g) && g.valid();
    const Json& p = *r->props;
    int w = p.has("width") ? p["width"].i(64) : 64, h = p.has("height") ? p["height"].i(64) : 64;
    if (w < 1) w = 1;
    if (h < 1) h = 1;
    if (w > 1024) w = 1024;
    if (h > 1024) h = 1024;
    int fill = p["fill"].i(0), repeat = p["repeat"].i(0);
    Vec2 from(0, 0), to(1, 0);
    jv_vec2(p["fill_from"], from);
    jv_vec2(p["fill_to"], to);
    TextureData td;
    td.format = TEX_ARGB8;
    td.flags = TEXF_DATA; /* straight alpha */
    td.width = td.orig_width = w;
    td.height = td.orig_height = h;
    td.levels.resize(1);
    td.levels[0].resize((size_t)w * h * 4);
    Vec2 d = to - from;
    float len = sqrtf(d.x * d.x + d.y * d.y);
    for (int y = 0; y < h; ++y)
        for (int x = 0; x < w; ++x) {
            Vec2 pos(w > 1 ? (float)x / (w - 1) : 0.0f, h > 1 ? (float)y / (h - 1) : 0.0f);
            float ofs = 0;
            if (len > 0) {
                Vec2 q = pos - from;
                if (fill == 1) {
                    ofs = sqrtf(q.x * q.x + q.y * q.y) / len;
                } else if (fill == 2) {
                    float mx = maxf(fabsf(d.x), fabsf(d.y));
                    ofs = mx > 0 ? maxf(fabsf(q.x), fabsf(q.y)) / mx : 0.0f;
                } else {
                    ofs = (q.x * d.x + q.y * d.y) / (len * len);
                }
            }
            if (repeat == 1) {
                ofs = fmodf(ofs, 1.0f);
                if (ofs < 0) ofs += 1;
            } else if (repeat == 2) {
                ofs = fmodf(fabsf(ofs), 2.0f);
                if (ofs > 1) ofs = 2 - ofs;
            } else {
                ofs = clampf(ofs, 0, 1);
            }
            Color col = ok ? g.at(ofs) : Color(ofs, ofs, ofs, 1);
            u8* px = &td.levels[0][((size_t)y * w + x) * 4]; /* B G R A */
            px[0] = (u8)(clampf(col.b, 0, 1) * 255 + 0.5f);
            px[1] = (u8)(clampf(col.g, 0, 1) * 255 + 0.5f);
            px[2] = (u8)(clampf(col.r, 0, 1) * 255 + 0.5f);
            px[3] = (u8)(clampf(col.a, 0, 1) * 255 + 0.5f);
        }
    return c.cache_texture(key, td);
}

/* A CurveTexture (texture_mode RGB: the value in every channel, RED: in
 * red), 256 wide, values clamped to 0..1, once per resource. */
static Texture* curve_texture(Canvas& c, Resource* r) {
    std::string key = "curvetex:" + r->path + format("%p", (void*)r);
    Texture* cached = c.cached_texture(key);
    if (cached) return cached;
    CurveData curve;
    CurveData::from(r, curve);
    bool red = r->props && (*r->props)["texture_mode"].i(0) == 1;
    TextureData td;
    td.format = TEX_ARGB8;
    td.flags = TEXF_DATA;
    td.width = td.orig_width = 256;
    td.height = td.orig_height = 1;
    td.levels.resize(1);
    td.levels[0].resize(256 * 4);
    for (int i = 0; i < 256; ++i) {
        float v = curve.valid() ? clampf(curve.sample((float)i / 255.0f), 0, 1) : 0.0f;
        u8 b = (u8)(v * 255 + 0.5f);
        u8* px = &td.levels[0][(size_t)i * 4];
        px[0] = red ? 0 : b;
        px[1] = red ? 0 : b;
        px[2] = b;
        px[3] = 255;
    }
    return c.cache_texture(key, td);
}

/* A sampler2D parameter for an effect shader: an image, a gradient (1D:
 * its lut; 2D: its fill), a curve's values. NULL: unset (white). */
static Texture* effect_texture(Canvas& c, const MaterialRes* m, const char* name) {
    Resource* r = shader_param_resource(m, name);
    if (!r) return NULL;
    if (r->is_texture()) return param_texture(c, m, name);
    if (r->type == "GradientTexture1D") return gradient_lut(c, r);
    if (r->type == "GradientTexture2D" && r->props) return gradient_2d(c, r);
    if (r->type == "CurveTexture") return curve_texture(c, r);
    return NULL;
}

/* The effect applied now and how it changes per particle. */
static EffectParams g_effect;
static int g_effect_particle = 0; /* the effect when it changes per particle */
static int g_effect_life_param;   /* the parameter its curve at the life sets */
static CurveData g_effect_curve;
static Vec2 g_effect_offset_k;

bool shader_effect_particle(float life, float custom_z, EffectParams* out) {
    if (!g_effect_particle) return false;
    *out = g_effect;
    /* the erosion curve (hint_default_black) at the life */
    out->p[g_effect_life_param] = g_effect_curve.valid() ? g_effect_curve.sample(life) : 0.0f;
    if (g_effect_particle == EFFECT_PANNING) {
        /* the offset: CUSTOM.z * initial_offset_multiplier / main_st.xy */
        out->p[2] += custom_z * g_effect_offset_k.x;
        out->p[3] += custom_z * g_effect_offset_k.y;
    }
    return true;
}

static bool set_effect(Canvas& c, const EffectParams& e) {
    g_effect = e;
    return c.renderer()->set_effect(&e);
}

/* The time panning of a uv (kept within 0..1: the textures repeat). */
static float pan(float speed) { return fmodf(speed * (float)g_shader_time, 1.0f); }

/* The effect shaders (engine/gfx/effects.h); false: not one of them. */
static bool effect_shader(Canvas& c, const MaterialRes* m, bool* ok) {
    const std::string& s = m->shader;
    EffectParams e;
    memset(&e, 0, sizeof(e));
    g_effect_particle = 0;
    if (s == "shaders/scrolling_smoke.tres") {
        e.id = EFFECT_SCROLLING_SMOKE;
        e.p[0] = fmodf(param(m, "ScrollSpeed", 0.2f) * (float)g_shader_time + param(m, "timeOffset", 0.0f), 1.0f);
        Vec2 st = shader_param_vec2(m, "AlphaStep", Vec2(0, 1));
        e.p[1] = st.x;
        e.p[2] = st.y;
        e.p[3] = param(m, "Ovarall_Alpha", 1.0f);
        Color k = param_color(m, "Color", Color(1, 1, 1, 1));
        e.p[4] = k.r;
        e.p[5] = k.g;
        e.p[6] = k.b;
        e.tex[0] = effect_texture(c, m, "smokeTex");
        e.tex[1] = effect_texture(c, m, "alphaGradient");
        e.repeat = 3;
    } else if (s == "shaders/vfx/common/vfx_noise_scale_shader.gdshader") {
        /* The vertex() scaling by the noise is not ported. */
        e.id = EFFECT_LUT_COLOR;
        e.p[0] = param(m, "use_r_as_alpha", 0.0f);
        e.tex[0] = effect_texture(c, m, "lut");
    } else if (s == "images/vfx/simple_noise_flat_shader.tres" || s == "images/vfx/simple_noise_add_shader.tres") {
        bool two = s == "images/vfx/simple_noise_flat_shader.tres";
        e.id = EFFECT_SIMPLE_NOISE;
        Vec2 st = shader_param_vec2(m, "Step", Vec2(0, 0));
        Vec2 s1 = shader_param_vec2(m, "mask_scale", Vec2(1, 1)), p1 = shader_param_vec2(m, "mask_panning", Vec2(2, 3));
        e.p[0] = st.x;
        e.p[1] = st.y;
        e.p[2] = s1.x;
        e.p[3] = s1.y;
        e.p[4] = pan(p1.x);
        e.p[5] = pan(p1.y);
        e.tex[0] = effect_texture(c, m, "noise_tex");
        e.p[6] = e.p[7] = 1;
        if (two) {
            Vec2 s2 = shader_param_vec2(m, "mask2_scale", Vec2(1, 1)), p2 = shader_param_vec2(m, "mask2_panning", Vec2(0.2f, 0.3f));
            e.p[6] = s2.x;
            e.p[7] = s2.y;
            e.p[8] = pan(p2.x);
            e.p[9] = pan(p2.y);
            e.tex[1] = effect_texture(c, m, "noise_tex2");
        }
        e.repeat = 3;
    } else if (s == "shaders/vfx/monsters/decimillipede_suck_shader.tres") {
        float dir = param(m, "direction", 1.0f);
        e.id = EFFECT_DECIMILLIPEDE_SUCK;
        e.p[0] = pan(-0.5f);
        e.p[1] = pan(4.0f * dir);
        e.p[2] = pan(1.0f);
        e.p[3] = pan(3.0f * dir);
        e.p[4] = param(m, "opacity", 1.0f);
        e.tex[0] = effect_texture(c, m, "suckPattern");
        e.tex[1] = effect_texture(c, m, "suckGradient");
        e.tex[2] = effect_texture(c, m, "suckShape");
        e.repeat = 1;
    } else if (s == "images/vfx/vfx_molten_fist/vfx_molten_fist.tres") {
        e.id = EFFECT_MOLTEN_FIST;
        Vec2 st = shader_param_vec2(m, "NoiseStep", Vec2(0.1f, 0.2f));
        e.p[0] = st.x;
        e.p[1] = st.y;
        e.p[2] = param(m, "Noise2Strength", 1.0f);
        e.p[3] = fabsf(param(m, "Noise1Strength", 1.0f));
        if (param(m, "InvertNoiseMask", 1.0f) != 0.0f) e.p[3] = -maxf(e.p[3], 1e-6f);
        Vec2 s1 = shader_param_vec2(m, "Noise1Scaling", Vec2(1, 1)), p1 = shader_param_vec2(m, "Noise1Panning", Vec2(1, 1));
        Vec2 s2 = shader_param_vec2(m, "Noise2Scaling", Vec2(1, 1)), p2 = shader_param_vec2(m, "Noise2Panning", Vec2(1, 1));
        Vec2 ms = shader_param_vec2(m, "NoiseMaskScale", Vec2(1, 1)), mo = shader_param_vec2(m, "NoiseMaskOffset", Vec2(0, 0));
        e.p[4] = s1.x;
        e.p[5] = s1.y;
        e.p[6] = pan(p1.x);
        e.p[7] = pan(p1.y);
        e.p[8] = s2.x;
        e.p[9] = s2.y;
        e.p[10] = pan(p2.x);
        e.p[11] = pan(p2.y);
        e.p[12] = ms.x;
        e.p[13] = ms.y;
        e.p[14] = mo.x;
        e.p[15] = mo.y;
        e.tex[0] = effect_texture(c, m, "Noise1Texture");
        e.tex[1] = effect_texture(c, m, "Noise2Texture");
        e.tex[2] = effect_texture(c, m, "NoiseMask");
        e.tex[3] = effect_texture(c, m, "tex_frg_40");
        e.repeat = 3;
    } else if (s == "images/vfx/slash_shader_flat.tres") {
        e.id = EFFECT_SLASH_FLAT;
        Vec2 st = shader_param_vec2(m, "step", Vec2(0, 0));
        float ms = param(m, "master_step", 1.0f);
        e.p[0] = st.x * ms;
        e.p[1] = st.y * ms;
        e.p[2] = param(m, "noise_1_strength", 1.0f);
        e.p[3] = param(m, "noise_2_strength", 1.0f);
        e.p[4] = param(m, "opacity", 1.0f);
        Vec2 s1 = shader_param_vec2(m, "noise_1_scale", Vec2(1, 1)), p1 = shader_param_vec2(m, "noise_1_panning", Vec2(2, 3));
        Vec2 s2 = shader_param_vec2(m, "noise_2_scale", Vec2(1, 1)), p2 = shader_param_vec2(m, "noise_2_panning", Vec2(0.2f, 0.3f));
        e.p[5] = s1.x;
        e.p[6] = s1.y;
        e.p[7] = pan(p1.x);
        e.p[8] = pan(p1.y);
        e.p[9] = s2.x;
        e.p[10] = s2.y;
        e.p[15] = pan(p2.x);
        e.p[11] = pan(p2.y);
        Color k = param_color(m, "ColorParameter", Color(0, 0, 0, 1));
        e.p[12] = k.r;
        e.p[13] = k.g;
        e.p[14] = k.b;
        e.tex[0] = effect_texture(c, m, "shape_mask_tex");
        e.tex[1] = effect_texture(c, m, "noise_1_tex");
        e.tex[2] = effect_texture(c, m, "noise_2_tex");
        e.repeat = 6;
    } else if (s == "shaders/texture_transition.gdshader" || s == "inline:14a82729491d6b89") {
        /* inline:14a8...: the character transitions' copy, without
         * additionalFade and alpha. */
        bool full = s == "shaders/texture_transition.gdshader";
        e.id = EFFECT_TEXTURE_TRANSITION;
        e.p[0] = param(m, "threshold", 0.0f);
        e.p[1] = full ? param(m, "additionalFade", 0.0f) : 0.0f;
        e.p[2] = full ? param(m, "alpha", 0.0f) : 1.0f;
        e.tex[0] = effect_texture(c, m, "transitionTex");
    } else if (s == "shaders/vfx/common/vfx_panning_shader.gdshader") {
        e.id = EFFECT_PANNING;
        static const float st0[4] = { 1, 1, 0, 0 };
        float st[4];
        param_vec4(m, "main_st", st0, st);
        e.p[0] = st[0];
        e.p[1] = st[1];
        e.p[2] = pan(st[2]);
        e.p[3] = pan(st[3]);
        CurveData::from(shader_param_resource(m, "erosion_over_lifetime"), g_effect_curve);
        e.p[4] = g_effect_curve.valid() ? g_effect_curve.sample(0) : 0.0f;
        e.p[5] = param(m, "erosion_offset", 1.0f);
        e.p[6] = param(m, "energy", 1.0f);
        e.p[7] = param(m, "use_red_channel_as_alpha", 0.0f);
        Vec2 k = shader_param_vec2(m, "initial_offset_multiplier", Vec2(0, 0));
        g_effect_offset_k = Vec2(st[0] != 0 ? k.x / st[0] : 0.0f, st[1] != 0 ? k.y / st[1] : 0.0f);
        e.tex[0] = effect_texture(c, m, "lut");
        e.tex[1] = effect_texture(c, m, "mask");
        g_effect_particle = EFFECT_PANNING;
        g_effect_life_param = 4;
    } else if (s == "shaders/vfx/common/vfx_ray_shader.gdshader") {
        /* VERTEX.x *= CUSTOM.z: Particles2D::draw. */
        e.id = EFFECT_RAY;
        e.tex[0] = effect_texture(c, m, "lut");
        e.tex[1] = effect_texture(c, m, "vertical_alpha");
    } else if (s == "shaders/vfx/ui/card/exhaust/vfx_card_exhaust_dissolve_shader.gdshader") {
        e.id = EFFECT_EXHAUST_DISSOLVE;
        e.p[0] = param(m, "erosion_base", 0.0f);
        e.p[1] = param(m, "erosion_texture_x_offset", 0.0f);
        e.p[2] = param(m, "bright_offset", 0.0f);
        e.p[3] = param(m, "hue_shift", 0.0f);
        e.tex[0] = effect_texture(c, m, "erosion_texture");
        e.tex[1] = effect_texture(c, m, "bright_lut");
        e.repeat = 1;
    } else if (s == "shaders/vfx/ui/card/exhaust/vfx_card_exhaust_quick_erosion_shader.gdshader") {
        e.id = EFFECT_EXHAUST_QUICK;
        CurveData::from(shader_param_resource(m, "erosion_curve"), g_effect_curve);
        e.p[0] = g_effect_curve.valid() ? g_effect_curve.sample(0) : 0.0f;
        e.p[1] = param(m, "erosion_offset", 0.1f);
        e.p[2] = param(m, "bright_offset", 0.0f);
        e.p[3] = param(m, "hue_shift", 0.0f);
        e.tex[0] = effect_texture(c, m, "bright_lut");
        g_effect_particle = EFFECT_EXHAUST_QUICK;
        g_effect_life_param = 0;
    } else if (s == "shaders/dissolve.gdshader") {
        /* TEXTURE_PIXEL_SIZE: the drawn texture's (a Sprite2D's viewport) */
        Vec2 ts(1, 1);
        if (g_item && g_item->type == "Sprite2D" && ((const Sprite2D*)g_item)->texture)
            ts = ((const Sprite2D*)g_item)->texture->size();
        e.id = EFFECT_DISSOLVE;
        e.p[0] = param(m, "threshold", 0.0f);
        e.p[1] = -pan(0.25f);
        e.p[2] = ts.x > 0 ? 1.0f / ts.x : 1.0f;
        e.p[3] = ts.y > 0 ? 1.0f / ts.y : 1.0f;
        e.tex[0] = effect_texture(c, m, "dissolveGradient1");
        e.tex[1] = effect_texture(c, m, "dissolveGradient2");
        e.repeat = 3;
    } else if (s == "shaders/vfx/dagger/vfx_dagger_spray_single_shader.gdshader") {
        e.id = EFFECT_LUT_ALPHA;
        e.tex[0] = effect_texture(c, m, "lut");
    } else {
        return false;
    }
    *ok = set_effect(c, e);
    return true;
}

/* A texture's uv: scale, and the offset its panning has moved it by now
 * (kept within 0..1: the textures repeat). */
static void scroll_uv(const MaterialRes* m, const char* scale, const char* pan, const char* offset, Vec2 global,
                      float out[4]) {
    Vec2 s = shader_param_vec2(m, scale, Vec2(1, 1));
    Vec2 p = pan ? shader_param_vec2(m, pan, Vec2(1, 1)) : Vec2(0, 0);
    Vec2 o = offset ? shader_param_vec2(m, offset, Vec2(0, 0)) : Vec2(0, 0);
    float t = (float)g_shader_time;
    out[0] = s.x;
    out[1] = s.y;
    out[2] = o.x + global.x + (pan ? fmodf(p.x * t, 1.0f) : 0.0f);
    out[3] = o.y + global.y + (pan ? fmodf(p.y * t, 1.0f) : 0.0f);
}

/* The erosion particle shaders' parameters while one is applied. */
static VfxParams g_vfx;
static bool g_vfx_on = false;

const VfxParams* shader_vfx() { return g_vfx_on ? &g_vfx : NULL; }

/* The erosion particle shaders: VfxParams from the material; the
 * threshold here is the erosion curve's start (particles set theirs). */
static bool vfx_shader(Canvas& c, const MaterialRes* m) {
    const std::string& s = m->shader;
    bool ring = s == "shaders/vfx/common/vfx_ring_polar_shader.gdshader";
    bool poof = s == "shaders/vfx/common/vfx_poof_shader.gdshader";
    bool common = s == "shaders/vfx/common/vfx_common_particle_shader.gdshader";
    if (!ring && !poof && !common) return false;
    VfxParams v;
    v.lut = ring ? NULL : gradient_lut(c, shader_param_resource(m, "lut"));
    v.polar = ring ? 1.0f : 0.0f;
    CurveData curve;
    CurveData::from(shader_param_resource(m, "erosion_curve"), curve);
    v.erosion[0] = curve.valid() ? curve.sample(0) : 0.0f;
    v.erosion[1] = param(m, "erosion_offset", ring ? 0.1f : 0.25f);
    v.erosion[2] = 1.0f;
    v.erosion[3] = common ? 1.0f : 0.0f;
    v.distortion[0] = v.distortion[1] = v.distortion[2] = v.distortion[3] = 0.0f;
    if (poof) {
        Vec2 pan = shader_param_vec2(m, "distortion_panning_speed", Vec2(0, 0));
        float t = (float)g_shader_time;
        v.distortion[0] = fmodf(pan.x * t, 1.0f);
        v.distortion[1] = fmodf(pan.y * t, 1.0f);
        v.distortion[2] = param(m, "distortion_strength", 0.0f);
        v.distortion[3] = 1.0f;
    }
    g_vfx = v;
    g_vfx_on = true;
    c.renderer()->set_vfx(&v);
    return true;
}

typedef float M3[3][3];

static void mul(const M3 a, const M3 b, M3 out) {
    for (int i = 0; i < 3; ++i)
        for (int j = 0; j < 3; ++j) out[i][j] = a[i][0] * b[0][j] + a[i][1] * b[1][j] + a[i][2] * b[2][j];
}

static void inverse(const M3 m, M3 out) {
    float det = m[0][0] * (m[1][1] * m[2][2] - m[1][2] * m[2][1]) - m[0][1] * (m[1][0] * m[2][2] - m[1][2] * m[2][0]) +
                m[0][2] * (m[1][0] * m[2][1] - m[1][1] * m[2][0]);
    float id = det != 0 ? 1.0f / det : 0.0f;
    out[0][0] = (m[1][1] * m[2][2] - m[1][2] * m[2][1]) * id;
    out[0][1] = (m[0][2] * m[2][1] - m[0][1] * m[2][2]) * id;
    out[0][2] = (m[0][1] * m[1][2] - m[0][2] * m[1][1]) * id;
    out[1][0] = (m[1][2] * m[2][0] - m[1][0] * m[2][2]) * id;
    out[1][1] = (m[0][0] * m[2][2] - m[0][2] * m[2][0]) * id;
    out[1][2] = (m[0][2] * m[1][0] - m[0][0] * m[1][2]) * id;
    out[2][0] = (m[1][0] * m[2][1] - m[1][1] * m[2][0]) * id;
    out[2][1] = (m[0][1] * m[2][0] - m[0][0] * m[2][1]) * id;
    out[2][2] = (m[0][0] * m[1][1] - m[0][1] * m[1][0]) * id;
}

/* shaders/hsv.gdshader: rgb' = YIQ^-1 * v * S(s) * H(h)^T * YIQ * rgb
 * (GLSL "col.rgb *= hue_shift" multiplies by the transpose). */
void Canvas::hsv_matrix(float h, float s, float v, float out[12]) {
    const M3 yiq = { { 0.2989f, 0.5870f, 0.1140f }, { 0.5959f, -0.2774f, -0.3216f }, { 0.2115f, -0.5229f, 0.3114f } };
    float hue = (1.0f - h) * 6.283185f;
    float sh = sinf(hue), ch = cosf(hue);
    const M3 ht = { { 1, 0, 0 }, { 0, ch, -sh }, { 0, sh, ch } };
    const M3 sv = { { v, 0, 0 }, { 0, s * v, 0 }, { 0, 0, s * v } };
    M3 a, b, inv, r;
    mul(ht, yiq, a);
    mul(sv, a, b);
    inverse(yiq, inv);
    mul(inv, b, r);
    for (int i = 0; i < 3; ++i) {
        out[i * 4 + 0] = r[i][0];
        out[i * 4 + 1] = r[i][1];
        out[i * 4 + 2] = r[i][2];
        out[i * 4 + 3] = 0;
    }
}

bool shader_begin(Canvas& c, const MaterialRes* m, const CanvasItem* item) {
    if (!m || m->kind != Resource::RES_SHADER_MATERIAL) return true;
    g_item = item;
    if (m->shader == "shaders/hsv.gdshader") {
        float cm[12];
        Canvas::hsv_matrix(param(m, "h", 1.0f), param(m, "s", 1.0f), param(m, "v", 1.0f), cm);
        c.set_color_matrix(cm);
        return true;
    }
    if (m->shader == "shaders/vfx/wind_sway.gdshader") {
        /* getWind in the vertex shader: time = TIME * speed + offset; the
         * strength eases between minStrength and maxStrength (GLSL clamp:
         * min(max(x, lo), hi), so maxStrength wins when it is the lower),
         * times strengthScale; (sin(time) + cos(time * detail)) of it. */
        float time = (float)g_shader_time * param(m, "speed", 1.0f) + param(m, "offset", 0.0f);
        float lo = param(m, "minStrength", 0.05f), hi = param(m, "maxStrength", 0.01f);
        float diff = (hi - lo) * (hi - lo);
        float x = lo + diff + sinf(time / param(m, "interval", 3.5f)) * diff;
        float strength = (x < lo ? lo : x);
        strength = (strength > hi ? hi : strength) * param(m, "strengthScale", 100.0f);
        float wind = (sinf(time) + cosf(time * param(m, "detail", 1.0f))) * strength;
        c.set_wind(true, wind, param(m, "heightOffset", 0.0f));
        return true;
    }
    if (m->shader == "shaders/power_flash_vfx.gdshader") {
        /* COLOR *= texel with its rgb times strength (render_mode
         * blend_add: shader_blend). */
        float k = param(m, "strength", 3.0f);
        float cm[12] = { k, 0, 0, 0, 0, k, 0, 0, 0, 0, k, 0 };
        c.set_color_matrix(cm);
        return true;
    }
    if (m->shader == "shaders/blur/canvas_group_mask_blur.gdshader") {
        /* CanvasGroup draws its mask itself (controls.cpp); the blur mode
         * is not ported. */
        return true;
    }
    if (m->shader == "shaders/dark_blur.gdshader") {
        ScreenParams p;
        memset(&p, 0, sizeof(p));
        p.mode = SCREEN_DARK;
        p.lod = param(m, "lod", 5.0f);
        p.mix = param(m, "mix_percentage", 0.3f);
        return c.renderer()->set_screen(&p);
    }
    if (m->shader == "shaders/vfx/vfx_water_reflection.gdshader") {
        EffectParams e;
        memset(&e, 0, sizeof(e));
        e.id = EFFECT_WATER_REFLECTION;
        e.p[0] = -fmodf((float)g_shader_time * param(m, "speed", 0.1f), 1.0f);
        e.p[1] = param(m, "strength", 0.1f);
        e.tex[0] = param_texture(c, m, "noise");
        e.tex[1] = param_texture(c, m, "mask");
        e.repeat = 1;
        g_effect_particle = 0;
        return set_effect(c, e);
    }
    if (m->shader == "shaders/doom_overlay.gdshader") {
        /* The pulse: 0.7 * smoothstep(-10, 1, sin(TIME * 20) + sin(TIME * 10)). */
        ScreenParams p;
        memset(&p, 0, sizeof(p));
        p.mode = SCREEN_DOOM;
        p.mix = param(m, "s", 1.0f);
        p.strength = param(m, "v", 1.0f);
        float t = (float)g_shader_time;
        float x = sinf(t * 20) + sinf(t * 10);
        float k = clampf((x + 10) / 11, 0, 1);
        p.power = 0.7f * k * k * (3 - 2 * k);
        Color g = param_color(m, "gradient_color", Color(0, 0, 0, 1));
        p.color[0] = g.r;
        p.color[1] = g.g;
        p.color[2] = g.b;
        p.color[3] = g.a;
        return c.renderer()->set_screen(&p);
    }
    if (m->shader == "shaders/vfx/debug/debug_ancient_card_border_overlay.gdshader") {
        ScreenParams p;
        memset(&p, 0, sizeof(p));
        p.mode = SCREEN_GLASS;
        p.lod = param(m, "blur_amount", 0.0f);
        p.strength = param(m, "add_amount", 0.0f);
        p.mix = param(m, "corner_weight", 0.0f);
        p.mask = param_texture(c, m, "mask");
        Color cc = param_color(m, "corner_color", Color(1, 1, 1, 1));
        p.color[0] = cc.r;
        p.color[1] = cc.g;
        p.color[2] = cc.b;
        p.color[3] = cc.a;
        return c.renderer()->set_screen(&p);
    }
    if (m->shader == "shaders/radial_blur.gdshader") {
        ScreenParams p;
        memset(&p, 0, sizeof(p));
        p.mode = SCREEN_RADIAL;
        Vec2 bc = shader_param_vec2(m, "blur_center", Vec2(0.45f, 0.55f));
        p.center[0] = param(m, "blur_center.x", bc.x);
        p.center[1] = param(m, "blur_center.y", bc.y);
        p.power = param(m, "blur_power", 0.005f);
        p.count = (int)param(m, "sampling_count", 12.0f);
        return c.renderer()->set_screen(&p);
    }
    if (m->shader == "shaders/vfx/vfx_water_reflection_post.gdshader" ||
        m->shader == "shaders/vfx/the_insatiable_sand_fall_2.gdshader") {
        /* noise(UV + -TIME * speed (* direction)): the offset, kept within
         * 0..1 (the noise repeats). */
        ScreenParams p;
        memset(&p, 0, sizeof(p));
        p.mode = SCREEN_NOISE;
        p.noise = param_texture(c, m, "noise");
        p.mask = param_texture(c, m, "mask");
        p.strength = param(m, "strength", 0.1f);
        float t = (float)g_shader_time * param(m, "speed", 0.1f);
        float dx = 1, dy = 1;
        if (m->shader == "shaders/vfx/the_insatiable_sand_fall_2.gdshader") {
            float a = param(m, "angle_degrees", 45.0f) * 3.14159265f / 180.0f;
            dx = cosf(a);
            dy = sinf(a);
        }
        p.noise_offset[0] = -fmodf(t * dx, 1.0f);
        p.noise_offset[1] = -fmodf(t * dy, 1.0f);
        return c.renderer()->set_screen(&p);
    }
    if (m->shader == "shaders/button_pulse.gdshader") {
        /* COLOR.rgb += |sin(2 TIME)| * 0.2 * pulse_strength (added to the
         * texel here, i.e. before modulation: exact for white modulate). */
        float k = fabsf(sinf((float)g_shader_time * 2.0f)) * 0.2f * param(m, "pulse_strength", 0.0f);
        float cm[12] = { 1, 0, 0, k, 0, 1, 0, k, 0, 0, 1, k };
        c.set_color_matrix(cm);
        return true;
    }
    if (m->shader == "shaders/power.gdshader") {
        /* COLOR.rgb += |sin(3 TIME)| * 0.2 while pulse == 1 (NPower's
         * PulsingStarted). */
        float k = param(m, "pulse", 0.0f) == 1.0f ? fabsf(sinf((float)g_shader_time * 3.0f)) * 0.2f : 0.0f;
        if (k == 0.0f) return true;
        float cm[12] = { 1, 0, 0, k, 0, 1, 0, k, 0, 0, 1, k };
        c.set_color_matrix(cm);
        return true;
    }
    if (m->shader == "shaders/relic.gdshader") {
        /* rgb += |sin(3 TIME)| * 0.2 while pulse == 1; is_used: the
         * average grey; is_wax: that grey * (1, 0.8, 0.8). */
        float k = param(m, "pulse", 0.0f) == 1.0f ? fabsf(sinf((float)g_shader_time * 3.0f)) * 0.2f : 0.0f;
        bool used = param(m, "is_used", 0.0f) == 1.0f, wax = param(m, "is_wax", 0.0f) == 1.0f;
        if (k == 0.0f && !used && !wax) return true;
        float cm[12] = { 1, 0, 0, k, 0, 1, 0, k, 0, 0, 1, k };
        if (used || wax) {
            static const float tint[3] = { 1.0f, 0.8f, 0.8f };
            for (int r = 0; r < 3; ++r) {
                float w = wax ? tint[r] : 1.0f;
                cm[r * 4 + 0] = cm[r * 4 + 1] = cm[r * 4 + 2] = w / 3.0f;
                cm[r * 4 + 3] = w * k;
            }
        }
        c.set_color_matrix(cm);
        return true;
    }
    if (m->shader == "scenes/ui/normal_map_point.gdshader") {
        /* Texels within 0.5 of initial_color become map_color. The icons
         * are two-tone (a dark body, light features near initial_color):
         * per channel, the line through the dark tone (kept) and
         * initial_color (to map_color), which also keeps the edges'
         * antialiasing. */
        Color from = param_color(m, "initial_color", Color(0.705882f, 0.615686f, 0.537255f, 1));
        Color to = param_color(m, "map_color", from);
        static const float dark[3] = { 0.17f, 0.13f, 0.08f };
        const float f[3] = { from.r, from.g, from.b }, t[3] = { to.r, to.g, to.b };
        float cm[12] = { 0 };
        for (int r = 0; r < 3; ++r) {
            float k = f[r] - dark[r] > 0.01f ? (t[r] - dark[r]) / (f[r] - dark[r]) : 1.0f;
            cm[r * 4 + r] = k;
            cm[r * 4 + 3] = dark[r] * (1 - k);
        }
        c.set_color_matrix(cm);
        return true;
    }
    if (m->shader == "scenes/rest_site/rest_site_light_shader.gdshader") {
        /* The campfire's light: the texture wobbled by a noise texture and
         * dimmed by a flickering grey ramp (0.62..1), both scrolled by
         * TIME. Here: the texture as it is, flickering about as much. */
        float t = (float)g_shader_time;
        float k = 0.84f + 0.1f * sinf(t * 7.3f) * sinf(t * 2.9f + 1.0f) + 0.05f * sinf(t * 13.1f);
        float cm[12] = { k, 0, 0, 0, 0, k, 0, 0, 0, 0, k, 0 };
        c.set_color_matrix(cm);
        return true;
    }
    if (m->shader == "shaders/vfx/common/vfx_grayscale_particle_shader.gdshader") {
        /* The texel's red through the lut, times the particle's colour;
         * the alpha is the red. */
        c.renderer()->set_gradient_map(gradient_lut(c, shader_param_resource(m, "lut")), true);
        return true;
    }
    if (m->shader == "shaders/vfx/common/vfx_flipbook_shader.gdshader" ||
        m->shader == "shaders/vfx/common/vfx_row_flipbook_shader.gdshader" ||
        m->shader == "shaders/vfx/common/vfx_projectile_flipbook_shader.gdshader") {
        /* The colour as above, the texel's alpha; the frame of the flipbook
         * is the particles' (Particles2D::draw). Erosion and hue shift are
         * not ported. */
        c.renderer()->set_gradient_map(gradient_lut(c, shader_param_resource(m, "lut")), false);
        return true;
    }
    if (m->shader == "shaders/vfx/vfx_stepped_shader_fire_add.tres" ||
        m->shader == "shaders/vfx/vfx_stepped_shader_fire_flat.tres" ||
        m->shader == "shaders/vfx/vfx_stepped_shader_fire_dark.tres") {
        /* The stepped fire (FireParams; add: blend_add, see shader_blend). The
         * dark variant moves every panning by GlobalOffset. */
        bool dark = m->shader == "shaders/vfx/vfx_stepped_shader_fire_dark.tres";
        Vec2 g = dark ? shader_param_vec2(m, "GlobalOffset", Vec2(0, 0)) : Vec2(0, 0);
        FireParams f;
        f.noise1 = param_texture(c, m, "Noise1Texture");
        f.noise2 = param_texture(c, m, "Noise2Texture");
        f.noise_mask = param_texture(c, m, "NoiseMask");
        f.dist1 = param_texture(c, m, "Distortion1Texture");
        f.dist2 = param_texture(c, m, "Distortion2Texture");
        f.dist_mask = param_texture(c, m, "DistortionMask");
        scroll_uv(m, "Noise1Scaling", "Noise1Panning", NULL, g, f.noise1_uv);
        scroll_uv(m, "Noise2Scaling", "Noise2Panning", NULL, g, f.noise2_uv);
        scroll_uv(m, "NoiseMaskScale", NULL, "NoiseMaskOffset", Vec2(0, 0), f.mask_uv);
        scroll_uv(m, "Distortion1Scale", "Distortion1Panning", NULL, g, f.dist1_uv);
        scroll_uv(m, "Distortion2Scale", "Distortion2Panning", NULL, g, f.dist2_uv);
        scroll_uv(m, "DistortionMaskScale", NULL, "DistortionMaskOffset", Vec2(0, 0), f.dist_mask_uv);
        f.strength[0] = param(m, "Noise1Strength", 1.0f);
        f.strength[1] = param(m, "Noise2Strength", 1.0f);
        f.strength[2] = param(m, "InvertNoiseMask", 1.0f) != 0.0f ? 1.0f : 0.0f;
        f.strength[3] = dark ? 1.0f : 0.0f;
        f.dist_strength[0] = param(m, "Distortion1Strength", 0.02f);
        f.dist_strength[1] = param(m, "Distortion2Strength", 0.02f);
        f.dist_strength[2] = f.dist_strength[3] = 0;
        Vec2 inner_step = shader_param_vec2(m, "InnerColorStep", Vec2(0.3f, 0.4f));
        Vec2 outer_step = shader_param_vec2(m, "OuterStep", Vec2(0.1f, 0.2f));
        f.steps[0] = inner_step.x;
        f.steps[1] = inner_step.y;
        f.steps[2] = outer_step.x;
        f.steps[3] = outer_step.y;
        Color in = param_color(m, "InnerColor", Color(1, 0.85f, 0, 1));
        Color out = param_color(m, "OuterColor", Color(1, 0, 0, 1));
        f.inner[0] = in.r;
        f.inner[1] = in.g;
        f.inner[2] = in.b;
        f.inner[3] = in.a;
        f.outer[0] = out.r;
        f.outer[1] = out.g;
        f.outer[2] = out.b;
        f.outer[3] = out.a;
        c.renderer()->set_fire(&f);
        return true;
    }
    if (m->shader == "scenes/combat/doom_bar.gdshader") {
        /* SCREEN_UV noise panning and the scene's purple gradient. Use the
         * multi-sampler mode with selector 2 for this fragment shader. */
        FireParams f;
        memset(&f, 0, sizeof(f));
        f.noise1 = param_texture(c, m, "noise_tex");
        f.strength[3] = 2;
        f.strength[0] = (float)g_shader_time;
        c.renderer()->set_gradient_map(gradient_lut(c, shader_param_resource(m, "gradient_tex")), false);
        c.renderer()->set_fire(&f);
        return true;
    }
    if (vfx_shader(c, m)) return true;
    bool ok = true;
    if (effect_shader(c, m, &ok)) return ok;
    if (m->shader == "shaders/card_ripple.gdshader") {
        /* render_mode blend_add: shader_blend. */
        float p[4] = { (float)g_shader_time * param(m, "ripple_speed", 0.0f), param(m, "modulo_width", 0.02f),
                       param(m, "ease", 0.0f), param(m, "width", 0.0f) };
        c.set_ripple(p);
        return true;
    }
    return false;
}

/* Blend modes from a shader's render_mode (the tree applies the item's
 * blend after shader_begin). */
bool shader_blend(const MaterialRes* m, BlendMode* out) {
    if (m && m->kind == Resource::RES_SHADER_MATERIAL && m->shader == "shaders/vfx/vfx_stepped_shader_fire_add.tres") {
        *out = BLEND_ADD;
        return true;
    }
    if (m && m->kind == Resource::RES_SHADER_MATERIAL && m->shader == "shaders/card_ripple.gdshader") {
        *out = BLEND_ADD;
        return true;
    }
    if (m && m->kind == Resource::RES_SHADER_MATERIAL && m->shader == "shaders/power_flash_vfx.gdshader") {
        *out = BLEND_ADD;
        return true;
    }
    if (m && m->kind == Resource::RES_SHADER_MATERIAL && m->shader == "shaders/dissolve.gdshader") {
        *out = BLEND_PREMULT_ALPHA;
        return true;
    }
    if (m && m->kind == Resource::RES_SHADER_MATERIAL && m->shader == "images/vfx/simple_noise_add_shader.tres") {
        *out = BLEND_ADD;
        return true;
    }
    return false;
}

void shader_end(Canvas& c, const MaterialRes* m) {
    g_item = NULL; /* shader_param outside a draw reads the material only */
    if (m && m->kind == Resource::RES_SHADER_MATERIAL) {
        c.set_color_matrix(NULL);
        c.set_ripple(NULL);
        c.set_wind(false, 0, 0);
        c.renderer()->set_gradient_map(NULL, false);
        c.renderer()->set_fire(NULL);
        c.renderer()->set_vfx(NULL);
        c.renderer()->set_screen(NULL);
        c.renderer()->set_effect(NULL);
        g_vfx_on = false;
        g_effect_particle = 0;
    }
}

} /* namespace spire */

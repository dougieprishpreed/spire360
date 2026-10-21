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
    /* the item's "<name>.x" / "<name>.y" (a script's SetShaderParameter) */
    if (g_item) {
        std::map<std::string, float>::const_iterator x = g_item->shader_params.find(std::string(name) + ".x");
        std::map<std::string, float>::const_iterator y = g_item->shader_params.find(std::string(name) + ".y");
        if (x != g_item->shader_params.end() && y != g_item->shader_params.end()) return Vec2(x->second, y->second);
    }
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
 * red) or a CurveXYZTexture (curve_x, curve_y, curve_z in red, green,
 * blue), 256 wide, values clamped to 0..1, once per resource. */
static Texture* curve_texture(Canvas& c, Resource* r) {
    std::string key = "curvetex:" + r->path + format("%p", (void*)r);
    Texture* cached = c.cached_texture(key);
    if (cached) return cached;
    if (r->type == "CurveXYZTexture") {
        CurveData xyz[3];
        for (int k = 0; k < 3; ++k) CurveData::from_xyz(r, k, xyz[k]);
        TextureData td;
        td.format = TEX_ARGB8;
        td.flags = TEXF_DATA;
        td.width = td.orig_width = 256;
        td.height = td.orig_height = 1;
        td.levels.resize(1);
        td.levels[0].resize(256 * 4);
        for (int i = 0; i < 256; ++i) {
            u8* px = &td.levels[0][(size_t)i * 4]; /* B G R A */
            for (int k = 0; k < 3; ++k) {
                float v = xyz[k].valid() ? clampf(xyz[k].sample((float)i / 255.0f), 0, 1) : 0.0f;
                px[2 - k] = (u8)(v * 255 + 0.5f);
            }
            px[3] = 255;
        }
        return c.cache_texture(key, td);
    }
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
    if (r->type == "CurveTexture" || r->type == "CurveXYZTexture") return curve_texture(c, r);
    return NULL;
}

/* NCrystalSphereMask's gridFadeParams as the cells' alphas now: the item's
 * shader params "cell_<i>" (the screen computes them; 1 when unset) in an
 * 11 x 11 texture's red, rebuilt when they change. */
static Texture* scry_cells(Canvas& c, const CanvasItem* item) {
    static std::map<const CanvasItem*, std::pair<std::vector<u8>, Texture*> > cache;
    std::vector<u8> a(121, 255);
    for (int i = 0; item && i < 121; ++i) {
        std::map<std::string, float>::const_iterator it = item->shader_params.find(format("cell_%d", i));
        if (it != item->shader_params.end()) a[(size_t)i] = (u8)(clampf(it->second, 0, 1) * 255 + 0.5f);
    }
    std::pair<std::vector<u8>, Texture*>& slot = cache[item];
    if (slot.second && slot.first == a) return slot.second;
    if (slot.second) Canvas::release_later(slot.second);
    TextureData td;
    td.format = TEX_ARGB8;
    td.flags = TEXF_DATA;
    td.width = td.orig_width = 11;
    td.height = td.orig_height = 11;
    td.levels.resize(1);
    td.levels[0].resize(121 * 4);
    for (int i = 0; i < 121; ++i) {
        u8* px = &td.levels[0][(size_t)i * 4]; /* B G R A */
        px[0] = px[1] = 0;
        px[2] = a[(size_t)i];
        px[3] = 255;
    }
    slot.first = a;
    slot.second = c.renderer()->create_texture(td);
    return slot.second;
}

/* The effect applied now and how it changes per particle. */
static EffectParams g_effect;
static int g_effect_particle = 0; /* the effect when it changes per particle */
/* Per particle: parameter g_life_param[i] = g_life_scale[i] * curve i at
 * the particle's life (hint_default_black: 0 without a curve; -1: none);
 * g_cz_param[i] += g_cz_scale[i] * CUSTOM.z. */
static int g_life_param[2], g_cz_param[3], g_cx_param;
static float g_life_scale[2], g_cz_scale[3];
static CurveData g_effect_curve, g_effect_curve2;

static void effect_particles(int id) {
    g_effect_particle = id;
    g_life_param[0] = g_life_param[1] = -1;
    g_cz_param[0] = g_cz_param[1] = g_cz_param[2] = -1;
    g_cx_param = -1;
    g_life_scale[0] = g_life_scale[1] = 1;
}

bool shader_effect_particle(float life, float custom_z, EffectParams* out, float custom_x) {
    if (!g_effect_particle) return false;
    *out = g_effect;
    const CurveData* curves[2] = { &g_effect_curve, &g_effect_curve2 };
    for (int i = 0; i < 2; ++i)
        if (g_life_param[i] >= 0)
            out->p[g_life_param[i]] = g_life_scale[i] * (curves[i]->valid() ? curves[i]->sample(life) : 0.0f);
    for (int i = 0; i < 3; ++i)
        if (g_cz_param[i] >= 0) out->p[g_cz_param[i]] += custom_z * g_cz_scale[i];
    if (g_cx_param >= 0) out->p[g_cx_param] += custom_x;
    return true;
}

/* The screen shader applied now when it changes per particle. */
static ScreenParams g_screen_base;
static bool g_screen_particle = false;
static CurveData g_screen_curve;

bool shader_screen_particle(float life, ScreenParams* out) {
    if (!g_screen_particle) return false;
    *out = g_screen_base;
    out->strength = g_screen_base.strength * (g_screen_curve.valid() ? g_screen_curve.sample(life) : 1.0f);
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
    } else if (s == "images/vfx/simple_noise_flat_shader.tres" || s == "images/vfx/simple_noise_add_shader.tres" ||
               s == "inline:de17fd3cee97c1a1") {
        /* inline:de17fd3cee97c1a1: the Necrobinder's own copy of the flat
         * two-noise one (the same parameters) */
        bool two = s != "images/vfx/simple_noise_add_shader.tres";
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
    } else if (s == "inline:08658321c711a98e") {
        /* The spectral knight's flame: the one-noise simple noise, its
         * noise at UV + TIME * mask_offset * (2, 0.2). */
        e.id = EFFECT_SIMPLE_NOISE;
        Vec2 st = shader_param_vec2(m, "Step", Vec2(0, 0)), off = shader_param_vec2(m, "mask_offset", Vec2(2, 3));
        e.p[0] = st.x;
        e.p[1] = st.y;
        e.p[2] = e.p[3] = 1;
        e.p[4] = pan(off.x * 2.0f);
        e.p[5] = pan(off.y * 0.2f);
        e.p[6] = e.p[7] = 1;
        e.tex[0] = effect_texture(c, m, "noise_tex");
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
    } else if (s == "shaders/vfx/vfx_doom_flame_shader.tres") {
        e.id = EFFECT_DOOM_FLAME;
        Vec2 s1 = shader_param_vec2(m, "Noise1Scaling", Vec2(1, 1)), p1 = shader_param_vec2(m, "Noise1Panning", Vec2(1, 1));
        Vec2 s2 = shader_param_vec2(m, "Noise2Scaling", Vec2(1, 1)), p2 = shader_param_vec2(m, "Noise2Panning", Vec2(1, 1));
        Vec2 ms = shader_param_vec2(m, "NoiseMaskScale", Vec2(1, 1)), mo = shader_param_vec2(m, "NoiseMaskOffset", Vec2(0, 0));
        Vec2 ics = shader_param_vec2(m, "InnerColorStep", Vec2(0.3f, 0.4f)), os = shader_param_vec2(m, "OuterStep", Vec2(0.1f, 0.2f));
        e.p[0] = s1.x;
        e.p[1] = s1.y;
        e.p[2] = pan(p1.x);
        e.p[3] = pan(p1.y);
        e.p[4] = s2.x;
        e.p[5] = s2.y;
        e.p[6] = pan(p2.x);
        e.p[7] = pan(p2.y);
        e.p[8] = ms.x;
        e.p[9] = ms.y;
        e.p[10] = mo.x;
        e.p[11] = mo.y;
        e.p[12] = param(m, "Noise1Strength", 1.0f) * param(m, "Noise2Strength", 1.0f);
        e.p[13] = param(m, "InvertNoiseMask", 1.0f) != 0.0f ? 1.0f : 0.0f;
        e.p[14] = ics.x;
        e.p[15] = ics.y;
        Vec2 d1s = shader_param_vec2(m, "Distortion1Scale", Vec2(1, 1)), d1p = shader_param_vec2(m, "Distortion1Panning", Vec2(1, 1));
        Vec2 d2s = shader_param_vec2(m, "Distortion2Scale", Vec2(1, 1)), d2p = shader_param_vec2(m, "Distortion2Panning", Vec2(1, 1));
        Color outer = param_color(m, "OuterColor", Color(1, 0, 0, 1)), inner = param_color(m, "InnerColor", Color(1, 0.85f, 0, 1));
        float x[20] = { d1s.x, d1s.y, pan(d1p.x), pan(d1p.y), d2s.x, d2s.y, pan(d2p.x), pan(d2p.y),
                        param(m, "Distortion1Strength", 0.02f), param(m, "Distortion2Strength", 0.02f), os.x, os.y,
                        outer.r, outer.g, outer.b, 0, inner.r, inner.g, inner.b, 0 };
        for (int k = 0; k < 20; ++k) e.extra[k] = x[k];
        e.extra_count = 5;
        e.tex[0] = effect_texture(c, m, "Noise1Texture");
        e.tex[1] = effect_texture(c, m, "Noise2Texture");
        e.tex[2] = effect_texture(c, m, "NoiseMask");
        e.tex[3] = effect_texture(c, m, "Distortion1Texture");
        e.tex[4] = effect_texture(c, m, "Distortion2Texture");
        e.repeat = 1 | 2 | 8 | 16;
    } else if (s == "shaders/vfx/forms/demon/vfx_demon_form_idle_panning_fire_shader.gdshader" ||
               s == "shaders/vfx/forms/reaper/vfx_reaper_form_idle_panning_noise_shader.gdshader") {
        /* tiling_and_offset: UV * st.xy + st.zw * TIME + initial_offset */
        bool reaper = s == "shaders/vfx/forms/reaper/vfx_reaper_form_idle_panning_noise_shader.gdshader";
        static const float st0[4] = { 0, 0, 0, 0 };
        float a[4], b[4];
        param_vec4(m, "main_st_a", st0, a);
        param_vec4(m, "main_st_b", st0, b);
        Vec2 ia = shader_param_vec2(m, "main_st_a_initial_offset", Vec2(0, 0));
        Vec2 ib = shader_param_vec2(m, "main_st_b_initial_offset", Vec2(0, 0));
        Vec2 er = shader_param_vec2(m, "erosion", Vec2(0, 0));
        e.id = EFFECT_FORM_PANNING;
        e.p[0] = a[0];
        e.p[1] = a[1];
        e.p[2] = fmodf(pan(a[2]) + ia.x, 1.0f);
        e.p[3] = fmodf(pan(a[3]) + ia.y, 1.0f);
        e.p[4] = b[0];
        e.p[5] = b[1];
        e.p[6] = fmodf(pan(b[2]) + ib.x, 1.0f);
        e.p[7] = fmodf(pan(b[3]) + ib.y, 1.0f);
        e.p[8] = er.x;
        e.p[9] = er.y;
        float div = param(m, "mask_divisor", 0.5f);
        e.p[10] = div != 0 ? 1.0f / div : 0.0f;
        e.p[11] = reaper ? 1.0f : 0.0f;
        e.p[12] = 3.14159265f * param(m, "rotation_normalized", 0.0f);
        e.p[13] = reaper ? 0.0f : 1.0f;
        e.tex[0] = effect_texture(c, m, "lut");
        e.tex[1] = effect_texture(c, m, "alpha_overlay");
        e.tex[2] = effect_texture(c, m, "mask");
    } else if (s == "shaders/vfx/forms/serpent/vfx_serpent_form_snakes_shader.gdshader") {
        static const float st0[4] = { 0, 0, 0, 0 };
        float st[4];
        param_vec4(m, "distortion_st", st0, st);
        Vec2 io = shader_param_vec2(m, "distortion_initial_offset", Vec2(0, 0));
        Vec2 dir = shader_param_vec2(m, "distortion_dir", Vec2(0, 0));
        e.id = EFFECT_SERPENT_SNAKES;
        e.p[0] = st[0];
        e.p[1] = st[1];
        e.p[2] = fmodf(pan(st[2]) + io.x, 1.0f);
        e.p[3] = fmodf(pan(st[3]) + io.y, 1.0f);
        e.p[4] = param(m, "distortion_strength", 0.0f);
        e.p[5] = dir.x;
        e.p[6] = dir.y;
        e.tex[0] = effect_texture(c, m, "lut");
    } else if (s == "shaders/vfx/forms/echo/vfx_echo_form_echo_lines_shader.gdshader") {
        Vec2 sc = shader_param_vec2(m, "scale_center", Vec2(0.5f, 0.5f));
        Vec2 ss = shader_param_vec2(m, "scale_alpha_smoothstep", Vec2(0, 1));
        e.id = EFFECT_ECHO_LINES;
        e.p[0] = sc.x;
        e.p[1] = sc.y;
        e.p[2] = param(m, "scale_factor", 1.1f);
        e.p[3] = ss.x;
        e.p[4] = ss.y;
        e.tex[0] = effect_texture(c, m, "mask_texture");
    } else if (s == "shaders/vfx/common/vfx_round_smoke_shader.gdshader") {
        Vec2 sp = shader_param_vec2(m, "distortion_panning_speed", Vec2(0, 0));
        e.id = EFFECT_ROUND_SMOKE;
        e.p[0] = sp.x * (float)g_shader_time;
        e.p[1] = sp.y * (float)g_shader_time;
        e.p[2] = param(m, "distortion_strength", 0.0f);
        CurveData::from(shader_param_resource(m, "erosion_curve"), g_effect_curve);
        e.p[3] = g_effect_curve.valid() ? g_effect_curve.sample(0) : 0.0f;
        e.p[4] = param(m, "erosion_offset", 0.25f);
        e.tex[0] = effect_texture(c, m, "lut");
        effect_particles(EFFECT_ROUND_SMOKE);
        g_life_param[0] = 3;
    } else if (s == "shaders/vfx/scream/vfx_scream_squiggly_streaks_shader.gdshader") {
        Vec2 tl = shader_param_vec2(m, "distortion_tiling", Vec2(0, 0)), sp = shader_param_vec2(m, "distortion_offset_speed", Vec2(0, 0));
        e.id = EFFECT_SCREAM_STREAKS;
        e.p[0] = tl.x;
        e.p[1] = tl.y;
        e.p[2] = sp.x * (float)g_shader_time;
        e.p[3] = sp.y * (float)g_shader_time;
        e.p[4] = param(m, "distortion_strength", 0.05f);
        e.p[5] = 0;
        effect_particles(EFFECT_SCREAM_STREAKS);
        g_cx_param = 5;
    } else if (s == "shaders/vfx/ui/card/remove/vfx_card_remove_erosion_shader.gdshader") {
        e.id = EFFECT_CARD_REMOVE;
        e.p[0] = param(m, "distortion_speed", 1.0f) * (float)g_shader_time;
        e.p[2] = param(m, "distortion_strength", 1.0f);
        CurveData::from(shader_param_resource(m, "erosion_curve"), g_effect_curve);
        e.p[3] = g_effect_curve.valid() ? g_effect_curve.sample(0) : 0.0f;
        e.p[4] = param(m, "erosion_offset", 0.25f);
        e.p[5] = param(m, "bright_erosion_offset", 0.35f);
        e.tex[0] = effect_texture(c, m, "lut");
        effect_particles(EFFECT_CARD_REMOVE);
        g_life_param[0] = 3;
        g_cz_param[0] = 0;
        g_cz_scale[0] = 1;
    } else if (s == "shaders/vfx/minion_divebomb/vfx_minion_divebomb_rings_shader.gdshader") {
        e.id = EFFECT_DIVEBOMB_RINGS;
        CurveData::from(shader_param_resource(m, "erosion_curve"), g_effect_curve);
        e.p[0] = g_effect_curve.valid() ? g_effect_curve.sample(0) : 0.0f;
        e.p[1] = param(m, "erosion_offset", 0.25f);
        e.tex[0] = effect_texture(c, m, "lut");
        effect_particles(EFFECT_DIVEBOMB_RINGS);
        g_life_param[0] = 0;
    } else if (s == "shaders/vfx/hyperbeam/vfx_hyperbeam_laser_line.gdshader") {
        e.id = EFFECT_HYPERBEAM_LINE;
        e.p[0] = pan(param(m, "texture_panning_speed", 0.0f));
        e.tex[0] = effect_texture(c, m, "main_texture");
        e.tex[1] = effect_texture(c, m, "overlay");
        e.tex[2] = effect_texture(c, m, "lut");
        e.repeat = 1;
    } else if (s == "shaders/vfx/hyperbeam/vfx_hyperbeam_impact_spikes_shader.gdshader") {
        Vec2 tl = shader_param_vec2(m, "texture_tiling", Vec2(1, 1));
        e.id = EFFECT_HYPERBEAM_SPIKES;
        e.p[0] = tl.x;
        e.p[1] = tl.y;
        CurveData::from(shader_param_resource(m, "offset_curve"), g_effect_curve);
        e.p[3] = g_effect_curve.valid() ? g_effect_curve.sample(0) : 0.0f;
        e.p[4] = param(m, "initial_offset", 0.0f);
        e.tex[0] = effect_texture(c, m, "mask_texture");
        effect_particles(EFFECT_HYPERBEAM_SPIKES);
        g_life_param[0] = 3;
        g_cz_param[0] = 2;
        g_cz_scale[0] = 1;
    } else if (s == "shaders/vfx/grand_finale/vfx_grand_finale_spotlight_add_shader.gdshader" ||
               s == "shaders/vfx/grand_finale/vfx_grand_finale_spotlight_darken_shader.gdshader") {
        Vec2 sp = shader_param_vec2(m, "source_pos", Vec2(0, 0)), ap = shader_param_vec2(m, "angle_parameters", Vec2(0, 0));
        Vec2 er = shader_param_vec2(m, "erosion", Vec2(0, 0));
        e.id = EFFECT_SPOTLIGHT;
        e.p[0] = sp.x;
        e.p[1] = sp.y;
        e.p[2] = param(m, "angle_offset", 0.0f);
        e.p[3] = param(m, "invert", 0.0f);
        e.p[4] = ap.x;
        e.p[5] = ap.y;
        e.p[6] = er.x;
        e.p[7] = er.y;
    } else if (s == "shaders/vfx/monsters/aeonglass_ray_shader.tres") {
        Color core = param_color(m, "CoreColor", Color(0, 0, 0, 1)), blur = param_color(m, "BlurColor", Color(0, 0, 0, 1));
        Vec2 sc = shader_param_vec2(m, "EdgeMaskScale", Vec2(1, 1));
        e.id = EFFECT_AEONGLASS_RAY;
        e.p[0] = param(m, "Progress", 0.0f);
        e.p[1] = core.r;
        e.p[2] = core.g;
        e.p[3] = core.b;
        e.p[4] = blur.r;
        e.p[5] = blur.g;
        e.p[6] = blur.b;
        e.p[7] = sc.x;
        e.p[8] = sc.y;
        e.tex[0] = effect_texture(c, m, "RayTex");
        e.tex[1] = effect_texture(c, m, "EdgeMaskTex");
    } else if (s == "shaders/vfx/monsters/soul_fysh_soundwave_shader.tres") {
        Color k = param_color(m, "ColorParameter", Color(1, 0.682353f, 0.039216f, 1));
        Vec2 st = shader_param_vec2(m, "step", Vec2(0.4f, 0.6f));
        e.id = EFFECT_SOUL_FYSH_WAVE;
        e.p[0] = fmodf((param(m, "startingOffset", 0.0f) + (float)g_shader_time) * param(m, "ScrollSpeed", 0.8f), 1.0f);
        e.p[1] = param(m, "amount", 1.0f);
        e.p[2] = st.x;
        e.p[3] = st.y;
        e.p[4] = k.r;
        e.p[5] = k.g;
        e.p[6] = k.b;
        e.tex[0] = effect_texture(c, m, "edgeMask");
        e.tex[1] = effect_texture(c, m, "soundwaveTex");
        e.repeat = 3;
    } else if (s == "shaders/vfx/monsters/waterfall_giant_water_shader.tres") {
        Color k = param_color(m, "ColorParameter", Color(1, 0.682353f, 0.039216f, 1));
        Vec2 off = shader_param_vec2(m, "movingTexOffset", Vec2(0, 0)), sc = shader_param_vec2(m, "movingTexScale", Vec2(0, 0));
        Vec2 sp = shader_param_vec2(m, "scrollSpeed", Vec2(0, 0));
        e.id = EFFECT_WATERFALL_WATER;
        e.p[0] = off.x;
        e.p[1] = off.y;
        e.p[2] = sc.x;
        e.p[3] = sc.y;
        e.p[4] = pan(sp.x);
        e.p[5] = pan(sp.y);
        e.p[6] = k.r;
        e.p[7] = k.g;
        e.p[8] = k.b;
        e.tex[0] = effect_texture(c, m, "movingTex");
        e.tex[1] = effect_texture(c, m, "waterTex");
        e.repeat = 3;
    } else if (s == "shaders/colorize_addblend.tres") {
        Color k = param_color(m, "Color", Color(1, 1, 1, 1));
        e.id = EFFECT_COLORIZE_ADD;
        e.p[0] = k.r;
        e.p[1] = k.g;
        e.p[2] = k.b;
        e.p[3] = param(m, "MultiplyVsAdd", 1.0f) != 0.0f ? 1.0f : 0.0f;
    } else if (s == "shaders/vfx/ui/vfx_ui_epoch_unlock_chains_shader.gdshader") {
        Color k = param_color(m, "bright_color", Color(0, 0, 0, 0));
        e.id = EFFECT_EPOCH_CHAINS;
        e.p[0] = k.r;
        e.p[1] = k.g;
        e.p[2] = k.b;
        e.p[3] = k.a;
        e.p[4] = param(m, "bright_enabled", 0.0f);
        e.p[5] = param(m, "erosion_enabled", 0.0f);
        e.p[6] = param(m, "erosion_base", 0.0f);
        e.p[7] = param(m, "erosion_offset", 1.0f);
        e.tex[0] = effect_texture(c, m, "erosion_texture");
    } else if (s == "shaders/vfx/poison/vfx_poison_skull_shader.gdshader") {
        e.id = EFFECT_POISON_SKULL;
        Vec2 sp = shader_param_vec2(m, "distortion_panning_speed", Vec2(0, 0));
        e.p[0] = sp.x * (float)g_shader_time;
        e.p[1] = sp.y * (float)g_shader_time;
        e.p[2] = param(m, "distortion_strength", 0.0f);
        CurveData::from(shader_param_resource(m, "erosion_curve"), g_effect_curve);
        e.p[3] = g_effect_curve.valid() ? g_effect_curve.sample(0) : 0.0f;
        e.p[4] = param(m, "erosion_offset", 0.25f);
        e.tex[0] = effect_texture(c, m, "lut");
        /* the erosion at each particle's life (INSTANCE_CUSTOM.y / .w) */
        effect_particles(EFFECT_POISON_SKULL);
        g_life_param[0] = 3;
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
        e.tex[0] = effect_texture(c, m, "lut");
        e.tex[1] = effect_texture(c, m, "mask");
        /* the offset: CUSTOM.z * initial_offset_multiplier / main_st.xy */
        effect_particles(EFFECT_PANNING);
        g_life_param[0] = 4;
        g_cz_param[0] = 2;
        g_cz_scale[0] = st[0] != 0 ? k.x / st[0] : 0.0f;
        g_cz_param[1] = 3;
        g_cz_scale[1] = st[1] != 0 ? k.y / st[1] : 0.0f;
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
        effect_particles(EFFECT_EXHAUST_QUICK);
        g_life_param[0] = 0;
    } else if (s == "shaders/boss_map_point_placeholder.gdshader") {
        e.id = EFFECT_TINT_GRAY;
        Color k = param_color(m, "tint_color", Color(1, 1, 1, 1));
        e.p[0] = k.r;
        e.p[1] = k.g;
        e.p[2] = k.b;
    } else if (s == "shaders/enchantment_tab_appear.gdshader") {
        e.id = EFFECT_TAB_APPEAR;
        e.p[0] = param(m, "progress", 0.0f);
    } else if (s == "shaders/vfx/ui/vfx_ui_low_hp_border_shader.gdshader") {
        e.id = EFFECT_LOW_HP_BORDER;
        e.p[0] = param(m, "alpha", 1.0f);
        e.p[1] = clampf(param(m, "alpha_multiplier", 1.0f), 0, 1);
        e.p[2] = param(m, "inner_radius", 0.0f);
        e.p[3] = param(m, "outer_radius", 1.0f);
        Vec2 tiling = shader_param_vec2(m, "noise_tiling", Vec2(1, 1)), panning = shader_param_vec2(m, "noise_panning", Vec2(0, 0));
        e.p[4] = tiling.x;
        e.p[5] = tiling.y;
        e.p[6] = pan(panning.x) + param(m, "noise_additional_offset", 0.0f);
        e.p[7] = pan(panning.y);
        Vec2 sf = shader_param_vec2(m, "smoothstep_factors", Vec2(0, 1));
        e.p[8] = clampf(sf.x, 0, 1);
        e.p[9] = clampf(sf.x + sf.y, 0, 1);
        Color k = param_color(m, "main_color", Color(0, 0, 0, 1));
        e.p[10] = k.r;
        e.p[11] = k.g;
        e.p[12] = k.b;
        e.tex[0] = effect_texture(c, m, "noise");
        e.repeat = 1;
    } else if (s == "shaders/vfx/sleeping/vfx_sleeping_z_shader.gdshader") {
        e.id = EFFECT_SLEEPING_Z;
        CurveData::from(shader_param_resource(m, "erosion_curve"), g_effect_curve);
        CurveData::from(shader_param_resource(m, "distortion_curve"), g_effect_curve2);
        float strength = param(m, "distortion_strength", 0.0f);
        e.p[1] = param(m, "erosion_offset", 0.25f);
        Vec2 speed = shader_param_vec2(m, "distortion_speed", Vec2(0, 0));
        e.p[3] = pan(speed.x);
        e.p[4] = pan(speed.y);
        Color k = param_color(m, "outline_color", Color(0, 0, 0, 1));
        e.p[6] = k.r;
        e.p[7] = k.g;
        e.p[8] = k.b;
        e.p[9] = k.a;
        e.tex[0] = effect_texture(c, m, "animation_texture");
        e.repeat = 1;
        effect_particles(EFFECT_SLEEPING_Z);
        g_life_param[0] = 0;
        g_life_param[1] = 2;
        g_life_scale[1] = strength;
        g_cz_param[0] = 3;
        g_cz_param[1] = 4;
        g_cz_param[2] = 5;
        g_cz_scale[0] = g_cz_scale[1] = g_cz_scale[2] = 1;
        e.p[0] = g_effect_curve.valid() ? g_effect_curve.sample(0) : 0.0f;
        e.p[2] = strength * (g_effect_curve2.valid() ? g_effect_curve2.sample(0) : 0.0f);
    } else if (s == "shaders/vfx/heavy_blunt/vfx_heavy_blunt_erosion_shader.gdshader" ||
               s == "shaders/vfx/potion/vfx_potion_splash_shader.gdshader") {
        bool splash = s == "shaders/vfx/potion/vfx_potion_splash_shader.gdshader";
        e.id = splash ? EFFECT_POTION_SPLASH : EFFECT_LUT_EROSION;
        CurveData::from(shader_param_resource(m, "erosion_curve"), g_effect_curve);
        e.p[0] = g_effect_curve.valid() ? g_effect_curve.sample(0) : 0.0f;
        e.p[1] = param(m, "erosion_offset", splash ? 0.25f : 0.1f);
        if (splash) {
            Vec2 speed = shader_param_vec2(m, "distortion_panning_speed", Vec2(0, 0));
            e.p[2] = pan(speed.x);
            e.p[3] = pan(speed.y);
            e.p[4] = param(m, "distortion_strength", 0.0f);
        }
        e.tex[0] = effect_texture(c, m, "lut");
        effect_particles(e.id);
        g_life_param[0] = 0;
    } else if (s == "shaders/vfx/goopy_impact/vfx_goopy_impact_bits_shader.gdshader") {
        e.id = EFFECT_GOOPY_BITS;
        CurveData::from(shader_param_resource(m, "erosion_curve"), g_effect_curve);
        e.p[0] = g_effect_curve.valid() ? g_effect_curve.sample(0) : 0.0f;
        e.p[1] = param(m, "erosion_offset", 0.25f);
        Vec2 speed = shader_param_vec2(m, "distortion_panning_speed", Vec2(0, 0));
        e.p[2] = pan(speed.x);
        e.p[3] = pan(speed.y);
        e.p[4] = param(m, "distortion_strength", 0.0f);
        Vec2 size = shader_param_vec2(m, "flipbook_size", Vec2(2, 2));
        e.p[6] = size.x < 1 ? 1 : size.x;
        e.p[7] = size.y < 1 ? 1 : size.y;
        e.tex[0] = effect_texture(c, m, "lut");
        effect_particles(e.id);
        g_life_param[0] = 0;
        g_cz_param[0] = 5;
        g_cz_scale[0] = param(m, "frame_count", 4.0f) * 0.99f;
    } else if (s == "shaders/vfx/ui/card/afflictions/bound/vfx_ui_card_affliction_bound_main_shader.gdshader") {
        e.id = EFFECT_BOUND_MAIN;
        static const float st0[4] = { 0, 0, 0, 0 };
        float st[4];
        param_vec4(m, "bright_st", st0, st);
        e.p[0] = st[0];
        e.p[1] = st[1];
        e.p[2] = pan(st[2]);
        e.p[3] = pan(st[3]);
        e.p[4] = param(m, "bright_energy", 0.0f);
    } else if (s == "shaders/vfx/ui/card/afflictions/bound/vfx_ui_card_affliction_bound_border_shader.gdshader") {
        e.id = EFFECT_POLAR_BORDER;
        static const float st0[4] = { 0, 0, 0, 0 };
        float st[4];
        param_vec4(m, "main_st", st0, st);
        e.p[0] = st[0];
        e.p[1] = st[1];
        e.p[2] = pan(st[2]);
        e.p[3] = pan(st[3]);
        Vec2 me = shader_param_vec2(m, "mask_erosion", Vec2(0, 0));
        e.p[4] = me.x;
        e.p[5] = me.y;
        e.tex[0] = effect_texture(c, m, "mask");
    } else if (s == "shaders/vfx/scream/vfx_scream_squiggly_ring_polar_shader.gdshader") {
        e.id = EFFECT_SQUIGGLY_RING;
        CurveData::from(shader_param_resource(m, "erosion_curve"), g_effect_curve);
        CurveData::from(shader_param_resource(m, "offset_curve"), g_effect_curve2);
        Vec2 xo = shader_param_vec2(m, "x_offsets", Vec2(0, 0));
        e.p[0] = g_effect_curve.valid() ? g_effect_curve.sample(0) : 0.0f;
        e.p[1] = param(m, "erosion_offset", 0.0f);
        Vec2 dt = shader_param_vec2(m, "distortion_tiling", Vec2(0, 0));
        Vec2 ds = shader_param_vec2(m, "distortion_offset_speed", Vec2(0, 0));
        e.p[2] = dt.x;
        e.p[3] = dt.y;
        e.p[4] = pan(ds.x);
        e.p[5] = pan(ds.y);
        e.p[7] = param(m, "distortion_strength", 0.05f);
        Vec2 tl = shader_param_vec2(m, "tiling", Vec2(0, 0));
        e.p[8] = tl.x;
        e.p[9] = tl.y;
        e.p[10] = xo.x;
        e.p[11] = (xo.y - xo.x) * (g_effect_curve2.valid() ? g_effect_curve2.sample(0) : 0.0f);
        effect_particles(e.id);
        g_life_param[0] = 0;
        g_life_param[1] = 11;
        g_life_scale[1] = xo.y - xo.x;
        g_cx_param = 6;
    } else if (s == "shaders/vfx/fire/fire_shader_1.tres") {
        e.id = EFFECT_FIRE_SHAPE;
        float t = (float)g_shader_time + param(m, "TimeOffset", 0.0f);
        Vec2 s1 = shader_param_vec2(m, "Noise_Scroll_1", Vec2(0.2f, 0.2f));
        Vec2 s2 = shader_param_vec2(m, "Noise_Scroll_2", Vec2(0.2f, 0.2f));
        Vec2 ms = shader_param_vec2(m, "MaskNoiseScroll", Vec2(0, 0));
        Vec2 ss = shader_param_vec2(m, "Smooth_Step", Vec2(0.48f, 0.5f));
        e.p[0] = fmodf(t * s1.x, 1.0f);
        e.p[1] = fmodf(t * s1.y, 1.0f);
        e.p[2] = param(m, "NoiseScale1", 1.0f);
        e.p[3] = fmodf(t * s2.x * 3.0f, 1.0f);
        e.p[4] = fmodf(t * s2.y * 3.0f, 1.0f);
        e.p[5] = param(m, "NoiseScale2", 1.0f);
        e.p[6] = 1.0f - param(m, "Noise_Intensity_1", 1.0f);
        e.p[7] = 1.0f - param(m, "Noise_Intensity_2", 1.0f);
        e.p[8] = pan(ms.x);
        e.p[9] = pan(ms.y);
        e.p[10] = param(m, "MaskNoiseIntensity", 0.0f);
        e.p[11] = ss.x;
        e.p[12] = ss.y;
        Color k = param_color(m, "ColorParameter", Color(0, 0, 0, 1));
        e.p[13] = k.r;
        e.p[14] = k.g;
        e.p[15] = k.b;
        e.tex[0] = effect_texture(c, m, "NoiseTexture1");
        e.tex[1] = effect_texture(c, m, "NoiseTexture2");
        e.tex[2] = effect_texture(c, m, "top_gradient");
        e.tex[3] = effect_texture(c, m, "MaskNoiseTexture");
        e.tex[4] = effect_texture(c, m, "Shape_Mask");
        e.repeat = 1 | 2 | 8;
    } else if (s == "shaders/vfx/simple_distortion_flat_shader.tres") {
        e.id = EFFECT_SIMPLE_DISTORTION;
        Vec2 pn = shader_param_vec2(m, "distortionPanning1", Vec2(0.2f, 0.2f));
        Vec2 amt = shader_param_vec2(m, "distortionAmount", Vec2(0, 0));
        e.p[0] = pan(pn.x);
        e.p[1] = pan(pn.y);
        e.p[2] = amt.x;
        e.p[3] = amt.y;
        Color k = param_color(m, "Color_Multiply", Color(1, 1, 1, 1));
        e.p[4] = k.r;
        e.p[5] = k.g;
        e.p[6] = k.b;
        e.tex[0] = effect_texture(c, m, "distortionTexture1");
        e.repeat = 1;
    } else if (s == "shaders/vfx/ui/card/afflictions/entangled/vfx_ui_card_affliction_entangled_main_shader.gdshader") {
        e.id = EFFECT_ENTANGLED;
        static const float st0[4] = { 0, 0, 0, 0 };
        float st[4];
        param_vec4(m, "distortion_st", st0, st);
        e.p[0] = st[0];
        e.p[1] = st[1];
        e.p[2] = st[2];
        e.p[3] = pan(st[3]);
        e.p[4] = param(m, "distortion_intensity", 0.0f);
        e.tex[0] = effect_texture(c, m, "vine_lut");
    } else if (s == "shaders/vfx/2d_sphere_shader.tres") {
        e.id = EFFECT_SPHERE;
        Vec2 nps = shader_param_vec2(m, "NoisePanSpeed", Vec2(0, 0)), ns = shader_param_vec2(m, "NoiseScale", Vec2(10, 10));
        Vec2 tps = shader_param_vec2(m, "TexturePanSpeed", Vec2(1, 1)), to = shader_param_vec2(m, "TextureOffset", Vec2(0, 0));
        e.p[0] = pan(nps.x * ns.x);
        e.p[1] = pan(nps.y * ns.y);
        e.p[2] = ns.x;
        e.p[3] = ns.y;
        e.p[4] = param(m, "NoiseStrength", 0.01f);
        /* TIME * TexturePanSpeed + 1 + TextureOffset (TEXTURE repeats) */
        e.p[5] = pan(tps.x) + to.x;
        e.p[6] = pan(tps.y) + to.y;
        Color k = param_color(m, "AddColor", Color(0, 0.483333f, 1, 1));
        e.p[7] = k.r;
        e.p[8] = k.g;
        e.p[9] = k.b;
        Vec2 sm = shader_param_vec2(m, "SidesMaskStep", Vec2(0.1f, 0.3f)), tb = shader_param_vec2(m, "TopBottomMaskStep", Vec2(0.5f, 0.7f));
        Vec2 tbo = shader_param_vec2(m, "TopBottomMaskOffset", Vec2(0, 0));
        e.p[10] = sm.x;
        e.p[11] = sm.y;
        e.p[12] = tb.x;
        e.p[13] = tb.y;
        e.p[14] = tbo.x;
        e.p[15] = tbo.y;
        e.tex[0] = effect_texture(c, m, "NoiseTexture");
        e.tex[1] = effect_texture(c, m, "tex_frg_30");
        e.tex[2] = effect_texture(c, m, "Sides_Mask");
        e.tex[3] = effect_texture(c, m, "TopBottomMask");
        e.repeat = 1;
    } else if (s == "shaders/vfx/scream/vfx_scream_ring_polar_shader.gdshader") {
        e.id = EFFECT_SCREAM_RING;
        CurveData::from(shader_param_resource(m, "offset_curve"), g_effect_curve);
        Vec2 tl = shader_param_vec2(m, "tiling", Vec2(0, 0)), xo = shader_param_vec2(m, "x_offsets", Vec2(0, 0));
        e.p[0] = tl.x;
        e.p[1] = tl.y;
        e.p[2] = xo.x;
        e.p[3] = (xo.y - xo.x) * (g_effect_curve.valid() ? g_effect_curve.sample(0) : 0.0f);
        effect_particles(e.id);
        g_life_param[0] = 3;
        g_life_scale[0] = xo.y - xo.x;
    } else if (s == "shaders/vfx/ui/card/afflictions/hexed/vfx_ui_card_affliction_hexed_main_shader.gdshader") {
        e.id = EFFECT_HEXED;
        static const float st0[4] = { 1, 1, 0, 0 };
        float st[4];
        param_vec4(m, "noise_st", st0, st);
        e.p[0] = st[0];
        e.p[1] = st[1];
        e.p[2] = pan(st[2]);
        e.p[3] = pan(st[3]);
        Vec2 ns = shader_param_vec2(m, "noise_smoothstep", Vec2(0, 1));
        e.p[4] = ns.x;
        e.p[5] = ns.y;
        static const char* const names[3] = { "main_color", "bright_color", "border_color" };
        for (int i = 0; i < 3; ++i) {
            Color k = param_color(m, names[i], Color(0, 0, 0, 1));
            e.p[6 + i * 3] = k.r;
            e.p[7 + i * 3] = k.g;
            e.p[8 + i * 3] = k.b;
        }
    } else if (s == "shaders/vfx/ui/card/afflictions/ringing/vfx_ui_card_affliction_ringing_main_shader.gdshader") {
        e.id = EFFECT_RINGING;
        e.p[0] = pan(param(m, "animation_speed", 1.0f));
        e.tex[0] = effect_texture(c, m, "ring_base_alpha");
        e.tex[1] = effect_texture(c, m, "thin_ring_animation");
    } else if (s == "shaders/vfx/missile/vfx_missile_core_shader.gdshader") {
        e.id = EFFECT_LUT_FLIPBOOK;
        float frames = param(m, "frame_count", 9.0f);
        Vec2 size = shader_param_vec2(m, "flipbook_size", Vec2(3, 3));
        e.p[0] = frames > 0 ? fmodf((float)g_shader_time * param(m, "flipbook_speed", 1.0f), frames) : 0.0f;
        e.p[1] = size.x < 1 ? 1 : size.x;
        e.p[2] = size.y < 1 ? 1 : size.y;
        e.tex[0] = effect_texture(c, m, "lut");
    } else if (s == "shaders/vfx/vfx_noisy_dissolve_shader.tres") {
        e.id = EFFECT_NOISY_DISSOLVE;
        Vec2 pn = shader_param_vec2(m, "NoisePanning", Vec2(0, 0));
        e.p[0] = param(m, "step", 0.0f);
        e.p[1] = param(m, "step_spread", 0.1f);
        e.p[2] = pan(pn.x);
        e.p[3] = pan(pn.y);
        e.tex[0] = effect_texture(c, m, "NoiseTexture");
        e.repeat = 1;
    } else if (s == "shaders/vfx/ui/card/afflictions/smog/vfx_ui_card_affliction_smog_main_shader.gdshader") {
        /* tiling_and_offset(uv, (st.xyz, 0), (0, st.w)) */
        e.id = EFFECT_SMOG;
        static const float st0[4] = { 0, 0, 0, 0 };
        float a[4], b[4];
        param_vec4(m, "main_st", st0, a);
        param_vec4(m, "secondary_st", st0, b);
        e.p[0] = a[0];
        e.p[1] = a[1];
        e.p[2] = pan(a[2]);
        e.p[3] = a[3];
        e.p[4] = b[0];
        e.p[5] = b[1];
        e.p[6] = pan(b[2]);
        e.p[7] = b[3];
        e.p[8] = param(m, "secondary_smoke_influence", 0.0f);
        Vec2 ef = shader_param_vec2(m, "erosion_factors", Vec2(0, 0));
        e.p[9] = ef.x;
        e.p[10] = ef.y;
        e.tex[0] = effect_texture(c, m, "lut");
        e.tex[1] = effect_texture(c, m, "mask");
    } else if (s == "shaders/vfx/ui/card/afflictions/entangled/vfx_ui_card_affliction_entangled_leaf_shader.gdshader") {
        e.id = EFFECT_LEAF;
        static const float st0[4] = { 0, 0, 0, 0 };
        float st[4];
        param_vec4(m, "rotation_st", st0, st);
        e.p[0] = st[0];
        e.p[1] = st[1];
        e.p[3] = pan(st[3]);
        e.p[4] = param(m, "rotation_range", 0.0f);
        Vec2 ro = shader_param_vec2(m, "rotation_offset", Vec2(0, 0));
        e.p[5] = ro.x;
        e.p[6] = ro.y;
        e.tex[0] = effect_texture(c, m, "lut");
        effect_particles(e.id);
        g_cz_param[0] = 2;
        g_cz_scale[0] = 1;
    } else if (s == "scenes/backgrounds/vantom_boss/oil.gdshader") {
        e.id = EFFECT_OIL;
        float cur = param(m, "current_mana", 0.5f), mx = param(m, "max_mana", 1.0f);
        float rank = param(m, "rank_advance_mana", 1.0f);
        float ratio = (rank != 0 ? minf(1.0f, mx / rank) : 1.0f) * param(m, "inset", 0.8f);
        float gs = param(m, "groove_speed", 0.1f);
        e.p[0] = ratio;
        e.p[1] = mx != 0 ? cur / mx * ratio * 1.05f : 0.0f;
        e.p[2] = pan(param(m, "border_speed", 0.02f));
        e.p[3] = pan(gs);
        e.p[4] = sinf((float)g_shader_time * gs);
        e.p[5] = param(m, "groove_distance", 0.3f);
        e.p[6] = param(m, "groove_width", 0.075f);
        e.p[7] = param(m, "groove_opacity", 0.5f);
        e.p[8] = param(m, "border_width", 0.1f);
        e.p[9] = param(m, "glow_strength", 0.5f);
        Color bc = param_color(m, "border_color", Color(0, 0, 0, 0)), mc = param_color(m, "mana_color", Color(0, 0, 0, 0));
        e.p[10] = bc.r;
        e.p[11] = bc.g;
        e.p[12] = bc.b;
        e.p[13] = bc.a;
        float q[4] = { mc.r, mc.g, mc.b, mc.a };
        for (int i = 0; i < 4; ++i) q[i] = floorf(clampf(q[i], 0, 1) * 255.0f + 0.5f);
        e.p[14] = q[0] * 256.0f + q[1];
        e.p[15] = q[2] * 256.0f + q[3] + (cur > 0 ? 65536.0f : 0.0f);
        e.tex[0] = effect_texture(c, m, "noise");
        e.tex[1] = effect_texture(c, m, "groovy_noise");
    } else if (s == "shaders/vfx/ui/card/afflictions/galvanized/vfx_ui_card_affliction_galvanized_main_shader.gdshader") {
        e.id = EFFECT_GALVANIZED;
        static const float st0[4] = { 0, 0, 0, 0 };
        float b[4], d[4];
        param_vec4(m, "blink_st", st0, b);
        param_vec4(m, "distortion_st", st0, d);
        e.p[0] = b[0];
        e.p[1] = pan(b[2]);
        e.p[2] = d[0];
        e.p[3] = d[1];
        e.p[4] = pan(d[2]);
        e.p[5] = pan(d[3]);
        e.p[6] = param(m, "distortion_offset", 0.0f);
        e.p[7] = param(m, "distortion_intensity", 0.0f);
        Vec2 bs = shader_param_vec2(m, "blink_smoothstep", Vec2(0, 0)), sf = shader_param_vec2(m, "smoothstep_factors", Vec2(0, 0));
        e.p[8] = bs.x;
        e.p[9] = bs.y;
        e.p[10] = sf.x;
        e.p[11] = sf.y;
        e.tex[0] = effect_texture(c, m, "lut");
    } else if (s == "shaders/vfx/sweeping_beam/vfx_sweeping_beam_flipbook_shader.gdshader") {
        /* vertex(): the quad tilted in 3D (rot_y, rot_x) and seen through
         * fov; world = R * (UV - 0.5, 0.5 / tan(fov / 2)), its xy times
         * depth_scale * R[2].z; offset = depth_scale * R[2].xy. */
        e.id = EFFECT_SWEEP_FLIPBOOK;
        float ry = param(m, "rot_y_deg", 0.0f) * 0.017453293f, rx = param(m, "rot_x_deg", 0.0f) * 0.017453293f;
        float sy = sinf(ry), cy = cosf(ry), sx = sinf(rx), cx = cosf(rx);
        float ps = tanf(param(m, "fov", 90.0f) * 0.017453293f * 0.5f);
        float zc = ps != 0 ? 0.5f / ps : 0.0f, depth = zc + 0.5f;
        float col[3][3] = { { cy, 0, -sy }, { sy * sx, cx, cy * sx }, { sy * cx, -sx, cy * cx } };
        for (int k = 0; k < 3; ++k) {
            e.p[k] = col[0][k];
            e.p[3 + k] = col[1][k];
            e.p[6 + k] = col[2][k] * zc;
        }
        e.p[9] = depth * col[2][2];
        e.p[10] = depth * col[2][0];
        e.p[11] = depth * col[2][1];
        CurveData::from(shader_param_resource(m, "flipbook_curve"), g_effect_curve);
        float frames = param(m, "frame_count", 9.0f);
        e.p[12] = frames * 0.99f * (g_effect_curve.valid() ? g_effect_curve.sample(0) : 0.0f);
        Vec2 size = shader_param_vec2(m, "flipbook_size", Vec2(3, 3));
        e.p[13] = size.x < 1 ? 1 : size.x;
        e.p[14] = size.y < 1 ? 1 : size.y;
        e.tex[0] = effect_texture(c, m, "lut");
        e.tex[1] = effect_texture(c, m, "mask_texture");
        effect_particles(e.id);
        g_life_param[0] = 12;
        g_life_scale[0] = frames * 0.99f;
    } else if (s == "shaders/crystal_sphere_item.gdshader") {
        e.id = EFFECT_SHINE;
        e.p[0] = param(m, "val", 1.0f);
    } else if (s == "shaders/scry_reveal.gdshader") {
        e.id = EFFECT_SCRY;
        e.p[0] = param(m, "circles", 8.0f);
        e.p[1] = param(m, "outerCircle", -1.0f);
        e.p[2] = param(m, "uvMargin", 0.0f);
        Color b = param_color(m, "borderColor", Color(0, 0, 0, 1));
        e.p[3] = b.r;
        e.p[4] = b.g;
        e.p[5] = b.b;
        e.p[6] = fmodf((float)g_shader_time * 0.0065f, 1.0f);
        e.p[7] = fmodf((float)g_shader_time * 0.0011f, 1.0f);
        /* colors[8] (and the ninth the last circle reads: zero), then
         * circleData[8] */
        const Json& cols = (*m->props)["shader_parameter/colors"]["v"];
        const Json& data = (*m->props)["shader_parameter/circleData"]["v"];
        for (int k = 0; k < 8; ++k)
            for (int q = 0; q < 3; ++q) {
                size_t at = (size_t)(k * 4 + q);
                e.extra[k * 4 + q] = at < cols.size() ? jv_float(cols[at]) : 0.0f;
                e.extra[(9 + k) * 4 + q] = at < data.size() ? jv_float(data[at]) : 0.0f;
            }
        for (int k = 0; k < 8; ++k) {
            size_t at = (size_t)(k * 4 + 3);
            e.extra[(9 + k) * 4 + 3] = at < data.size() ? jv_float(data[at]) : 0.0f;
        }
        e.extra_count = 17;
        e.tex[0] = effect_texture(c, m, "noiseTex1");
        e.tex[1] = effect_texture(c, m, "noiseTex2");
        e.tex[2] = scry_cells(c, g_item);
        e.repeat = 1 | 2;
    } else if (s == "shaders/blur/Blur.gdshader") {
        /* textureSize(TEXTURE): the drawn TextureRect's */
        Vec2 ts(512, 512);
        if (g_item && g_item->type == "TextureRect" && ((const TextureRect*)g_item)->texture)
            ts = ((const TextureRect*)g_item)->texture->size();
        e.id = EFFECT_PORTRAIT_BLUR;
        e.p[0] = param(m, "radius", 32.0f) / 32.0f;
        e.p[1] = ts.x > 0 ? 1.0f / ts.x : 0.0f;
        e.p[2] = ts.y > 0 ? 1.0f / ts.y : 0.0f;
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
    /* a ShaderMaterial without a shader draws as the default canvas item */
    if (m->shader.empty()) return true;
    g_item = item;
    if (m->shader == "shaders/hsv.gdshader" || m->shader == "inline:aa241422154f3376") {
        /* inline:aa24...: the FTUE screens' (and the card library's) copy */
        float cm[12];
        Canvas::hsv_matrix(param(m, "h", 1.0f), param(m, "s", 1.0f), param(m, "v", 1.0f), cm);
        c.set_color_matrix(cm);
        return true;
    }
    if (m->shader == "shaders/vfx/wind_sway.gdshader" || m->shader == "inline:c4e318138159a7c6") {
        /* inline:c4e3...: the overgrowth rest site's copy */
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
    if (m->shader == "shaders/boss_map_point.gdshader") {
        /* COLOR's red through map_color, blue through black_layer_color,
         * green as white: a colour matrix of those columns. */
        Color mc = param_color(m, "map_color", Color(1, 1, 1, 1)), bl = param_color(m, "black_layer_color", Color(0, 0, 0, 1));
        float cm[12] = { mc.r, 1, bl.r, 0, mc.g, 1, bl.g, 0, mc.b, 1, bl.b, 0 };
        c.set_color_matrix(cm);
        return true;
    }
    if (m->shader == "shaders/vfx/monsters/amalgam_glare_shader.tres") {
        /* an empty fragment(): COLOR = texel * vertex colour, render_mode
         * blend_add (shader_blend) */
        return true;
    }
    if (m->shader == "shaders/vfx/common/vfx_common_subtractive.gdshader") {
        /* COLOR = texel * vertex colour, render_mode blend_sub (shader_blend) */
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
    if (m->shader == "shaders/vfx/vfx_water_reflection.gdshader" || m->shader == "inline:79431f9d3f8deb72") {
        /* inline:7943...: the fake merchant's copy */
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
    if (m->shader == "shaders/vfx/distortion/vfx_screen_distortion_outward_shader.gdshader" ||
        m->shader == "shaders/vfx/scream/vfx_scream_distortion_polar_shader.gdshader") {
        ScreenParams p;
        memset(&p, 0, sizeof(p));
        p.mode = SCREEN_DISTORT;
        p.strength = param(m, "distortion_base_intensity", 0.05f);
        if (m->shader == "shaders/vfx/scream/vfx_scream_distortion_polar_shader.gdshader") {
            /* tiling_and_offset: uv * tiling + offset_speed * TIME */
            Vec2 tiling = shader_param_vec2(m, "tiling", Vec2(1, 1)), speed = shader_param_vec2(m, "offset_speed", Vec2(0, 0));
            p.count = 1;
            p.center[0] = tiling.x;
            p.center[1] = tiling.y;
            p.noise_offset[0] = fmodf(speed.x * (float)g_shader_time, 1.0f);
            p.noise_offset[1] = fmodf(speed.y * (float)g_shader_time, 1.0f);
        }
        return c.renderer()->set_screen(&p);
    }
    if (m->shader == "shaders/vfx/common/vfx_screen_chromatic_aberration_shader.gdshader") {
        /* intensity = intensity_curve(life) * base_intensity (an instance
         * uniform) * distance_curve(dist) * TEXTURE.r */
        ScreenParams p;
        memset(&p, 0, sizeof(p));
        p.mode = SCREEN_CHROMA;
        p.strength = param(m, "base_intensity", 1.0f);
        p.noise = effect_texture(c, m, "distance_curve");
        CurveData::from(shader_param_resource(m, "intensity_curve"), g_screen_curve);
        g_screen_base = p;
        g_screen_particle = true;
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
        m->shader == "shaders/vfx/vfx_stepped_shader_fire_dark.tres" || m->shader == "inline:0e24d7d5bad2b589") {
        /* inline:0e24...: the flat one's copy (the waterfall giant's and
         * the insatiable's backgrounds). The stepped fire (FireParams; add: blend_add, see shader_blend). The
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
    if (m && m->kind == Resource::RES_SHADER_MATERIAL && (m->shader == "shaders/vfx/vfx_stepped_shader_fire_add.tres" ||
                                                         m->shader == "shaders/vfx/monsters/amalgam_glare_shader.tres" ||
                                                         m->shader == "shaders/vfx/2d_sphere_shader.tres")) {
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
    if (m && m->kind == Resource::RES_SHADER_MATERIAL && m->shader == "shaders/vfx/common/vfx_common_subtractive.gdshader") {
        *out = BLEND_SUB;
        return true;
    }
    if (m && m->kind == Resource::RES_SHADER_MATERIAL && m->shader == "shaders/dissolve.gdshader") {
        *out = BLEND_PREMULT_ALPHA;
        return true;
    }
    if (m && m->kind == Resource::RES_SHADER_MATERIAL && m->shader == "shaders/vfx/grand_finale/vfx_grand_finale_spotlight_add_shader.gdshader") {
        *out = BLEND_ADD;
        return true;
    }
    if (m && m->kind == Resource::RES_SHADER_MATERIAL && m->shader == "shaders/vfx/grand_finale/vfx_grand_finale_spotlight_darken_shader.gdshader") {
        *out = BLEND_SUB;
        return true;
    }
    if (m && m->kind == Resource::RES_SHADER_MATERIAL && m->shader == "shaders/colorize_addblend.tres") {
        *out = BLEND_ADD;
        return true;
    }
    if (m && m->kind == Resource::RES_SHADER_MATERIAL && m->shader == "shaders/vfx/vfx_doom_flame_shader.tres") {
        *out = BLEND_ADD;
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
        g_screen_particle = false;
    }
}

} /* namespace spire */

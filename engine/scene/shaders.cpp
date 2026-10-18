#include "shaders.h"

#include "canvas.h"
#include "node.h"
#include "particles.h"
#include "scene.h"
#include "value.h"

#include <math.h>
#include <map>

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
static void hsv_matrix(const MaterialRes* m, float out[12]) {
    float h = param(m, "h", 1.0f), s = param(m, "s", 1.0f), v = param(m, "v", 1.0f);
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
        hsv_matrix(m, cm);
        c.set_color_matrix(cm);
        return true;
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
        m->shader == "shaders/vfx/common/vfx_row_flipbook_shader.gdshader") {
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
    if (vfx_shader(c, m)) return true;
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
    return false;
}

void shader_end(Canvas& c, const MaterialRes* m) {
    if (m && m->kind == Resource::RES_SHADER_MATERIAL) {
        c.set_color_matrix(NULL);
        c.set_ripple(NULL);
        c.renderer()->set_gradient_map(NULL, false);
        c.renderer()->set_fire(NULL);
        c.renderer()->set_vfx(NULL);
        g_vfx_on = false;
    }
}

} /* namespace spire */

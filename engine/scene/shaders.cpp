#include "shaders.h"

#include "canvas.h"
#include "node.h"
#include "value.h"

#include <math.h>

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
    }
}

} /* namespace spire */

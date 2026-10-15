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

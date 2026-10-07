#include "value.h"

#include <float.h>
#include <string.h>

namespace spire {

float jv_float(const Json& v, float fallback) {
    if (v.is_number() || v.is_bool()) return v.f(fallback);
    if (v.is_object() && v.has("f")) {
        const std::string& s = v["f"].str();
        if (s == "inf") return FLT_MAX;
        if (s == "-inf") return -FLT_MAX;
        return 0.0f;
    }
    return fallback;
}

const char* jv_ctor(const Json& v) {
    if (!v.is_object() || !v.has("t")) return "";
    return v["t"].str().c_str();
}

static bool args(const Json& v, size_t n) {
    return v.is_object() && v.has("t") && v["v"].is_array() && v["v"].size() >= n;
}

bool jv_vec2(const Json& v, Vec2& out) {
    if (!args(v, 2)) return false;
    out = Vec2(jv_float(v["v"][(size_t)0]), jv_float(v["v"][1]));
    return true;
}

bool jv_color(const Json& v, Color& out) {
    if (!args(v, 3)) return false;
    const Json& a = v["v"];
    out = Color(jv_float(a[(size_t)0]), jv_float(a[1]), jv_float(a[2]), a.size() > 3 ? jv_float(a[3]) : 1.0f);
    return true;
}

bool jv_rect2(const Json& v, Rect2& out) {
    if (!args(v, 4)) return false;
    const Json& a = v["v"];
    out = Rect2(jv_float(a[(size_t)0]), jv_float(a[1]), jv_float(a[2]), jv_float(a[3]));
    return true;
}

bool jv_is_ext(const Json& v) { return v.is_object() && v.has("ext"); }
bool jv_is_sub(const Json& v) { return v.is_object() && v.has("sub"); }
bool jv_is_object(const Json& v) { return v.is_object() && v.has("class") && v.has("props"); }

std::string jv_nodepath(const Json& v) {
    if (v.is_object() && v.has("np")) return v["np"].str();
    return v.str();
}

} /* namespace spire */

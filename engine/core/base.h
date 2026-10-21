/* Spire360 engine: basic types. C++03 (the XDK compiler is VS2010; see
 * docs/CODING_RULES.md): no auto, lambdas, range-for, nullptr or enum class. */
#ifndef SPIRE_BASE_H
#define SPIRE_BASE_H

#include <math.h>
#include <stddef.h>
#include <string>
#include <vector>

namespace spire {

typedef unsigned char u8;
typedef unsigned short u16;
typedef unsigned int u32;
typedef signed int i32;
#if defined(_MSC_VER)
typedef unsigned __int64 u64;
typedef __int64 i64;
#else
typedef unsigned long long u64;
typedef long long i64;
#endif

inline float clampf(float v, float lo, float hi) { return v < lo ? lo : (v > hi ? hi : v); }
inline float minf(float a, float b) { return a < b ? a : b; }
inline float maxf(float a, float b) { return a > b ? a : b; }

struct Vec2 {
    float x, y;
    Vec2() : x(0), y(0) {}
    Vec2(float x_, float y_) : x(x_), y(y_) {}
    Vec2 operator+(const Vec2& o) const { return Vec2(x + o.x, y + o.y); }
    Vec2 operator-(const Vec2& o) const { return Vec2(x - o.x, y - o.y); }
    Vec2 operator*(float s) const { return Vec2(x * s, y * s); }
    Vec2 operator*(const Vec2& o) const { return Vec2(x * o.x, y * o.y); }
    Vec2 operator/(float s) const { return Vec2(x / s, y / s); }
    bool operator==(const Vec2& o) const { return x == o.x && y == o.y; }
    bool operator!=(const Vec2& o) const { return !(*this == o); }
};

struct Rect2 {
    Vec2 pos, size;
    Rect2() {}
    Rect2(float x, float y, float w, float h) : pos(x, y), size(w, h) {}
    Rect2(const Vec2& p, const Vec2& s) : pos(p), size(s) {}
    Vec2 end() const { return pos + size; }
    bool has_area() const { return size.x > 0 && size.y > 0; }
    Rect2 intersection(const Rect2& o) const {
        float x0 = maxf(pos.x, o.pos.x), y0 = maxf(pos.y, o.pos.y);
        float x1 = minf(pos.x + size.x, o.pos.x + o.size.x), y1 = minf(pos.y + size.y, o.pos.y + o.size.y);
        if (x1 < x0) x1 = x0;
        if (y1 < y0) y1 = y0;
        return Rect2(x0, y0, x1 - x0, y1 - y0);
    }
};

/* Straight (not premultiplied) RGBA, 0..1, as Godot's Color. */
struct Color {
    float r, g, b, a;
    Color() : r(1), g(1), b(1), a(1) {}
    Color(float r_, float g_, float b_, float a_ = 1.0f) : r(r_), g(g_), b(b_), a(a_) {}
    Color operator*(const Color& o) const { return Color(r * o.r, g * o.g, b * o.b, a * o.a); }
    bool operator==(const Color& o) const { return r == o.r && g == o.g && b == o.b && a == o.a; }
};

/* 2D affine transform, Godot's Transform2D layout: columns x, y and origin.
 * A point p maps to x * p.x + y * p.y + origin. */
struct Xform2D {
    Vec2 x, y, origin;
    Xform2D() : x(1, 0), y(0, 1), origin(0, 0) {}
    Xform2D(const Vec2& x_, const Vec2& y_, const Vec2& o) : x(x_), y(y_), origin(o) {}

    static Xform2D make(const Vec2& pos, float rotation, const Vec2& scale, float skew = 0.0f) {
        /* Godot: Transform2D(rotation, scale, skew, position). */
        Xform2D t;
        t.x = Vec2(cosf(rotation) * scale.x, sinf(rotation) * scale.x);
        t.y = Vec2(-sinf(rotation + skew) * scale.y, cosf(rotation + skew) * scale.y);
        t.origin = pos;
        return t;
    }
    Vec2 xform(const Vec2& p) const { return x * p.x + y * p.y + origin; }
    Vec2 basis_xform(const Vec2& v) const { return x * v.x + y * v.y; }
    Xform2D operator*(const Xform2D& o) const {
        /* this applied after o */
        return Xform2D(basis_xform(o.x), basis_xform(o.y), xform(o.origin));
    }
    /* Godot's Transform2D.affine_inverse (identity when singular) */
    Xform2D affine_inverse() const {
        float det = x.x * y.y - y.x * x.y;
        if (det == 0) return Xform2D();
        float id = 1.0f / det;
        Xform2D r(Vec2(y.y * id, -x.y * id), Vec2(-y.x * id, x.x * id), Vec2(0, 0));
        r.origin = r.basis_xform(origin) * -1.0f;
        return r;
    }
    static Xform2D translation(const Vec2& v) { return Xform2D(Vec2(1, 0), Vec2(0, 1), v); }
    static Xform2D scaling(float sx, float sy) { return Xform2D(Vec2(sx, 0), Vec2(0, sy), Vec2(0, 0)); }
};

std::string format(const char* fmt, ...);
void log_info(const char* fmt, ...);
void log_warn(const char* fmt, ...);
/* Where log lines go (one line, no newline); NULL: stderr. The console
 * apps send them to their log file. */
typedef void (*LogSink)(const char* line);
void set_log_sink(LogSink sink);

} /* namespace spire */

#endif

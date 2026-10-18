/**************************************************************************/
/*  line_builder.cpp                                                      */
/**************************************************************************/
/*                         This file is part of:                          */
/*                             GODOT ENGINE                               */
/*                        https://godotengine.org                         */
/**************************************************************************/
/* Copyright (c) 2014-present Godot Engine contributors (see AUTHORS.md). */
/* Copyright (c) 2007-2014 Juan Linietsky, Ariel Manzur.                  */
/*                                                                        */
/* Permission is hereby granted, free of charge, to any person obtaining  */
/* a copy of this software and associated documentation files (the        */
/* "Software"), to deal in the Software without restriction, including    */
/* without limitation the rights to use, copy, modify, merge, publish,    */
/* distribute, sublicense, and/or sell copies of the Software, and to     */
/* permit persons to whom the Software is furnished to do so, subject to  */
/* the following conditions:                                              */
/*                                                                        */
/* The above copyright notice and this permission notice shall be         */
/* included in all copies or substantial portions of the Software.        */
/*                                                                        */
/* THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,        */
/* EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF     */
/* MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. */
/* IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY   */
/* CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,   */
/* TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE      */
/* SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.                 */
/**************************************************************************/

#ifndef SPIRE_LINE_BUILDER_H
#define SPIRE_LINE_BUILDER_H
#include "line_2d.h"
#include <math.h>
#include <vector>
namespace spire { namespace line_mesh {
struct Vector2 : public Vec2 {
    Vector2() {}
    Vector2(float a, float b) : Vec2(a, b) {}
    Vector2(const Vec2& v) : Vec2(v) {}
    Vector2 operator+(const Vec2& v) const { return Vector2(x + v.x, y + v.y); }
    Vector2 operator-(const Vec2& v) const { return Vector2(x - v.x, y - v.y); }
    Vector2 operator*(float value) const { return Vector2(x * value, y * value); }
    Vector2 operator/(float value) const { return Vector2(x / value, y / value); }
    Vector2& operator+=(const Vec2& v) { x += v.x; y += v.y; return *this; }
    Vector2& operator-=(const Vec2& v) { x -= v.x; y -= v.y; return *this; }
    float length() const { return sqrtf(x * x + y * y); }
    Vector2 normalized() const { float n = length(); return n > 0 ? Vector2(x / n, y / n) : Vector2(); }
    Vector2 orthogonal() const { return Vector2(y, -x); }
    float dot(const Vector2& v) const { return x * v.x + y * v.y; }
    float cross(const Vector2& v) const { return x * v.y - y * v.x; }
    float distance_to(const Vector2& v) const { return Vector2(*this - v).length(); }
    float distance_squared_to(const Vector2& v) const { float a = x - v.x, b = y - v.y; return a * a + b * b; }
    float angle_to(const Vector2& v) const { return atan2f(cross(v), dot(v)); }
    Vector2 lerp(const Vector2& v, float t) const { return *this + (v - *this) * t; }
    Vector2 operator-() const { return Vector2(-x, -y); }
};
inline Vector2 operator*(float a, const Vector2& v) { return Vector2(v.x * a, v.y * a); }
struct MeshRect {
    Vector2 position, size;
    MeshRect(float x, float y, float w, float h) : position(x, y), size(w, h) {}
    Vector2 get_size() const { return size; }
};
struct Curve {
    const CurveData* data;
    explicit Curve(const CurveData* value) : data(value) {}
    float sample_baked(float t) const { return data->sample(t); }
};
struct Gradient {
    const GradientData* data;
    explicit Gradient(const GradientData* value) : data(value) {}
    Color get_color(int i) const { return data->colors[(size_t)i]; }
    int get_point_count() const { return (int)data->colors.size(); }
    Color get_color_at_offset(float t) const { return data->at(t); }
};
class LineBuilder {
public:
	// TODO Move in a struct and reference it
	// Input
	std::vector<Vector2> points;
	Line2D::LineJointMode joint_mode;
	Line2D::LineCapMode begin_cap_mode;
	Line2D::LineCapMode end_cap_mode;
	bool closed;
	float width;
	Curve *curve;
	Color default_color;
	Gradient *gradient;
	Line2D::LineTextureMode texture_mode;
	float sharp_limit;
	int round_precision;
	float tile_aspect; // w/h
	// TODO offset_joints option (offers alternative implementation of round joints)

	// TODO Move in a struct and reference it
	// Output
	std::vector<Vector2> vertices;
	std::vector<Color> colors;
	std::vector<Vector2> uvs;
	std::vector<int> indices;

	LineBuilder();

	void build();

private:
	enum Orientation {
		UP = 0,
		DOWN = 1
	};

	// Triangle-strip methods
	void strip_begin(Vector2 up, Vector2 down, Color color, float uvx);
	void strip_new_quad(Vector2 up, Vector2 down, Color color, float uvx);
	void strip_add_quad(Vector2 up, Vector2 down, Color color, float uvx);
	void strip_add_tri(Vector2 up, Orientation orientation);
	void strip_add_arc(Vector2 center, float angle_delta, Orientation orientation);

	void new_arc(Vector2 center, Vector2 vbegin, float angle_delta, Color color, MeshRect uv_rect);

private:
	bool _interpolate_color;
	int _last_index[2]; // Index of last up and down vertices of the strip
};
} } /* namespace spire::line_mesh */
#endif

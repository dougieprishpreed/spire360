#include "line_2d.h"
#include "line_builder.h"
#include "canvas.h"
#include "shaders.h"
#include "value.h"

#include <math.h>

namespace spire {

bool Line2D::set(const std::string& key, const Json& value, const PropCtx& ctx) {
    if (key == "points") {
        points.clear();
        const Json& data = value["v"];
        for (size_t i = 0; i + 1 < data.size(); i += 2) points.push_back(Vec2(data[i].f(), data[i + 1].f()));
    } else if (key == "width") width = value.f(10);
    else if (key == "default_color") jv_color(value, color);
    else if (key == "width_curve") CurveData::from(ctx.resource(value), width_curve);
    else if (key == "gradient") GradientData::from(ctx.resource(value), gradient);
    else if (key == "texture") {
        Resource* r = ctx.resource(value);
        texture = r && r->is_texture() ? (Texture2DRes*)r : NULL;
    } else if (key == "texture_mode") texture_mode = value.i();
    else if (key == "closed") closed = value.b();
    else if (key == "begin_cap_mode") begin_cap = value.i();
    else if (key == "end_cap_mode") end_cap = value.i();
    else if (key == "joint_mode") joint_mode = value.i();
    else if (key == "sharp_limit") sharp_limit = value.f(2);
    else if (key == "round_precision") round_precision = value.i(8);
    else return Node2D::set(key, value, ctx);
    return true;
}

void Line2D::draw(Canvas& canvas) {
    if (points.size() < 2 || width <= 0) return;
    line_mesh::LineBuilder builder;
    for (size_t i = 0; i < points.size(); ++i) builder.points.push_back(points[i]);
    builder.width = width;
    builder.joint_mode = (LineJointMode)joint_mode;
    builder.begin_cap_mode = (LineCapMode)begin_cap;
    builder.end_cap_mode = (LineCapMode)end_cap;
    builder.closed = closed;
    builder.sharp_limit = sharp_limit;
    builder.round_precision = round_precision > 0 ? round_precision : 1;
    builder.default_color = color;
    builder.texture_mode = (LineTextureMode)texture_mode;
    line_mesh::Gradient gradient_adapter(&gradient);
    line_mesh::Curve curve_adapter(&width_curve);
    builder.gradient = gradient.valid() ? &gradient_adapter : NULL;
    builder.curve = width_curve.valid() ? &curve_adapter : NULL;
    ImageTextureRes* image = NULL;
    Rect2 destination, source;
    Vec2 uv_from, uv_size(1, 1);
    if (texture && texture_mode != 0 && texture->get_rect_region(Rect2(Vec2(), texture->size()),
        Rect2(Vec2(), texture->size()), destination, source, &image) && image) {
        Vec2 pixels = image->size(), layout = texture->size();
        uv_from = Vec2(source.pos.x / pixels.x, source.pos.y / pixels.y);
        uv_size = Vec2(source.size.x / pixels.x, source.size.y / pixels.y);
        builder.tile_aspect = layout.y > 0 ? layout.x / layout.y : 1;
    }
    builder.build();
    if (builder.indices.empty()) return;
    std::vector<Vec2> vertices, uvs;
    std::vector<Color> colors;
    Vec2 flipbook_size(1, 1), flipbook_offset;
    if (material && material->shader == "shaders/vfx/common/vfx_projectile_flipbook_shader.gdshader") {
        const Json* size = get_extra("instance_shader_parameters/flipbook_size");
        if (size) jv_vec2(*size, flipbook_size);
        if (flipbook_size.x < 1) flipbook_size.x = 1;
        if (flipbook_size.y < 1) flipbook_size.y = 1;
        const Json* count = get_extra("instance_shader_parameters/frame_count");
        const Json* speed = get_extra("instance_shader_parameters/flipbook_speed");
        const Json* offset = get_extra("instance_shader_parameters/flipbook_offset");
        float frames = count ? count->f(1) : 1, initial = offset ? offset->f() : 0;
        float progress = initial + (float)g_shader_time * (speed ? speed->f() : 0);
        float frame = fmodf(floorf((frames * initial + frames * progress) * 0.99f), flipbook_size.x * flipbook_size.y);
        if (frame < 0) frame += flipbook_size.x * flipbook_size.y;
        flipbook_offset = Vec2(fmodf(frame, flipbook_size.x) / flipbook_size.x,
                              floorf((frame + 0.5f) / flipbook_size.x) / flipbook_size.y);
    }
    for (size_t i = 0; i < builder.vertices.size(); ++i) {
        vertices.push_back(builder.vertices[i]);
        Vec2 uv = i < builder.uvs.size() ? (Vec2)builder.uvs[i] : Vec2();
        uv = flipbook_offset + Vec2(uv.x / flipbook_size.x, uv.y / flipbook_size.y);
        uvs.push_back(uv_from + uv * uv_size);
        colors.push_back(builder.colors.size() == 1 ? builder.colors[0] : builder.colors[i]);
    }
    canvas.draw_triangles(image, vertices, uvs, colors, builder.indices, texture_mode == 1 ? DRAW_REPEAT : 0);
}

} /* namespace spire */

#version 450
// The canvas's vertex shader (vk_renderer.cpp): screen pixels of the target
// to Vulkan's clip space, y down, pixel centres on halves (the software
// renderer's rules); and the position over the target, 0..1, for the
// shaders that read the screen copy or the group layer.

layout(location = 0) in vec2 a_pos;
layout(location = 1) in vec4 a_color;
layout(location = 2) in vec2 a_uv;

layout(push_constant) uniform Target {
    vec4 viewport; // 2 / width, 2 / height, -1, -1
    vec4 screen;   // 1 / width, 1 / height
} T;

layout(location = 0) out vec4 v_color;
layout(location = 1) out vec2 v_uv;
layout(location = 2) out vec2 v_screen;

void main() {
    gl_Position = vec4(a_pos * T.viewport.xy + T.viewport.zw, 0.0, 1.0);
    v_color = a_color;
    v_uv = a_uv;
    v_screen = a_pos * T.screen.xy;
}

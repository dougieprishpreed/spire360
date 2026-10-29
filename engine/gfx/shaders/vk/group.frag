#version 450
// main.frag's canvas group alone: the group's layer through the mask item's
// alpha (Godot's clip_children). xost runs every instruction of main.frag's
// other modes too (171 of them), and the Underdocks puts six layers a frame
// through it.
#include "common.glsl"

void main() {
    float a = texture(g_tex, v_uv).a * v_color.a;
    o_color = texture(g_group, v_screen) * a;
}

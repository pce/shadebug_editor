#version 330 core

in vec2 a_pos;
in vec2 a_uv;
in vec4 i_rect;
in vec4 i_fill;
in vec4 i_border;
in vec2 i_params;
in vec4 i_uv;

uniform vec2 screen;

out vec2 v_local;
out vec2 v_size;
out vec4 v_fill;
out vec4 v_border;
out vec2 v_params;
out vec2 v_uv;

void main() {
    vec2 world = i_rect.xy + a_pos * i_rect.zw;
    vec2 ndc   = (world / screen) * 2.0 - 1.0;
    ndc.y      = -ndc.y;
    gl_Position = vec4(ndc, 0.0, 1.0);
    v_local  = a_pos;
    v_size   = i_rect.zw;
    v_fill   = i_fill;
    v_border = i_border;
    v_params = i_params;
    v_uv     = i_uv.xy + a_uv * i_uv.zw;
}

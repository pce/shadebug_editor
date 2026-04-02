#version 330 core

in  vec2 v_local;
in  vec2 v_size;
in  vec4 v_fill;
in  vec4 v_border;
in  vec2 v_params;
in  vec2 v_uv;
out vec4 frag_color;

uniform sampler2D u_tex;

float sdf_rrect(vec2 p, vec2 half_size, float r) {
    vec2 q = abs(p) - half_size + r;
    return min(max(q.x, q.y), 0.0) + length(max(q, vec2(0.0))) - r;
}

void main() {
    float cr     = v_params.x;
    float bw     = v_params.y;
    vec2  hs   = v_size * 0.5;
    vec2  p      = (v_local - 0.5) * v_size;
    float d      = sdf_rrect(p, hs, max(cr, 0.001));
    float aa     = fwidth(d);
    float outer  = 1.0 - smoothstep(-aa, aa, d);
    vec4  tex_col  = texture(u_tex, v_uv);
    vec4  fill_col = v_fill * tex_col;
    vec4 color;
    if (bw > 0.0) {
        float inner = 1.0 - smoothstep(-aa, aa, d + bw);
        color = mix(v_border, fill_col, inner);
    } else {
        color = fill_col;
    }
    color.a   *= outer;
    frag_color = color;
}

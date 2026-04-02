#pragma once

// ── Embedded shader sources for the instanced rect pipeline ──────────────────
//
//  One vertex shader + one fragment shader per backend.
//
//  Vertex shader:  unit-quad × instance UiRect → NDC clip space.
//  Fragment shader: SDF rounded rect + border + texture/solid fill.
//
//  Uniform layout (vertex stage, block 0):
//    float2 screen;  — framebuffer size in pixels
//
//  Texture (fragment stage, slot 0):
//    sampler2D u_tex — 2D RGBA texture; bind a 1×1 white default for solid fills.
//

namespace shadebug::renderer::shaders {

// ─────────────────────────────────────────────────────────────────────────────
//  GLSL 330 core  (SOKOL_GLCORE)
// ─────────────────────────────────────────────────────────────────────────────

inline constexpr const char* kGlslVert = R"(
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
)";

inline constexpr const char* kGlslFrag = R"(
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
)";

// ─────────────────────────────────────────────────────────────────────────────
//  Metal Shading Language 2.0  (SOKOL_METAL)
// ─────────────────────────────────────────────────────────────────────────────
//
//  Vertex buffers:
//    [[buffer(0)]]  — static quad vertices  (SG_VERTEXSTEP_PER_VERTEX)
//    [[buffer(1)]]  — instance UiRect data  (SG_VERTEXSTEP_PER_INSTANCE)
//
//  VS uniform block → [[buffer(2)]]  (msl_buffer_n = 2)
//
//  Fragment texture  → [[texture(0)]]
//  Fragment sampler  → [[sampler(0)]]
//

inline constexpr const char* kMslVert = R"(
#include <metal_stdlib>
using namespace metal;

struct RectVert {
    float2 pos        [[attribute(0)]];
    float2 uv         [[attribute(1)]];
    float4 i_rect     [[attribute(2)]];
    float4 i_fill     [[attribute(3)]];
    float4 i_border   [[attribute(4)]];
    float2 i_params   [[attribute(5)]];
    float4 i_uv       [[attribute(6)]];
};

struct Uniforms {
    float2 screen;
    float2 _pad;
};

struct Varyings {
    float4 pos    [[position]];
    float2 local;
    float2 size;
    float4 fill;
    float4 border;
    float2 params;
    float2 uv;
};

vertex Varyings vs_main(RectVert in [[stage_in]],
                        constant Uniforms& u [[buffer(2)]])
{
    Varyings out;
    float2 world = in.i_rect.xy + in.pos * in.i_rect.zw;
    float2 ndc   = (world / u.screen) * 2.0 - 1.0;
    ndc.y        = -ndc.y;
    out.pos    = float4(ndc, 0.0, 1.0);
    out.local  = in.pos;
    out.size   = in.i_rect.zw;
    out.fill   = in.i_fill;
    out.border = in.i_border;
    out.params = in.i_params;
    out.uv     = in.i_uv.xy + in.uv * in.i_uv.zw;
    return out;
}
)";

inline constexpr const char* kMslFrag = R"(
#include <metal_stdlib>
using namespace metal;

struct Varyings {
    float4 pos    [[position]];
    float2 local;
    float2 size;
    float4 fill;
    float4 border;
    float2 params;
    float2 uv;
};

float sdf_rrect(float2 p, float2 half_size, float r) {
    float2 q = abs(p) - half_size + r;
    return min(max(q.x, q.y), 0.0) + length(max(q, float2(0.0))) - r;
}

fragment float4 fs_main(Varyings in [[stage_in]],
                         texture2d<float> tex [[texture(0)]],
                         sampler smp [[sampler(0)]])
{
    float cr     = in.params.x;
    float bw     = in.params.y;
    float2 hs  = in.size * 0.5;
    float2 p     = (in.local - 0.5) * in.size;
    float d      = sdf_rrect(p, hs, max(cr, 0.001));
    float aa     = length(float2(dfdx(d), dfdy(d)));
    float outer  = 1.0 - smoothstep(-aa, aa, d);

    float4 tex_col  = tex.sample(smp, in.uv);
    float4 fill_col = in.fill * tex_col;

    float4 color;
    if (bw > 0.0) {
        float inner = 1.0 - smoothstep(-aa, aa, d + bw);
        color = mix(in.border, fill_col, inner);
    } else {
        color = fill_col;
    }
    color.a *= outer;
    return color;
}
)";

// ─────────────────────────────────────────────────────────────────────────────
//  HLSL 5.0  (SOKOL_D3D11)
// ─────────────────────────────────────────────────────────────────────────────

inline constexpr const char* kHlslVert = R"(
cbuffer vs_params : register(b0) {
    float2 screen;
    float2 _pad;
};

struct VsIn {
    float2 pos      : POSITION0;
    float2 uv       : TEXCOORD0;
    float4 i_rect   : TEXCOORD1;
    float4 i_fill   : TEXCOORD2;
    float4 i_border : TEXCOORD3;
    float2 i_params : TEXCOORD4;
    float4 i_uv     : TEXCOORD5;
};

struct VsOut {
    float4 pos    : SV_Position;
    float2 local  : TEXCOORD0;
    float2 size   : TEXCOORD1;
    float4 fill   : TEXCOORD2;
    float4 border : TEXCOORD3;
    float2 params : TEXCOORD4;
    float2 uv     : TEXCOORD5;
};

VsOut vs_main(VsIn inp) {
    VsOut o;
    float2 world = inp.i_rect.xy + inp.pos * inp.i_rect.zw;
    float2 ndc   = (world / screen) * 2.0 - 1.0;
    ndc.y        = -ndc.y;
    o.pos    = float4(ndc, 0.0, 1.0);
    o.local  = inp.pos;
    o.size   = inp.i_rect.zw;
    o.fill   = inp.i_fill;
    o.border = inp.i_border;
    o.params = inp.i_params;
    o.uv     = inp.i_uv.xy + inp.uv * inp.i_uv.zw;
    return o;
}
)";

inline constexpr const char* kHlslFrag = R"(
Texture2D<float4> u_tex : register(t0);
SamplerState      u_smp : register(s0);

struct VsOut {
    float4 pos    : SV_Position;
    float2 local  : TEXCOORD0;
    float2 size   : TEXCOORD1;
    float4 fill   : TEXCOORD2;
    float4 border : TEXCOORD3;
    float2 params : TEXCOORD4;
    float2 uv     : TEXCOORD5;
};

float sdf_rrect(float2 p, float2 b, float r) {
    float2 q = abs(p) - b + r;
    return min(max(q.x, q.y), 0.0) + length(max(q, float2(0.0, 0.0))) - r;
}

float4 fs_main(VsOut inp) : SV_Target0 {
    float cr    = inp.params.x;
    float bw    = inp.params.y;
    float2 hs = inp.size * 0.5;
    float2 p    = (inp.local - 0.5) * inp.size;
    float d     = sdf_rrect(p, hs, max(cr, 0.001));
    float aa    = fwidth(d);
    float outer = 1.0 - smoothstep(-aa, aa, d);

    float4 tex_col  = u_tex.Sample(u_smp, inp.uv);
    float4 fill_col = inp.fill * tex_col;
    float4 color;
    if (bw > 0.0) {
        float inner = 1.0 - smoothstep(-aa, aa, d + bw);
        color = lerp(inp.border, fill_col, inner);
    } else {
        color = fill_col;
    }
    color.a *= outer;
    return color;
}
)";

// ── Runtime source selection ──────────────────────────────────────────────────

[[nodiscard]] inline const char* vert_source() noexcept {
#if defined(SOKOL_METAL)
    return kMslVert;
#elif defined(SOKOL_D3D11)
    return kHlslVert;
#else
    return kGlslVert;
#endif
}

[[nodiscard]] inline const char* frag_source() noexcept {
#if defined(SOKOL_METAL)
    return kMslFrag;
#elif defined(SOKOL_D3D11)
    return kHlslFrag;
#else
    return kGlslFrag;
#endif
}

} // namespace shadebug::renderer::shaders

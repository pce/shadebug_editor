#pragma once

//  Embedded shaders for the 3D coloured-mesh pipeline
//
//  Vertex layout (one buffer, per-vertex):
//    attr 0  a_pos  — float3  world-space position
//    attr 1  a_col  — float3  RGB vertex colour
//
//  VS uniform block (block 0):
//    mat4 mvp  — column-major model-view-projection
//
//  Fragment texture (slot 0):
//    u_albedo — 2D RGBA texture; bind a 1×1 white image for pure vertex colour,
//               or bind the SDF panel's sample_view for Option B overlay.
//
//  Screen-space UV: computed in the VS as NDC → [0,1] and passed to the FS.
//  The fragment colour is: vertex_col * albedo_sample.
//

namespace shadebug::renderer::scene3d_shaders {

//  Metal Shading Language 2.x  (SOKOL_METAL)
//
//  Bindings:
//    [[buffer(0)]]  — vertex data
//    [[buffer(1)]]  — MeshUniforms
//    [[texture(0)]] — albedo (fragment)
//    [[sampler(0)]] — albedo sampler (fragment)

inline constexpr const char* kMslVert = R"(
#include <metal_stdlib>
using namespace metal;

struct MeshVert {
    float3 pos [[attribute(0)]];
    float3 col [[attribute(1)]];
};

struct MeshUniforms {
    float4x4 mvp;
};

struct Varyings {
    float4 pos       [[position]];
    float4 col;
    float2 screen_uv;   // NDC -> [0,1], used to sample the SDF/albedo texture
};

vertex Varyings vs_main(MeshVert in [[stage_in]],
                        constant MeshUniforms& u [[buffer(1)]])
{
    Varyings out;
    float4 clip  = u.mvp * float4(in.pos, 1.0);
    out.pos      = clip;
    out.col      = float4(in.col, 1.0);
    // Screen-space UV: NDC [-1,1] -> [0,1]; flip Y so texture V=0 is top
    out.screen_uv = float2(clip.x / clip.w * 0.5 + 0.5,
                            1.0 - (clip.y / clip.w * 0.5 + 0.5));
    return out;
}
)";

inline constexpr const char* kMslFrag = R"(
#include <metal_stdlib>
using namespace metal;

struct Varyings {
    float4 pos       [[position]];
    float4 col;
    float2 screen_uv;
};

fragment float4 fs_main(Varyings in [[stage_in]],
                        texture2d<float> albedo [[texture(0)]],
                        sampler          smp    [[sampler(0)]])
{
    float4 tex = albedo.sample(smp, in.screen_uv);
    return in.col * tex;
}
)";

//  GLSL 330 core  (SOKOL_GLCORE)

inline constexpr const char* kGlslVert = R"(
#version 330 core

in vec3 a_pos;
in vec3 a_col;

uniform mat4 mvp;

out vec4 v_col;
out vec2 v_screen_uv;

void main() {
    vec4 clip     = mvp * vec4(a_pos, 1.0);
    gl_Position   = clip;
    v_col         = vec4(a_col, 1.0);
    v_screen_uv   = vec2(clip.x / clip.w * 0.5 + 0.5,
                         1.0 - (clip.y / clip.w * 0.5 + 0.5));
}
)";

inline constexpr const char* kGlslFrag = R"(
#version 330 core

in  vec4 v_col;
in  vec2 v_screen_uv;
out vec4 frag_color;

uniform sampler2D u_albedo;

void main() {
    vec4 tex    = texture(u_albedo, v_screen_uv);
    frag_color  = v_col * tex;
}
)";

//  HLSL 5.0  (SOKOL_D3D11)

inline constexpr const char* kHlslVert = R"(
cbuffer MeshUniforms : register(b0) {
    column_major float4x4 mvp;
};

struct VsIn {
    float3 pos : POSITION0;
    float3 col : TEXCOORD0;
};

struct VsOut {
    float4 pos       : SV_Position;
    float4 col       : TEXCOORD0;
    float2 screen_uv : TEXCOORD1;
};

VsOut vs_main(VsIn inp) {
    VsOut o;
    float4 clip   = mul(mvp, float4(inp.pos, 1.0));
    o.pos         = clip;
    o.col         = float4(inp.col, 1.0);
    o.screen_uv   = float2(clip.x / clip.w * 0.5 + 0.5,
                           1.0 - (clip.y / clip.w * 0.5 + 0.5));
    return o;
}
)";

inline constexpr const char* kHlslFrag = R"(
Texture2D<float4> u_albedo : register(t0);
SamplerState      u_smp    : register(s0);

struct VsOut {
    float4 pos       : SV_Position;
    float4 col       : TEXCOORD0;
    float2 screen_uv : TEXCOORD1;
};

float4 fs_main(VsOut inp) : SV_Target0 {
    float4 tex = u_albedo.Sample(u_smp, inp.screen_uv);
    return inp.col * tex;
}
)";

// Runtime selection
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

} // namespace shadebug::renderer::scene3d_shaders


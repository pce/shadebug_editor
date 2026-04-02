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

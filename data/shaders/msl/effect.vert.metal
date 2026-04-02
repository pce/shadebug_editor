#include <metal_stdlib>
using namespace metal;
struct EffectVert {
    float2 pos [[attribute(0)]];
};
struct Varyings {
    float4 pos [[position]];
    float2 uv;
};
vertex Varyings vs_main(EffectVert in [[stage_in]]) {
    Varyings out;
    out.pos = float4(in.pos * 2.0f - 1.0f, 0.0f, 1.0f);
    out.uv  = in.pos;
    return out;
}

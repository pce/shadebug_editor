#include <metal_stdlib>
using namespace metal;
struct EffectUniforms { float2 iResolution; float iTime; float _pad; };
struct Varyings { float4 pos [[position]]; float2 uv; };
fragment float4 fs_main(Varyings in [[stage_in]], constant EffectUniforms& u [[buffer(0)]]) {
    float2 uv = in.uv;
    float t = u.iTime * 0.5f;
    float3 col_a = float3(0.25f, 0.05f, 0.55f);
    float3 col_b = float3(0.05f, 0.35f, 0.65f);
    float blend = metal::sin(t + uv.x * 2.0f + uv.y * 1.5f) * 0.5f + 0.5f;
    float3 grad = metal::mix(col_a, col_b, blend);
    float pulse = metal::sin(u.iTime * 1.2f) * 0.1f + 0.9f;
    float2 center = uv - 0.5f;
    float dist = metal::length(center);
    float vignette = metal::smoothstep(0.7f, 0.2f, dist * pulse);
    float3 col = grad * vignette;
    return float4(col, 1.0f);
}

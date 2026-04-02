#include <metal_stdlib>
using namespace metal;
struct EffectUniforms { float2 iResolution; float iTime; float _pad; };
struct Varyings { float4 pos [[position]]; float2 uv; };
fragment float4 fs_main(Varyings in [[stage_in]], constant EffectUniforms& u [[buffer(0)]]) {
    float2 uv = in.uv;
    float freq1 = 6.0f;
    float freq2 = 4.0f;
    float2 d1 = float2(metal::sin(u.iTime * 0.4f + uv.y * freq1), metal::cos(u.iTime * 0.3f + uv.x * freq1)) * 0.08f;
    float2 d2 = float2(metal::cos(u.iTime * 0.5f + uv.x * freq2), metal::sin(u.iTime * 0.6f + uv.y * freq2)) * 0.06f;
    float2 uv1 = uv + d1;
    float2 uv2 = float2(1.0f - uv.y, uv.x) + d2;
    float r1 = metal::length(uv1 - 0.5f);
    float r2 = metal::length(uv2 - 0.5f);
    float rings1 = metal::sin(r1 * 25.0f - u.iTime * 2.0f) * 0.5f + 0.5f;
    float rings2 = metal::sin(r2 * 20.0f + u.iTime * 1.5f) * 0.5f + 0.5f;
    float3 col1 = metal::mix(float3(0.1f, 0.3f, 0.9f), float3(0.9f, 0.2f, 0.6f), rings1);
    float3 col2 = metal::mix(float3(0.1f, 0.8f, 0.5f), float3(0.9f, 0.6f, 0.1f), rings2);
    float3 col = metal::mix(col1, col2, 0.5f);
    return float4(col, 1.0f);
}

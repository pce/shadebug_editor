#include <metal_stdlib>
using namespace metal;

struct EffectUniforms { float2 iResolution; float iTime; float _pad; };
// Custom params: slot 0=speed, 1=freq1, 2=freq2, 4..7=color_a, 8..11=color_b
struct ParamUniforms  { float4 p0; float4 p1; float4 p2; float4 p3; };
struct Varyings { float4 pos [[position]]; float2 uv; };

fragment float4 fs_main(Varyings in           [[stage_in]],
                        constant EffectUniforms& u [[buffer(0)]],
                        constant ParamUniforms&  p [[buffer(1)]]) {
    float2 uv = in.uv;

    // p0: x=speed, y=freq1, z=freq2
    float speed = (p.p0.x > 0.0f) ? p.p0.x : 1.0f;
    float freq1 = (p.p0.y > 0.0f) ? p.p0.y : 6.0f;
    float freq2 = (p.p0.z > 0.0f) ? p.p0.z : 4.0f;

    // p1 = color_a (slots 4-7), p2 = color_b (slots 8-11)
    float3 ca = (p.p1.x + p.p1.y + p.p1.z > 0.0f)
        ? p.p1.xyz : float3(0.1f, 0.3f, 0.9f);
    float3 cb = (p.p2.x + p.p2.y + p.p2.z > 0.0f)
        ? p.p2.xyz : float3(0.1f, 0.8f, 0.5f);
    float3 cc = float3(0.9f, 0.2f, 0.6f);
    float3 cd = float3(0.9f, 0.6f, 0.1f);

    float t = u.iTime * speed;
    float2 d1 = float2(metal::sin(t * 0.4f + uv.y * freq1),
                       metal::cos(t * 0.3f + uv.x * freq1)) * 0.08f;
    float2 d2 = float2(metal::cos(t * 0.5f + uv.x * freq2),
                       metal::sin(t * 0.6f + uv.y * freq2)) * 0.06f;
    float2 uv1 = uv + d1;
    float2 uv2 = float2(1.0f - uv.y, uv.x) + d2;
    float r1 = metal::length(uv1 - 0.5f);
    float r2 = metal::length(uv2 - 0.5f);
    float rings1 = metal::sin(r1 * 25.0f - t * 2.0f) * 0.5f + 0.5f;
    float rings2 = metal::sin(r2 * 20.0f + t * 1.5f) * 0.5f + 0.5f;
    float3 col1 = metal::mix(ca, cc, rings1);
    float3 col2 = metal::mix(cb, cd, rings2);
    float3 col  = metal::mix(col1, col2, 0.5f);
    return float4(col, 1.0f);
}

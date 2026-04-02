#include <metal_stdlib>
using namespace metal;
struct EffectUniforms { float2 iResolution; float iTime; float _pad; };
struct Varyings { float4 pos [[position]]; float2 uv; };

static float2 hash2(float2 p) {
    p = float2(metal::dot(p, float2(127.1f, 311.7f)), metal::dot(p, float2(269.5f, 183.3f)));
    return metal::fract(metal::sin(p) * 43758.5453f);
}
static float gnoise(float2 p) {
    float2 i = metal::floor(p);
    float2 f = metal::fract(p);
    float2 u = f * f * (3.0f - 2.0f * f);
    float a = metal::dot(hash2(i             ) * 2.0f - 1.0f, f            );
    float b = metal::dot(hash2(i + float2(1,0)) * 2.0f - 1.0f, f - float2(1,0));
    float c = metal::dot(hash2(i + float2(0,1)) * 2.0f - 1.0f, f - float2(0,1));
    float d = metal::dot(hash2(i + float2(1,1)) * 2.0f - 1.0f, f - float2(1,1));
    return metal::mix(metal::mix(a, b, u.x), metal::mix(c, d, u.x), u.y);
}
static float fbm(float2 p, int oct) {
    float v = 0.0f;
    float amp = 0.5f;
    for (int i = 0; i < oct; i++) {
        v += gnoise(p) * amp;
        p *= 2.0f;
        amp *= 0.5f;
    }
    return v;
}
fragment float4 fs_main(Varyings in [[stage_in]], constant EffectUniforms& u [[buffer(0)]]) {
    float2 uv = in.uv;
    float2 scroll1 = u.iTime * float2(0.10f, 0.07f);
    float2 scroll2 = u.iTime * float2(-0.08f, -0.05f);
    float2 p1 = uv * 4.0f + scroll1;
    float2 p2 = uv * 3.5f + scroll2 + float2(5.2f, 1.3f);
    float2 q = float2(fbm(p1, 4), fbm(p1 + float2(1.7f, 9.2f), 4));
    float2 r = float2(fbm(p2 + 4.0f*q, 4), fbm(p2 + 4.0f*q + float2(8.3f, 2.8f), 4));
    float n = fbm(uv * 3.0f + 4.0f * r + scroll1, 4) * 0.5f + 0.5f;
    float3 deep   = float3(0.05f, 0.15f, 0.45f);
    float3 mid    = float3(0.10f, 0.55f, 0.65f);
    float3 bright = float3(0.50f, 0.95f, 0.85f);
    float3 col = metal::mix(deep, mid, metal::smoothstep(0.2f, 0.55f, n));
    col = metal::mix(col, bright, metal::smoothstep(0.55f, 0.85f, n));
    return float4(col, 1.0f);
}

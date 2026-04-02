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
    float t = u.iTime * 0.1f;
    if (uv.x < 0.33f) {
        float n = gnoise(uv * 5.0f + t) * 0.5f + 0.5f;
        return float4(float3(n), 1.0f);
    } else if (uv.x < 0.66f) {
        float2 p = uv * 4.0f + t;
        float n = fbm(p, 4) * 0.5f + 0.5f;
        float3 col = metal::mix(float3(0.1f, 0.3f, 0.6f), float3(0.8f, 0.9f, 1.0f), n);
        return float4(col, 1.0f);
    } else {
        float2 p = uv * 3.0f;
        float2 q = float2(fbm(p + t, 4), fbm(p + float2(1.7f, 9.2f) + t, 4));
        float n = fbm(p + 4.0f * q, 4) * 0.5f + 0.5f;
        float3 col = metal::mix(float3(0.5f, 0.2f, 0.1f), float3(1.0f, 0.85f, 0.5f), n);
        return float4(col, 1.0f);
    }
}

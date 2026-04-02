#include <metal_stdlib>
using namespace metal;
struct EffectUniforms { float2 iResolution; float iTime; float _pad; };
struct Varyings { float4 pos [[position]]; float2 uv; };

static float bokeh(float2 uv, float2 center, float radius, float blur) {
    float d = metal::length(uv - center);
    return metal::smoothstep(radius + blur, radius - blur, d);
}

fragment float4 fs_main(Varyings in [[stage_in]], constant EffectUniforms& u [[buffer(0)]]) {
    float2 uv = in.uv;
    float3 col = float3(0.02f, 0.02f, 0.05f);

    float2 centers[8] = {
        float2(0.20f, 0.30f), float2(0.70f, 0.20f),
        float2(0.50f, 0.75f), float2(0.15f, 0.80f),
        float2(0.85f, 0.60f), float2(0.40f, 0.45f),
        float2(0.60f, 0.55f), float2(0.30f, 0.65f)
    };
    float3 colors[8] = {
        float3(1.0f, 0.6f, 0.2f), float3(0.2f, 0.8f, 1.0f),
        float3(1.0f, 0.3f, 0.5f), float3(0.5f, 1.0f, 0.3f),
        float3(0.9f, 0.9f, 0.2f), float3(0.6f, 0.3f, 1.0f),
        float3(1.0f, 0.7f, 0.5f), float3(0.3f, 0.9f, 0.8f)
    };
    float depths[8] = { 0.2f, 0.8f, 0.5f, 0.1f, 0.9f, 0.4f, 0.6f, 0.3f };

    float focal = 0.5f + metal::sin(u.iTime * 0.3f) * 0.4f;

    for (int i = 0; i < 8; i++) {
        float2 c = centers[i];
        c.x = metal::fract(c.x + u.iTime * (0.03f + float(i) * 0.005f));
        c.y = metal::fract(c.y + u.iTime * (0.02f + float(i) * 0.003f));
        float depth = depths[i];
        float coc = metal::abs(depth - focal) * 0.15f;
        float blur = metal::max(0.002f, coc);
        float radius = 0.015f + coc * 0.5f;
        float b = bokeh(uv, c, radius, blur);
        col += colors[i] * b * (1.0f - coc * 2.0f);
    }
    return float4(col, 1.0f);
}

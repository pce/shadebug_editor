#include <metal_stdlib>
using namespace metal;
struct EffectUniforms { float2 iResolution; float iTime; float _pad; };
struct Varyings { float4 pos [[position]]; float2 uv; };

static float scene(float2 uv, float iTime) {
    float checker = metal::fmod(metal::floor(uv.x * 10.0f) + metal::floor(uv.y * 10.0f), 2.0f);
    float wave = metal::sin(uv.x * 20.0f + iTime) * metal::sin(uv.y * 20.0f + iTime * 0.7f) * 0.5f + 0.5f;
    return metal::mix(checker, wave, 0.5f);
}

fragment float4 fs_main(Varyings in [[stage_in]], constant EffectUniforms& u [[buffer(0)]]) {
    float2 uv = in.uv;
    float2 px = 1.0f / u.iResolution;

    float tl = scene(uv + float2(-px.x,  px.y), u.iTime);
    float tc = scene(uv + float2( 0.0f,  px.y), u.iTime);
    float tr = scene(uv + float2( px.x,  px.y), u.iTime);
    float ml = scene(uv + float2(-px.x,  0.0f), u.iTime);
    float mr = scene(uv + float2( px.x,  0.0f), u.iTime);
    float bl = scene(uv + float2(-px.x, -px.y), u.iTime);
    float bc = scene(uv + float2( 0.0f, -px.y), u.iTime);
    float br = scene(uv + float2( px.x, -px.y), u.iTime);

    float gx = -tl - 2.0f*ml - bl + tr + 2.0f*mr + br;
    float gy = -tl - 2.0f*tc - tr + bl + 2.0f*bc + br;
    float edge = metal::sqrt(gx*gx + gy*gy);

    float3 edgeColor = metal::mix(float3(0.0f), float3(0.95f, 0.75f, 0.2f), metal::smoothstep(0.1f, 0.4f, edge));
    return float4(edgeColor, 1.0f);
}

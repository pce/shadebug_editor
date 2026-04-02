#include <metal_stdlib>
using namespace metal;
struct EffectUniforms { float2 iResolution; float iTime; float _pad; };
struct Varyings { float4 pos [[position]]; float2 uv; };

static float2 hash2(float2 p) {
    p = float2(metal::dot(p,float2(127.1f,311.7f)), metal::dot(p,float2(269.5f,183.3f)));
    return metal::fract(metal::sin(p)*43758.5453f);
}
static float gnoise(float2 p) {
    float2 i=metal::floor(p); float2 f=metal::fract(p);
    float2 u=f*f*(3.0f-2.0f*f);
    float a=metal::dot(hash2(i           )*2.0f-1.0f, f           );
    float b=metal::dot(hash2(i+float2(1,0))*2.0f-1.0f, f-float2(1,0));
    float c=metal::dot(hash2(i+float2(0,1))*2.0f-1.0f, f-float2(0,1));
    float d=metal::dot(hash2(i+float2(1,1))*2.0f-1.0f, f-float2(1,1));
    return metal::mix(metal::mix(a,b,u.x), metal::mix(c,d,u.x), u.y);
}
static float fbm(float2 p, int oct) {
    float v=0.0f; float amp=0.5f;
    for(int i=0;i<oct;i++){v+=gnoise(p)*amp; p*=2.0f; amp*=0.5f;}
    return v;
}
fragment float4 fs_main(Varyings in [[stage_in]], constant EffectUniforms& u [[buffer(0)]]) {
    float2 uv = in.uv;
    float horizon = 0.55f;
    float sky_mask = metal::step(horizon, uv.y);
    float sea_y = (uv.y < horizon) ? uv.y / horizon : 0.0f;
    float2 seaUV = float2(uv.x, sea_y);
    float t = u.iTime * 0.3f;
    float2 dir1 = float2(0.707f, 0.707f);
    float2 dir2 = float2(-0.707f, 0.707f);
    float w1 = fbm(seaUV*6.0f + dir1*t, 6)*0.5f+0.5f;
    float w2 = fbm(seaUV*5.0f + dir2*t*0.8f, 5)*0.5f+0.5f;
    float h = (w1+w2)*0.5f;
    float fresnel = metal::pow(1.0f - sea_y, 2.0f);
    float3 deep  = float3(0.01f,0.05f,0.20f);
    float3 mid   = float3(0.05f,0.25f,0.55f);
    float3 light = float3(0.50f,0.85f,1.00f);
    float3 seaCol = metal::mix(deep, mid, h);
    seaCol = metal::mix(seaCol, light, fresnel*0.5f);
    float foam = metal::smoothstep(0.62f,0.75f,h)*(1.0f-fresnel);
    seaCol = metal::mix(seaCol, float3(1.0f), foam);
    float3 skyLow  = float3(0.60f,0.82f,0.98f);
    float3 skyHigh = float3(0.10f,0.30f,0.70f);
    float skyT = (uv.y - horizon) / (1.0f - horizon + 0.001f);
    float3 skyCol = metal::mix(skyLow, skyHigh, metal::clamp(skyT, 0.0f, 1.0f));
    float3 col = metal::mix(seaCol, skyCol, sky_mask);
    return float4(col, 1.0f);
}

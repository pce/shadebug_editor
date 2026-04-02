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
    float2 uv = in.uv; float t = u.iTime * 0.12f;
    float2 q = float2(fbm(uv*3.0f+t, 6), fbm(uv*3.0f+float2(5.2f,1.3f)+t*0.8f, 6));
    float n = fbm(uv*2.5f + 2.5f*q + t*0.5f, 6)*0.5f+0.5f;
    float3 sky = metal::mix(float3(0.40f,0.65f,0.95f), float3(0.20f,0.45f,0.80f), uv.y);
    float3 cloudEdge   = float3(0.75f,0.78f,0.82f);
    float3 cloudCenter = float3(0.98f,0.97f,0.95f);
    float cloudMask = metal::smoothstep(0.45f, 0.80f, n);
    float3 cloud = metal::mix(cloudEdge, cloudCenter, metal::smoothstep(0.60f, 0.85f, n));
    float rim = metal::smoothstep(0.44f, 0.55f, n) * (1.0f - metal::smoothstep(0.55f, 0.75f, n));
    cloud += float3(0.15f,0.10f,0.05f) * rim;
    float3 col = metal::mix(sky, cloud, cloudMask);
    return float4(col, 1.0f);
}

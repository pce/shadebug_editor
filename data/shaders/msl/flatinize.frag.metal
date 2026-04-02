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
    float t = u.iTime * 0.08f;
    float n = fbm(uv*4.0f+t, 3)*0.5f+0.5f;
    float bands = 5.0f;
    float q = metal::floor(n*bands)/bands;
    float blend = metal::smoothstep(0.0f, 0.2f/bands, metal::fract(n*bands));
    float qn = metal::mix(q, q+1.0f/bands, blend*0.7f);
    float3 c0=float3(0.15f,0.55f,0.90f); float3 c1=float3(0.25f,0.80f,0.55f);
    float3 c2=float3(0.95f,0.85f,0.25f); float3 c3=float3(0.95f,0.45f,0.20f);
    float3 c4=float3(0.70f,0.20f,0.55f);
    float3 col;
    if      (qn < 0.2f) col = metal::mix(c0,c1, qn*5.0f);
    else if (qn < 0.4f) col = metal::mix(c1,c2, (qn-0.2f)*5.0f);
    else if (qn < 0.6f) col = metal::mix(c2,c3, (qn-0.4f)*5.0f);
    else if (qn < 0.8f) col = metal::mix(c3,c4, (qn-0.6f)*5.0f);
    else                col = c4;
    return float4(col, 1.0f);
}

#include <metal_stdlib>
using namespace metal;
struct EffectUniforms { float2 iResolution; float iTime; float _pad; };
struct Varyings { float4 pos [[position]]; float2 uv; };

static float2 hash2(float2 p) {
    p = float2(metal::dot(p, float2(127.1f,311.7f)), metal::dot(p, float2(269.5f,183.3f)));
    return metal::fract(metal::sin(p) * 43758.5453f);
}
static float gnoise(float2 p) {
    float2 i = metal::floor(p); float2 f = metal::fract(p);
    float2 u = f*f*(3.0f-2.0f*f);
    float a = metal::dot(hash2(i           )*2.0f-1.0f, f           );
    float b = metal::dot(hash2(i+float2(1,0))*2.0f-1.0f, f-float2(1,0));
    float c = metal::dot(hash2(i+float2(0,1))*2.0f-1.0f, f-float2(0,1));
    float d = metal::dot(hash2(i+float2(1,1))*2.0f-1.0f, f-float2(1,1));
    return metal::mix(metal::mix(a,b,u.x), metal::mix(c,d,u.x), u.y);
}
static float fbm(float2 p, int oct) {
    float v=0.0f; float amp=0.5f;
    for(int i=0;i<oct;i++){v+=gnoise(p)*amp; p*=2.0f; amp*=0.5f;}
    return v;
}
fragment float4 fs_main(Varyings in [[stage_in]], constant EffectUniforms& u [[buffer(0)]]) {
    float2 uv = in.uv;
    float t = u.iTime * 0.5f;
    float2 wuv = uv;
    wuv.x += fbm(float2(uv.x*3.0f, uv.y*2.0f+t*0.4f), 4) * 0.12f;
    wuv.y += fbm(float2(uv.x*2.5f+t*0.3f, uv.y*3.0f), 4) * 0.10f;
    float wave  = fbm(wuv*5.0f + float2(t*0.6f, 0.0f), 4) * 0.5f + 0.5f;
    float wave2 = fbm(wuv*4.0f + float2(-t*0.4f, t*0.2f), 3) * 0.5f + 0.5f;
    float h = metal::mix(wave, wave2, 0.4f);
    float3 trough = float3(0.02f,0.08f,0.30f);
    float3 crest  = float3(0.60f,0.95f,1.00f);
    float3 col = metal::mix(trough, crest, metal::smoothstep(0.35f, 0.75f, h));
    float foam = metal::smoothstep(0.70f, 0.80f, h) * metal::smoothstep(0.90f, 0.80f, h);
    col = metal::mix(col, float3(1.0f), foam * 0.8f);
    float sky = metal::smoothstep(0.7f, 1.0f, uv.y);
    float3 skyCol = float3(0.45f,0.75f,0.95f);
    col = metal::mix(col, skyCol, sky * 0.4f);
    return float4(col, 1.0f);
}

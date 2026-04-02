#include <metal_stdlib>
using namespace metal;

// common

struct EffectUniforms {
    float2 iResolution;
    float  iTime;
    float  _pad;
};

struct Varyings {
    float4 pos [[position]];
    float2 uv;
};

float2 rotate2D(float2 p, float a) {
    float c = cos(a), s = sin(a);
    return float2(c*p.x - s*p.y, s*p.x + c*p.y);
}

// noise
float hash1(float3 p) {
    return fract(sin(dot(p, float3(127.1,311.7,74.7))) * 43758.5453);
}

float noise(float3 p) {
    float3 i = floor(p), f = fract(p);
    f = f * f * (3.0 - 2.0 * f);
    return mix(mix(mix(hash1(i+float3(0,0,0)), hash1(i+float3(1,0,0)), f.x),
                   mix(hash1(i+float3(0,1,0)), hash1(i+float3(1,1,0)), f.x), f.y),
               mix(mix(hash1(i+float3(0,0,1)), hash1(i+float3(1,0,1)), f.x),
                   mix(hash1(i+float3(0,1,1)), hash1(i+float3(1,1,1)), f.x), f.y), f.z);
}

float fbm(float3 p) {
    float v = 0.0, a = 0.5;
    for (int i = 0; i < 5; i++) { v += noise(p)*a; p *= 2.0; a *= 0.5; }
    return v;
}

// animation
float3 animateGalaxy(float3 p, float time) {
    float r = length(p.xy);
    float angle = time * 0.4 / (r + 0.2);
    float c = cos(angle), s = sin(angle);
    p.xy = float2(c*p.x - s*p.y, s*p.x + c*p.y);
    return p;
}

float starTwinkle(float h, float time) {
    return 0.8 + 0.4 * sin(time * 8.0 + h * 100.0);
}

// stars
float3 starColor(float t) {
    float temp = mix(2000.0, 12000.0, t);
    return float3(1.0,
                  clamp(1.0 - abs(temp-6000.0)/6000.0, 0.0, 1.0),
                  clamp((temp-2000.0)/10000.0,          0.0, 1.0));
}

float3 starField(float3 p, float time) {
    float3 cell = floor(p);
    float  h    = fract(sin(dot(cell, float3(12.3,45.6,78.9))) * 43758.5453);
    if (h > 0.996) {
        float d = length(fract(p) - 0.5);
        return starColor(fract(h*10.0)) * exp(-d*40.0)
             * pow(fract(h*100.0), 5.0) * starTwinkle(h, time) * 5.0;
    }
    return float3(0.0);
}

// galaxy
float galaxyDensity(float3 p, float time) {
    float r = length(p.xy), theta = atan2(p.y, p.x);
    float arms = (cos(theta*3.0 - r*6.0 + time) + cos(theta*5.0 + r*4.0 - time*0.5)) * 0.5;
    return smoothstep(0.2, 1.0, arms + fbm(p*2.0+time)) * exp(-r*1.5) * exp(-abs(p.z)*4.0);
}

float3 raymarch(float3 ro, float3 rd, float time) {
    float t = 0.0; float3 col = float3(0.0); float acc = 0.0;
    for (int i = 0; i < 64; i++) {
        float3 pos = animateGalaxy(ro + rd*t, time);
        float  d   = galaxyDensity(pos, time);
        float  a   = d * 0.08;
        col += mix(float3(0.2,0.1,0.4), float3(0.8,0.6,1.0), d) * a * (1.0-acc);
        acc += a * (1.0-acc);
        col += starField(pos*10.0, time) * 0.02;
        if (acc > 0.98) break;
        t += 0.05;
    }
    return col;
}

// post
float3 ACES(float3 x) {
    return clamp((x*(2.51*x+0.03))/(x*(2.43*x+0.59)+0.14), 0.0, 1.0);
}

// black hole warp
float2 blackHoleWarp(float2 uv, float2 center, float strength) {
    float2 d = uv - center;
    return uv + normalize(d) * (strength / (length(d) + 0.05)) * 0.02;
}

// fragment entry
fragment float4 fs_main(Varyings in [[stage_in]],
                        constant EffectUniforms& u [[buffer(0)]]) {
    float2 uv = in.uv * 2.0 - 1.0;
    uv.x *= u.iResolution.x / u.iResolution.y;

    float time = u.iTime * 0.2;
    float3 ro = float3(sin(time*0.2)*0.5, cos(time*0.15)*0.3, -3.0 + sin(time*0.1)*0.2);

    uv = blackHoleWarp(uv, float2(0.0), 0.5);
    float3 rd = normalize(float3(uv, 1.5));

    float3 col = raymarch(ro, rd, time);
    col = pow(ACES(col), float3(0.4545));
    return float4(col, 1.0);
}
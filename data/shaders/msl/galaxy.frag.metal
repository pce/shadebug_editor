#include <metal_stdlib>
using namespace metal;

// ============================================================
// UNIFORMS
// ============================================================
struct EffectUniforms {
    float2 iResolution;
    float  iTime;
    float  _pad;
};

struct Varyings {
    float4 pos [[position]];
    float2 uv;
};


// ============================================================
// ANIMATION MODULE
// ============================================================

// Rotate around Z axis (galaxy plane)
float2 rotate2D(float2 p, float a) {
    float c = cos(a);
    float s = sin(a);
    return float2(c*p.x - s*p.y, s*p.x + c*p.y);
}

// Differential galaxy rotation (core faster)
float3 animateGalaxy(float3 p, float time) {
    float r = length(p.xy);

    // inner rotates faster than outer
    float speed = 0.4 / (r + 0.2);

    float angle = time * speed;

    p.xy = rotate2D(p.xy, angle);
    return p;
}

// Star twinkle (separate concern!)
float starTwinkle(float h, float time) {
    return 0.8 + 0.4 * sin(time * 8.0 + h * 100.0);
}

// ============================================================
// HASH / NOISE
// ============================================================

static float hash1(float3 p) {
    return fract(sin(dot(p, float3(127.1,311.7,74.7))) * 43758.5453);
}

static float noise(float3 p) {
    float3 i = floor(p);
    float3 f = fract(p);
    f = f * f * (3.0 - 2.0 * f);

    float n = mix(mix(mix(hash1(i + float3(0,0,0)), hash1(i + float3(1,0,0)), f.x),
                      mix(hash1(i + float3(0,1,0)), hash1(i + float3(1,1,0)), f.x), f.y),
                  mix(mix(hash1(i + float3(0,0,1)), hash1(i + float3(1,0,1)), f.x),
                      mix(hash1(i + float3(0,1,1)), hash1(i + float3(1,1,1)), f.x), f.y),
                  f.z);
    return n;
}

static float fbm(float3 p) {
    float v = 0.0;
    float a = 0.5;
    for(int i=0;i<5;i++){
        v += noise(p) * a;
        p *= 2.0;
        a *= 0.5;
    }
    return v;
}

// ============================================================
// STAR COLOR TEMPERATURE (Kelvin → RGB approx)
// ============================================================

float3 starColor(float t) {
    // t ~ 0..1 mapped to temperature
    float temp = mix(2000.0, 12000.0, t);

    float3 col;

    col.r = clamp(1.0, 0.0, 1.0);
    col.g = clamp(1.0 - abs(temp - 6000.0)/6000.0, 0.0, 1.0);
    col.b = clamp((temp - 2000.0)/10000.0, 0.0, 1.0);

    return col;
}

// ============================================================
// BLACK HOLE LENSING (screen-space warp)
// ============================================================

float2 blackHoleWarp(float2 uv, float2 center, float strength) {
    float2 d = uv - center;
    float r = length(d);

    float warp = strength / (r + 0.05);
    return uv + normalize(d) * warp * 0.02;
}

// ============================================================
// GALAXY DENSITY FIELD (3D)
// ============================================================

float galaxyDensity(float3 p, float time) {

    float r = length(p.xy);
    float theta = atan2(p.y, p.x);

    // 🌀 multiple arm layers
    float arms1 = cos(theta * 3.0 - r * 6.0 + time);
    float arms2 = cos(theta * 5.0 + r * 4.0 - time * 0.5);

    float arms = (arms1 + arms2) * 0.5;

    // turbulence
    float n = fbm(p * 2.0 + time);

    float density = smoothstep(0.2, 1.0, arms + n);

    // radial falloff
    density *= exp(-r * 1.5);

    // vertical thickness
    density *= exp(-abs(p.z) * 4.0);

    return density;
}

// ============================================================
// RAYMARCHING (VOLUMETRIC)
// ============================================================

float3 raymarch(float3 ro, float3 rd, float time) {

    float t = 0.0;
    float3 col = float3(0.0);
    float acc = 0.0;

    for(int i=0;i<64;i++) {

        float3 pos = ro + rd * t;

        float d = galaxyDensity(pos, time);

        // emissive color
        float3 c = mix(float3(0.2,0.1,0.4), float3(0.8,0.6,1.0), d);

        // accumulate (front-to-back)
        float alpha = d * 0.08;
        col += c * alpha * (1.0 - acc);
        acc += alpha * (1.0 - acc);

        if(acc > 0.98) break;

        t += 0.05;
    }

    return col;
}

// ============================================================
// STAR FIELD (3D)
// ============================================================
float3 starField(float3 p, float time) {

    float3 col = float3(0.0);

    float3 cell = floor(p);
    float h = hash1(cell);

    if(h > 0.996) {

        float3 local = fract(p) - 0.5;
        float d = length(local);

        float glow = exp(-d * 40.0);

        float brightness = pow(fract(h * 100.0), 5.0);

        // animation injected here
        brightness *= starTwinkle(h, time);

        float temp = fract(h * 10.0);

        col = starColor(temp) * glow * brightness * 5.0;
    }

    return col;
}
// ============================================================
// ACES FILMIC TONEMAPPING
// ============================================================

float3 ACES(float3 x) {
    float a = 2.51;
    float b = 0.03;
    float c = 2.43;
    float d = 0.59;
    float e = 0.14;
    return clamp((x*(a*x+b))/(x*(c*x+d)+e), 0.0, 1.0);
}

// ============================================================
// MAIN FRAGMENT
// ============================================================

fragment float4 fs_main(Varyings in [[stage_in]],
                        constant EffectUniforms& u [[buffer(0)]]) {

    float2 uv = in.uv * 2.0 - 1.0;
    uv.x *= u.iResolution.x / u.iResolution.y;

    float time = u.iTime * 0.2;

    // black hole center
    float2 bhCenter = float2(0.0, 0.0);
    uv = blackHoleWarp(uv, bhCenter, 0.5);

    // camera
    float3 ro = float3(0.0, 0.0, -3.0);
    float3 rd = normalize(float3(uv, 1.5));

    // galaxy volume
    float3 col = raymarch(ro, rd, time);

    // stars
    col += starField(rd, time);

    // tone mapping
    col = ACES(col);

    // gamma correction
    col = pow(col, float3(0.4545));

    return float4(col, 1.0);
}
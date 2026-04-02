#pragma once
#include <metal_stdlib>
using namespace metal;

struct EffectUniforms {
    float2 iResolution;
    float  iTime;
    float  _pad;
};

struct Varyings {
    float4 pos [[position]];
    float2 uv;
};

// reusable math
float2 rotate2D(float2 p, float a) {
    float c = cos(a);
    float s = sin(a);
    return float2(c*p.x - s*p.y, s*p.x + c*p.y);
}
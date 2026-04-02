#pragma once
#include <metal_stdlib>
using namespace metal;

float3 starColor(float t) {
    float temp = mix(2000.0, 12000.0, t);

    float3 col;
    col.r = 1.0;
    col.g = clamp(1.0 - abs(temp - 6000.0)/6000.0, 0.0, 1.0);
    col.b = clamp((temp - 2000.0)/10000.0, 0.0, 1.0);

    return col;
}

float3 starField(float3 p, float time) {

    float3 col = float3(0.0);

    float3 cell = floor(p);
    float h = fract(sin(dot(cell, float3(12.3,45.6,78.9))) * 43758.5453);

    if(h > 0.996) {
        float3 local = fract(p) - 0.5;
        float d = length(local);

        float glow = exp(-d * 40.0);
        float brightness = pow(fract(h * 100.0), 5.0);

        brightness *= starTwinkle(h, time);

        float temp = fract(h * 10.0);

        col = starColor(temp) * glow * brightness * 5.0;
    }

    return col;
}
#pragma once
#include <metal_stdlib>
using namespace metal;

float3 animateGalaxy(float3 p, float time) {
    float r = length(p.xy);
    float speed = 0.4 / (r + 0.2);
    float angle = time * speed;

    float c = cos(angle);
    float s = sin(angle);

    p.xy = float2(c*p.x - s*p.y, s*p.x + c*p.y);
    return p;
}

float starTwinkle(float h, float time) {
    return 0.8 + 0.4 * sin(time * 8.0 + h * 100.0);
}
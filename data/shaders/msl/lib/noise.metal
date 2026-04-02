#pragma once
#include <metal_stdlib>
using namespace metal;

float hash1(float3 p) {
    return fract(sin(dot(p, float3(127.1,311.7,74.7))) * 43758.5453);
}

float noise(float3 p) {
    float3 i = floor(p);
    float3 f = fract(p);
    f = f * f * (3.0 - 2.0 * f);

    return mix(mix(mix(hash1(i + float3(0,0,0)), hash1(i + float3(1,0,0)), f.x),
                   mix(hash1(i + float3(0,1,0)), hash1(i + float3(1,1,0)), f.x), f.y),
               mix(mix(hash1(i + float3(0,0,1)), hash1(i + float3(1,0,1)), f.x),
                   mix(hash1(i + float3(0,1,1)), hash1(i + float3(1,1,1)), f.x), f.y),
               f.z);
}

float fbm(float3 p) {
    float v = 0.0;
    float a = 0.5;
    for(int i=0;i<5;i++){
        v += noise(p) * a;
        p *= 2.0;
        a *= 0.5;
    }
    return v;
}
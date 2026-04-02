#pragma once
#include <metal_stdlib>
using namespace metal;

float galaxyDensity(float3 p, float time) {

    float r = length(p.xy);
    float theta = atan2(p.y, p.x);

    float arms1 = cos(theta * 3.0 - r * 6.0 + time);
    float arms2 = cos(theta * 5.0 + r * 4.0 - time * 0.5);

    float arms = (arms1 + arms2) * 0.5;

    float n = fbm(p * 2.0 + time);

    float density = smoothstep(0.2, 1.0, arms + n);

    density *= exp(-r * 1.5);
    density *= exp(-abs(p.z) * 4.0);

    return density;
}

float3 raymarch(float3 ro, float3 rd, float time) {

    float t = 0.0;
    float3 col = float3(0.0);
    float acc = 0.0;

    for(int i=0;i<64;i++) {

        float3 pos = ro + rd * t;
        float3 animPos = animateGalaxy(pos, time);

        float d = galaxyDensity(animPos, time);

        float3 c = mix(float3(0.2,0.1,0.4),
                       float3(0.8,0.6,1.0), d);

        float alpha = d * 0.08;

        col += c * alpha * (1.0 - acc);
        acc += alpha * (1.0 - acc);

        col += starField(animPos * 10.0, time) * 0.02;

        if(acc > 0.98) break;

        t += 0.05;
    }

    return col;
}
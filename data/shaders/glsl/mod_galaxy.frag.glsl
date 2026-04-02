#version 330 core
uniform vec2 iResolution;
uniform float iTime;

in vec2 v_uv;
out vec4 fragColor;

#include "lib/common.glsl"

// MODULAR GALAXY

vec3 renderGalaxy(vec2 uv, float time){

    vec3 ro = vec3(0.0, 0.0, -3.0);
    vec3 rd = normalize(vec3(uv, 1.5));

    // optional modules
    rd = bendRay(ro, rd, vec3(0.0), 0.6);

    return raymarch(ro, rd, time);
}

void main(){

    vec2 uv = v_uv * 2.0 - 1.0;
    uv.x *= iResolution.x / iResolution.y;

    float time = iTime * 0.2;

    vec3 col = renderGalaxy(uv, time);

    col = ACES(col);
    col = pow(col, vec3(0.4545));

    fragColor = vec4(col, 1.0);
}
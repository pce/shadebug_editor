#version 330 core
uniform vec2 iResolution;
uniform float iTime;

in vec2 v_uv;
out vec4 fragColor;

#include "lib/common.glsl"


// MAIN GALAXY

void main(){

    // --------------------------------------------------------
    // UV → NDC
    // --------------------------------------------------------
    vec2 uv = v_uv * 2.0 - 1.0;
    uv.x *= iResolution.x / iResolution.y;

    float time = iTime * 0.2;

    // --------------------------------------------------------
    // CAMERA (animated orbit)
    // --------------------------------------------------------
    vec3 ro = vec3(
        sin(time * 0.2) * 0.5,
        cos(time * 0.15) * 0.3,
        -3.0 + sin(time * 0.1) * 0.2
    );

    vec3 rd = normalize(vec3(uv, 1.5));

    // --------------------------------------------------------
    // PHYSICALLY-INSPIRED BLACK HOLE
    // --------------------------------------------------------
    vec3 bhPos = vec3(0.0);
    rd = bendRay(ro, rd, bhPos, 0.8);

    // --------------------------------------------------------
    // RENDER GALAXY (VOLUMETRIC)
    // --------------------------------------------------------
    vec3 col = raymarch(ro, rd, time);

    // --------------------------------------------------------
    // FILMIC POST
    // --------------------------------------------------------
    col = ACES(col);
    col = pow(col, vec3(0.4545));

    fragColor = vec4(col, 1.0);
}
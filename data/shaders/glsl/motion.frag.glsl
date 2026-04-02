#version 330 core
uniform vec2  iResolution;
uniform float iTime;
in  vec2 v_uv;
out vec4 fragColor;

void main() {
    vec2 uv = v_uv;

    float freq1 = 6.0;
    float freq2 = 4.0;
    vec2 d1 = vec2(sin(iTime * 0.4 + uv.y * freq1), cos(iTime * 0.3 + uv.x * freq1)) * 0.08;
    vec2 d2 = vec2(cos(iTime * 0.5 + uv.x * freq2), sin(iTime * 0.6 + uv.y * freq2)) * 0.06;

    vec2 uv1 = uv + d1;
    vec2 uv2 = vec2(1.0 - uv.y, uv.x) + d2;

    float r1 = length(uv1 - 0.5);
    float r2 = length(uv2 - 0.5);

    float rings1 = sin(r1 * 25.0 - iTime * 2.0) * 0.5 + 0.5;
    float rings2 = sin(r2 * 20.0 + iTime * 1.5) * 0.5 + 0.5;

    vec3 col1 = mix(vec3(0.1, 0.3, 0.9), vec3(0.9, 0.2, 0.6), rings1);
    vec3 col2 = mix(vec3(0.1, 0.8, 0.5), vec3(0.9, 0.6, 0.1), rings2);
    vec3 col = mix(col1, col2, 0.5);

    fragColor = vec4(col, 1.0);
}

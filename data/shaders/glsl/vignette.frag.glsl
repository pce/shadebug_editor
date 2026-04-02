#version 330 core
uniform vec2  iResolution;
uniform float iTime;
in  vec2 v_uv;
out vec4 fragColor;
void main() {
    vec2 uv = v_uv;
    float t = iTime * 0.5;
    vec3 col_a = vec3(0.25, 0.05, 0.55);
    vec3 col_b = vec3(0.05, 0.35, 0.65);
    float blend = sin(t + uv.x * 2.0 + uv.y * 1.5) * 0.5 + 0.5;
    vec3 grad = mix(col_a, col_b, blend);
    float pulse = sin(iTime * 1.2) * 0.1 + 0.9;
    vec2 center = uv - 0.5;
    float dist = length(center);
    float vignette = smoothstep(0.7, 0.2, dist * pulse);
    vec3 col = grad * vignette;
    fragColor = vec4(col, 1.0);
}

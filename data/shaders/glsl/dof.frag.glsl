#version 330 core
uniform vec2  iResolution;
uniform float iTime;
in  vec2 v_uv;
out vec4 fragColor;

float bokeh(vec2 uv, vec2 center, float radius, float blur) {
    float d = length(uv - center);
    return smoothstep(radius + blur, radius - blur, d);
}

void main() {
    vec2 uv = v_uv;
    vec3 col = vec3(0.02, 0.02, 0.05);

    vec2 centers[8];
    centers[0] = vec2(0.20, 0.30);
    centers[1] = vec2(0.70, 0.20);
    centers[2] = vec2(0.50, 0.75);
    centers[3] = vec2(0.15, 0.80);
    centers[4] = vec2(0.85, 0.60);
    centers[5] = vec2(0.40, 0.45);
    centers[6] = vec2(0.60, 0.55);
    centers[7] = vec2(0.30, 0.65);

    vec3 colors[8];
    colors[0] = vec3(1.0, 0.6, 0.2);
    colors[1] = vec3(0.2, 0.8, 1.0);
    colors[2] = vec3(1.0, 0.3, 0.5);
    colors[3] = vec3(0.5, 1.0, 0.3);
    colors[4] = vec3(0.9, 0.9, 0.2);
    colors[5] = vec3(0.6, 0.3, 1.0);
    colors[6] = vec3(1.0, 0.7, 0.5);
    colors[7] = vec3(0.3, 0.9, 0.8);

    float depths[8];
    depths[0] = 0.2;
    depths[1] = 0.8;
    depths[2] = 0.5;
    depths[3] = 0.1;
    depths[4] = 0.9;
    depths[5] = 0.4;
    depths[6] = 0.6;
    depths[7] = 0.3;

    float focal = 0.5 + sin(iTime * 0.3) * 0.4;

    for (int i = 0; i < 8; i++) {
        vec2 c = centers[i];
        c.x = fract(c.x + iTime * (0.03 + float(i) * 0.005));
        c.y = fract(c.y + iTime * (0.02 + float(i) * 0.003));
        float depth = depths[i];
        float coc = abs(depth - focal) * 0.15;
        float blur = max(0.002, coc);
        float radius = 0.015 + coc * 0.5;
        float b = bokeh(uv, c, radius, blur);
        col += colors[i] * b * (1.0 - coc * 2.0);
    }

    fragColor = vec4(col, 1.0);
}

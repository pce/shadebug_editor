#version 330 core
uniform vec2  iResolution;
uniform float iTime;
in  vec2 v_uv;
out vec4 fragColor;

vec2 hash2(vec2 p) {
    p = vec2(dot(p, vec2(127.1, 311.7)), dot(p, vec2(269.5, 183.3)));
    return fract(sin(p) * 43758.5453);
}

float gnoise(vec2 p) {
    vec2 i = floor(p);
    vec2 f = fract(p);
    vec2 u = f * f * (3.0 - 2.0 * f);
    float a = dot(hash2(i              ) * 2.0 - 1.0, f            );
    float b = dot(hash2(i + vec2(1,0)  ) * 2.0 - 1.0, f - vec2(1,0));
    float c = dot(hash2(i + vec2(0,1)  ) * 2.0 - 1.0, f - vec2(0,1));
    float d = dot(hash2(i + vec2(1,1)  ) * 2.0 - 1.0, f - vec2(1,1));
    return mix(mix(a, b, u.x), mix(c, d, u.x), u.y);
}

float fbm(vec2 p, int oct) {
    float v = 0.0;
    float amp = 0.5;
    for (int i = 0; i < oct; i++) {
        v += gnoise(p) * amp;
        p *= 2.0;
        amp *= 0.5;
    }
    return v;
}

void main() {
    vec2 uv = v_uv;
    float t = iTime * 0.1;

    if (uv.x < 0.33) {
        float n = gnoise(uv * 5.0 + t);
        fragColor = vec4(vec3(n * 0.5 + 0.5), 1.0);
    } else if (uv.x < 0.66) {
        vec2 p = uv * 4.0 + t;
        float n = fbm(p, 4) * 0.5 + 0.5;
        vec3 col = mix(vec3(0.1, 0.3, 0.6), vec3(0.8, 0.9, 1.0), n);
        fragColor = vec4(col, 1.0);
    } else {
        vec2 p = uv * 3.0;
        vec2 q = vec2(fbm(p + t, 4), fbm(p + vec2(1.7, 9.2) + t, 4));
        float n = fbm(p + 4.0 * q, 4) * 0.5 + 0.5;
        vec3 col = mix(vec3(0.5, 0.2, 0.1), vec3(1.0, 0.85, 0.5), n);
        fragColor = vec4(col, 1.0);
    }
}

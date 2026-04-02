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
    vec2 scroll1 = iTime * vec2(0.10, 0.07);
    vec2 scroll2 = iTime * vec2(-0.08, -0.05);

    vec2 p1 = uv * 4.0 + scroll1;
    vec2 p2 = uv * 3.5 + scroll2 + vec2(5.2, 1.3);

    vec2 q = vec2(fbm(p1, 4), fbm(p1 + vec2(1.7, 9.2), 4));
    vec2 r = vec2(fbm(p2 + 4.0*q, 4), fbm(p2 + 4.0*q + vec2(8.3, 2.8), 4));

    float n = fbm(uv * 3.0 + 4.0 * r + scroll1, 4) * 0.5 + 0.5;

    vec3 deep   = vec3(0.05, 0.15, 0.45);
    vec3 mid    = vec3(0.10, 0.55, 0.65);
    vec3 bright = vec3(0.50, 0.95, 0.85);

    vec3 col = mix(deep, mid, smoothstep(0.2, 0.55, n));
    col = mix(col, bright, smoothstep(0.55, 0.85, n));

    fragColor = vec4(col, 1.0);
}

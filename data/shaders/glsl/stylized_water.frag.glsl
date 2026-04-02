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
    float a = dot(hash2(i            ) * 2.0 - 1.0, f            );
    float b = dot(hash2(i + vec2(1,0)) * 2.0 - 1.0, f - vec2(1,0));
    float c = dot(hash2(i + vec2(0,1)) * 2.0 - 1.0, f - vec2(0,1));
    float d = dot(hash2(i + vec2(1,1)) * 2.0 - 1.0, f - vec2(1,1));
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
    float t = iTime * 0.5;

    // Animated wave distortion
    vec2 wuv = uv;
    wuv.x += fbm(vec2(uv.x * 3.0, uv.y * 2.0 + t * 0.4), 4) * 0.12;
    wuv.y += fbm(vec2(uv.x * 2.5 + t * 0.3, uv.y * 3.0), 4) * 0.10;

    // Wave height
    float wave = fbm(wuv * 5.0 + vec2(t * 0.6, 0.0), 4) * 0.5 + 0.5;
    float wave2 = fbm(wuv * 4.0 + vec2(-t * 0.4, t * 0.2), 3) * 0.5 + 0.5;
    float h = mix(wave, wave2, 0.4);

    // Color: deep blue troughs, bright cyan-white crests
    vec3 trough = vec3(0.02, 0.08, 0.30);
    vec3 crest  = vec3(0.60, 0.95, 1.00);
    vec3 col = mix(trough, crest, smoothstep(0.35, 0.75, h));

    // Foam lines near crests
    float foam = smoothstep(0.70, 0.80, h) * smoothstep(0.90, 0.80, h);
    col = mix(col, vec3(1.0), foam * 0.8);

    // Sky reflection gradient at top
    float sky = smoothstep(0.7, 1.0, uv.y);
    vec3 skyCol = vec3(0.45, 0.75, 0.95);
    col = mix(col, skyCol, sky * 0.4);

    fragColor = vec4(col, 1.0);
}

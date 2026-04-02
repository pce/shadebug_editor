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
    vec2 i = floor(p); vec2 f = fract(p);
    vec2 u = f*f*(3.0-2.0*f);
    float a = dot(hash2(i            )*2.0-1.0, f           );
    float b = dot(hash2(i+vec2(1,0)  )*2.0-1.0, f-vec2(1,0));
    float c = dot(hash2(i+vec2(0,1)  )*2.0-1.0, f-vec2(0,1));
    float d = dot(hash2(i+vec2(1,1)  )*2.0-1.0, f-vec2(1,1));
    return mix(mix(a,b,u.x), mix(c,d,u.x), u.y);
}
float fbm(vec2 p, int oct) {
    float v=0.0; float amp=0.5;
    for(int i=0;i<oct;i++){v+=gnoise(p)*amp; p*=2.0; amp*=0.5;}
    return v;
}

void main() {
    vec2 uv = v_uv;
    // Perspective warp: compress top
    float horizon = 0.55;
    float sky_mask = step(horizon, uv.y);
    float sea_y = (uv.y < horizon) ? uv.y / horizon : 0.0;

    vec2 seaUV = vec2(uv.x, sea_y);
    float t = iTime * 0.3;

    // Two wave directions at 45 degrees
    vec2 dir1 = vec2(0.707, 0.707);
    vec2 dir2 = vec2(-0.707, 0.707);

    float w1 = fbm(seaUV * 6.0 + dir1 * t, 6) * 0.5 + 0.5;
    float w2 = fbm(seaUV * 5.0 + dir2 * t * 0.8, 5) * 0.5 + 0.5;
    float h = (w1 + w2) * 0.5;

    // Fresnel: more reflective at grazing angle (toward horizon)
    float fresnel = pow(1.0 - sea_y, 2.0);

    vec3 deep  = vec3(0.01, 0.05, 0.20);
    vec3 mid   = vec3(0.05, 0.25, 0.55);
    vec3 light = vec3(0.50, 0.85, 1.00);
    vec3 seaCol = mix(deep, mid, h);
    seaCol = mix(seaCol, light, fresnel * 0.5);

    // Foam whitecaps
    float foam = smoothstep(0.62, 0.75, h) * (1.0 - fresnel);
    seaCol = mix(seaCol, vec3(1.0), foam);

    // Sky gradient
    vec3 skyLow  = vec3(0.60, 0.82, 0.98);
    vec3 skyHigh = vec3(0.10, 0.30, 0.70);
    float skyT = (uv.y - horizon) / (1.0 - horizon + 0.001);
    vec3 skyCol = mix(skyLow, skyHigh, clamp(skyT, 0.0, 1.0));

    vec3 col = mix(seaCol, skyCol, sky_mask);
    fragColor = vec4(col, 1.0);
}

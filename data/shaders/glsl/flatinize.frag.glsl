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
    float t = iTime * 0.08;
    float n = fbm(uv * 4.0 + t, 3) * 0.5 + 0.5;

    // Quantize into 5 bands
    float bands = 5.0;
    float q = floor(n * bands) / bands;
    float blend = smoothstep(0.0, 0.2 / bands, fract(n * bands));
    float qn = mix(q, q + 1.0/bands, blend * 0.7);

    vec3 c0 = vec3(0.15, 0.55, 0.90);
    vec3 c1 = vec3(0.25, 0.80, 0.55);
    vec3 c2 = vec3(0.95, 0.85, 0.25);
    vec3 c3 = vec3(0.95, 0.45, 0.20);
    vec3 c4 = vec3(0.70, 0.20, 0.55);

    vec3 col;
    if (qn < 0.2)       col = mix(c0, c1, qn * 5.0);
    else if (qn < 0.4)  col = mix(c1, c2, (qn-0.2)*5.0);
    else if (qn < 0.6)  col = mix(c2, c3, (qn-0.4)*5.0);
    else if (qn < 0.8)  col = mix(c3, c4, (qn-0.6)*5.0);
    else                col = c4;

    fragColor = vec4(col, 1.0);
}

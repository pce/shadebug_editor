#version 330 core
uniform vec2  iResolution;
uniform float iTime;
in  vec2 v_uv;
out vec4 fragColor;

float hash(vec2 p) {
    return fract(sin(dot(p, vec2(127.1, 311.7))) * 43758.5453);
}
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
    // Slow rotation
    float angle = iTime * 0.02;
    float cs = cos(angle); float sn = sin(angle);
    vec2 ruv = vec2(cs*uv.x - sn*uv.y, sn*uv.x + cs*uv.y);

    // Deep space background
    vec3 col = vec3(0.0, 0.0, 0.02);

    // Starfield with twinkle
    vec2 suv = ruv * 80.0;
    vec2 sid = floor(suv);
    float h = hash(sid);
    float twinkle = sin(iTime * (2.0 + h * 5.0)) * 0.5 + 0.5;
    float star = step(0.985, h) * twinkle;
    col += vec3(star);

    // Nebula: 3 independent fbm layers for RGB
    vec2 p = ruv * 3.0 + iTime * 0.04;
    float nr = fbm(p              , 5) * 0.5 + 0.5;
    float ng = fbm(p + vec2(1.7, 9.2), 5) * 0.5 + 0.5;
    float nb = fbm(p + vec2(8.3, 2.8), 5) * 0.5 + 0.5;

    // Cluster regions modulate nebula brightness
    float cluster = smoothstep(0.4, 0.7, fbm(ruv*2.0, 4)*0.5+0.5);

    vec3 nebula = vec3(nr, ng, nb) * cluster * 0.55;
    col += nebula;
    col = clamp(col, 0.0, 1.0);
    fragColor = vec4(col, 1.0);
}

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
vec3 posterize(vec2 uv, float t) {
    float n = fbm(uv * 4.0 + t, 3) * 0.5 + 0.5;
    float bands = 5.0;
    float q = floor(n * bands) / bands;
    vec3 c0 = vec3(0.15, 0.55, 0.90); vec3 c1 = vec3(0.25, 0.80, 0.55);
    vec3 c2 = vec3(0.95, 0.85, 0.25); vec3 c3 = vec3(0.95, 0.45, 0.20);
    vec3 c4 = vec3(0.70, 0.20, 0.55);
    if      (q < 0.2) return mix(c0,c1, q*5.0);
    else if (q < 0.4) return mix(c1,c2, (q-0.2)*5.0);
    else if (q < 0.6) return mix(c2,c3, (q-0.4)*5.0);
    else if (q < 0.8) return mix(c3,c4, (q-0.6)*5.0);
    else              return c4;
}
float luma(vec3 c) { return dot(c, vec3(0.299, 0.587, 0.114)); }

void main() {
    vec2 uv = v_uv;
    vec2 px = 1.0 / iResolution;
    float t = iTime * 0.08;

    vec3 base = posterize(uv, t);

    // Sobel on luma of posterized base
    float tl = luma(posterize(uv+vec2(-px.x, px.y),t));
    float tc = luma(posterize(uv+vec2(0,     px.y),t));
    float tr = luma(posterize(uv+vec2( px.x, px.y),t));
    float ml = luma(posterize(uv+vec2(-px.x, 0   ),t));
    float mr = luma(posterize(uv+vec2( px.x, 0   ),t));
    float bl = luma(posterize(uv+vec2(-px.x,-px.y),t));
    float bc = luma(posterize(uv+vec2(0,    -px.y),t));
    float br = luma(posterize(uv+vec2( px.x,-px.y),t));
    float gx = -tl-2.0*ml-bl+tr+2.0*mr+br;
    float gy = -tl-2.0*tc-tr+bl+2.0*bc+br;
    float edge = smoothstep(0.05, 0.3, sqrt(gx*gx+gy*gy));

    vec3 col = mix(base, vec3(0.05), edge);
    fragColor = vec4(col, 1.0);
}

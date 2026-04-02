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
    float t = iTime * 0.12;

    // Domain warping for cloud shapes
    vec2 q = vec2(fbm(uv*3.0 + t, 6), fbm(uv*3.0 + vec2(5.2, 1.3) + t*0.8, 6));
    float n = fbm(uv*2.5 + 2.5*q + t*0.5, 6) * 0.5 + 0.5;

    // Sky blue background
    vec3 sky = mix(vec3(0.40, 0.65, 0.95), vec3(0.20, 0.45, 0.80), uv.y);
    // Cloud layers: white-cream centers, soft grey edges
    vec3 cloudEdge   = vec3(0.75, 0.78, 0.82);
    vec3 cloudCenter = vec3(0.98, 0.97, 0.95);

    float cloudMask = smoothstep(0.45, 0.80, n);
    vec3 cloud = mix(cloudEdge, cloudCenter, smoothstep(0.60, 0.85, n));

    // Rim lighting — slight warm tint at cloud edges
    float rim = smoothstep(0.44, 0.55, n) * (1.0 - smoothstep(0.55, 0.75, n));
    cloud += vec3(0.15, 0.10, 0.05) * rim;

    vec3 col = mix(sky, cloud, cloudMask);
    fragColor = vec4(col, 1.0);
}

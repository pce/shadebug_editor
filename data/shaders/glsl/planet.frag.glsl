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
float hash1(vec2 p) { return fract(sin(dot(p, vec2(127.1,311.7)))*43758.5453); }

void main() {
    vec2 uv = v_uv;
    float t = iTime * 0.05;

    // Planet disc
    vec2 center = vec2(0.5, 0.5);
    float radius = 0.30;
    vec2 delta = uv - center;
    float dist = length(delta);

    vec3 col = vec3(0.0, 0.0, 0.02);

    // Starfield background
    float h = hash1(floor(uv * 120.0));
    float star = step(0.985, h) * (sin(iTime * (2.0 + h * 6.0)) * 0.5 + 0.5);
    col += vec3(star * 0.9);

    if (dist < radius) {
        // UV on sphere surface
        vec2 sUV = delta / radius;
        float z = sqrt(max(0.0, 1.0 - dot(sUV, sUV)));

        // Rotate surface slowly
        vec2 rotUV = sUV;
        float ra = iTime * 0.06;
        rotUV.x = cos(ra)*sUV.x - sin(ra)*sUV.y;
        rotUV.y = sin(ra)*sUV.x + cos(ra)*sUV.y;

        // Surface fbm — continents / ocean
        float terrain = fbm(rotUV * 5.0 + 2.0, 5) * 0.5 + 0.5;
        vec3 ocean    = vec3(0.05, 0.20, 0.55);
        vec3 land     = vec3(0.25, 0.55, 0.20);
        vec3 sand     = vec3(0.75, 0.65, 0.35);
        vec3 surface  = mix(ocean, land,  smoothstep(0.40, 0.55, terrain));
        surface       = mix(surface, sand, smoothstep(0.55, 0.65, terrain));

        // Polar caps
        float polar = smoothstep(0.6, 0.78, abs(sUV.y));
        surface = mix(surface, vec3(0.9, 0.95, 1.0), polar);

        // Diffuse lighting from upper-left
        vec3 lightDir = normalize(vec3(-0.5, 0.6, 0.8));
        vec3 normal   = normalize(vec3(sUV, z));
        float diff = max(0.0, dot(normal, lightDir));
        surface *= (0.2 + 0.8 * diff);

        col = surface;

        // Soft atmosphere rim
        float rim = 1.0 - z;
        rim = pow(rim, 3.0);
        vec3 atm = vec3(0.3, 0.6, 1.0);
        col = mix(col, atm, rim * 0.6);
    } else {
        // Atmosphere glow just outside disc
        float atmDist = dist - radius;
        float atmGlow = exp(-atmDist * 20.0) * 0.4;
        col += vec3(0.2, 0.4, 0.9) * atmGlow;
    }

    fragColor = vec4(col, 1.0);
}

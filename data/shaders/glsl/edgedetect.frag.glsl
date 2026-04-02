#version 330 core
uniform vec2  iResolution;
uniform float iTime;
in  vec2 v_uv;
out vec4 fragColor;

float scene(vec2 uv) {
    float checker = mod(floor(uv.x * 10.0) + floor(uv.y * 10.0), 2.0);
    float wave = sin(uv.x * 20.0 + iTime) * sin(uv.y * 20.0 + iTime * 0.7) * 0.5 + 0.5;
    return mix(checker, wave, 0.5);
}

void main() {
    vec2 uv = v_uv;
    vec2 px = 1.0 / iResolution;

    float tl = scene(uv + vec2(-px.x,  px.y));
    float tc = scene(uv + vec2( 0.0,   px.y));
    float tr = scene(uv + vec2( px.x,  px.y));
    float ml = scene(uv + vec2(-px.x,  0.0));
    float mr = scene(uv + vec2( px.x,  0.0));
    float bl = scene(uv + vec2(-px.x, -px.y));
    float bc = scene(uv + vec2( 0.0,  -px.y));
    float br = scene(uv + vec2( px.x, -px.y));

    float gx = -tl - 2.0*ml - bl + tr + 2.0*mr + br;
    float gy = -tl - 2.0*tc - tr + bl + 2.0*bc + br;
    float edge = sqrt(gx*gx + gy*gy);

    vec3 edgeColor = mix(vec3(0.0), vec3(0.95, 0.75, 0.2), smoothstep(0.1, 0.4, edge));
    fragColor = vec4(edgeColor, 1.0);
}

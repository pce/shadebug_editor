#version 330 core
// ═══════════════════════════════════════════════════════════════════════════════
//  TOPOGRAPHIC SHADER (GLSL)  — depthmap preprocessing → cartographic vis.
//  See topo.frag.metal for full documentation.
// ═══════════════════════════════════════════════════════════════════════════════

uniform vec2  iResolution;
uniform float iTime;
// ParamUniforms — same layout as Metal
uniform vec4  iParams0;   // slot 0-3: mode, scale, light_az, light_el
uniform vec4  iParams1;   // slot 4-7: num_bands, line_sharp, _, animate

in  vec2 v_uv;
out vec4 fragColor;

// ── Procedural depthmap ───────────────────────────────────────────────────────
vec2 _hash2(vec2 p) {
    p = vec2(dot(p, vec2(127.1, 311.7)), dot(p, vec2(269.5, 183.3)));
    return fract(sin(p) * 43758.5453);
}
float _gnoise(vec2 p) {
    vec2 i = floor(p), f = fract(p);
    vec2 u = f*f*(3.0-2.0*f);
    float a = dot(_hash2(i          )*2.0-1.0, f          );
    float b = dot(_hash2(i+vec2(1,0))*2.0-1.0, f-vec2(1,0));
    float c = dot(_hash2(i+vec2(0,1))*2.0-1.0, f-vec2(0,1));
    float d = dot(_hash2(i+vec2(1,1))*2.0-1.0, f-vec2(1,1));
    return mix(mix(a,b,u.x), mix(c,d,u.x), u.y);
}
float _fbm(vec2 p, int oct) {
    float v = 0.0, amp = 0.5;
    for (int i = 0; i < oct; ++i) { v += _gnoise(p)*amp; p *= 2.0; amp *= 0.5; }
    return v * 0.5 + 0.5;
}
float _fbm_w(vec2 p, int oct) {
    vec2 w = vec2(_fbm(p + vec2(0.0,0.0), 3), _fbm(p + vec2(5.2,1.3), 3));
    w = w*2.0-1.0;
    return _fbm(p + w*1.5, oct);
}
float depth_field(vec2 p, float anim) {
    float h0 = _fbm_w(p*1.0 + anim, 6);
    float h1 = _fbm(p*3.2 + anim*0.5, 4) * 0.20;
    float h2 = _fbm(p*9.0, 3) * 0.05;
    return pow(clamp(h0+h1+h2, 0.0, 1.0), 1.15);
}

// ── Gradient, normal, hillshade ───────────────────────────────────────────────
vec2 depth_gradient(vec2 uv, float scale, float eps, float anim) {
    vec2 p = uv * scale;
    float r  = depth_field(p + vec2( eps, 0.0), anim);
    float l  = depth_field(p - vec2( eps, 0.0), anim);
    float up = depth_field(p + vec2(0.0,  eps), anim);
    float dn = depth_field(p - vec2(0.0,  eps), anim);
    return vec2(r-l, up-dn) / (2.0*eps);
}
vec3 depth_normal(vec2 grad) { return normalize(vec3(-grad.x, -grad.y, 1.0)); }

float hillshade(vec3 normal, float az_deg, float el_deg) {
    float az = (az_deg - 90.0) * (3.14159265 / 180.0);
    float el =  el_deg          * (3.14159265 / 180.0);
    vec3 light = normalize(vec3(cos(el)*cos(az), cos(el)*sin(az), sin(el)));
    return 0.12 + 0.88 * max(0.0, dot(normal, light));
}

// ── Hypsometric colour ramp ───────────────────────────────────────────────────
vec3 hypsometric(float h) {
    vec3 c = mix(vec3(0.02,0.07,0.28), vec3(0.04,0.17,0.50), smoothstep(0.20,0.32,h));
    c = mix(c, vec3(0.12,0.38,0.60),  smoothstep(0.32,0.42,h));
    c = mix(c, vec3(0.76,0.72,0.48),  smoothstep(0.42,0.47,h));
    c = mix(c, vec3(0.52,0.72,0.26),  smoothstep(0.47,0.55,h));
    c = mix(c, vec3(0.36,0.56,0.18),  smoothstep(0.55,0.64,h));
    c = mix(c, vec3(0.14,0.38,0.14),  smoothstep(0.64,0.70,h));
    c = mix(c, vec3(0.50,0.44,0.34),  smoothstep(0.70,0.80,h));
    c = mix(c, vec3(0.78,0.80,0.84),  smoothstep(0.80,0.88,h));
    c = mix(c, vec3(0.96,0.97,1.00),  smoothstep(0.88,0.95,h));
    return c;
}

// ── Topo contour lines ────────────────────────────────────────────────────────
float topo_line_single(float h, float freq, float sharp) {
    float s = h*freq, f = fract(s);
    float w = max(fwidth(s), 1e-5);
    float t = mix(1.5, 10.0, sharp);
    return 1.0 - clamp(min(f, 1.0-f)*t/w, 0.0, 1.0);
}
float topo_lines(float h, float bands, float sharp) {
    return max(topo_line_single(h, bands, sharp) * 0.5,
               topo_line_single(h, bands/5.0, sharp*0.55));
}

// ── Slope / aspect ────────────────────────────────────────────────────────────
float slope_map(vec2 g) { return clamp(length(g)*2.5, 0.0, 1.0); }

vec3 aspect_map(vec2 g, float slope) {
    float angle = atan(g.x, g.y);
    float hue   = angle / (2.0*3.14159265) + 0.5;
    float s = clamp(slope*2.0, 0.0, 1.0), v = 0.88;
    float h6 = hue*6.0, i = floor(h6), f = h6-i;
    float p = v*(1.0-s), q = v*(1.0-s*f), t_ = v*(1.0-s*(1.0-f));
    int ii = int(mod(i, 6.0));
    if      (ii==0) return vec3(v,  t_, p );
    else if (ii==1) return vec3(q,  v,  p );
    else if (ii==2) return vec3(p,  v,  t_);
    else if (ii==3) return vec3(p,  q,  v );
    else if (ii==4) return vec3(t_, p,  v );
    else            return vec3(v,  p,  q );
}

// ── Sobel edge detector ───────────────────────────────────────────────────────
float sobel_edges(vec2 uv, float scale, float eps, float anim) {
    vec2 p = uv*scale;
    float tl=depth_field(p+vec2(-eps, eps),anim), tc=depth_field(p+vec2(0,eps),anim),  tr=depth_field(p+vec2(eps,eps),anim);
    float ml=depth_field(p+vec2(-eps,  0 ),anim),                                       mr=depth_field(p+vec2(eps, 0 ),anim);
    float bl=depth_field(p+vec2(-eps,-eps),anim), bc=depth_field(p+vec2(0,-eps),anim),  br=depth_field(p+vec2(eps,-eps),anim);
    float gx=-tl-2.0*ml-bl+tr+2.0*mr+br;
    float gy=-tl-2.0*tc-tr+bl+2.0*bc+br;
    return clamp(sqrt(gx*gx+gy*gy)/(8.0*eps), 0.0, 1.0);
}

// ── Main ──────────────────────────────────────────────────────────────────────
void main() {
    vec2 uv = v_uv;
    int   mode      = int(iParams0.x + 0.5);
    float scale     = (iParams0.y > 0.1) ? iParams0.y : 3.0;
    float light_az  = (iParams0.z > 0.0) ? iParams0.z : 315.0;
    float light_el  = (iParams0.w > 0.0) ? iParams0.w : 45.0;
    float num_bands = (iParams1.x > 0.0) ? iParams1.x : 20.0;
    float line_sh   = (iParams1.y > 0.0) ? iParams1.y : 0.55;
    float animate   = iParams1.w;

    float anim = animate * iTime * 0.03;
    float eps  = 2.5 / max(iResolution.x * scale, 1.0);

    float  h    = depth_field(uv * scale, anim);
    vec2   grad = depth_gradient(uv, scale, eps, anim);
    vec3   norm = depth_normal(grad);

    float  shade = hillshade(norm, light_az, light_el);
    vec3   hypso = hypsometric(h);
    float  tline = topo_lines(h, num_bands, line_sh);
    float  slope = slope_map(grad);
    vec3   asp   = aspect_map(grad, slope);
    float  edges = sobel_edges(uv, scale, eps, anim);

    vec3 col;
    if      (mode == 0) col = hypso;
    else if (mode == 1) col = vec3(shade);
    else if (mode == 2) col = mix(vec3(0.96,0.94,0.88), vec3(0.28,0.16,0.04), tline);
    else if (mode == 3) col = mix(vec3(0.10,0.45,0.82), vec3(0.92,0.22,0.08), slope);
    else if (mode == 4) col = asp;
    else if (mode == 5) { col = hypso*(0.35+0.65*shade); col = mix(col, vec3(0.0), tline*0.5); }
    else if (mode == 6) col = mix(vec3(0.02,0.02,0.04), vec3(0.98,0.86,0.28), smoothstep(0.08,0.55,edges*10.0));
    else if (mode == 7) col = norm*0.5+0.5;
    else if (mode == 8) col = vec3(h);
    else if (mode == 9) { float gm=length(grad); col=mix(vec3(0,0,0.02), vec3(0,1.0,0.8), smoothstep(0.0,0.5,gm*3.0)); }
    else                { col = hypso*(0.35+0.65*shade); col = mix(col, vec3(0.0), tline*0.5); }

    fragColor = vec4(col, 1.0);
}


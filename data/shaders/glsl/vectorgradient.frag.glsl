#version 330 core
// ═══════════════════════════════════════════════════════════════════════════════
//  VECTOR GRADIENT SHADER (GLSL)  —  vectorgradient.frag.glsl
//  See vectorgradient.frag.metal for full documentation.
//
//  Params  (uniform vec4 iParams0..iParams3)
//   iParams0.x  mode       int 0..4
//   iParams0.y  density    float [4..40]
//   iParams0.z  arrow_size float [0.2..1.5]
//   iParams0.w  speed      float [0..4]
//   iParams1    color_field  vec4   (slot 4..7)
//   iParams2    color_shape  vec4   (slot 8..11)
//   iParams3.x  mouse_x    float [0..1]
//   iParams3.y  mouse_y    float [0..1]
//   iParams3.z  repel      float 0|1
//   iParams3.w  attr_r     float [0.02..0.3]
// ═══════════════════════════════════════════════════════════════════════════════

uniform vec2  iResolution;
uniform float iTime;
uniform vec4  iParams0;
uniform vec4  iParams1;
uniform vec4  iParams2;
uniform vec4  iParams3;

in  vec2 v_uv;
out vec4 fragColor;

// ── SDF primitives ─────────────────────────────────────────────────────────

float sdSeg(vec2 p, vec2 a, vec2 b, float r) {
    vec2  pa = p - a, ba = b - a;
    float h  = clamp(dot(pa, ba) / dot(ba, ba), 0.0, 1.0);
    return length(pa - ba * h) - r;
}

float sdTriFilled(vec2 p, vec2 a, vec2 b, vec2 c) {
    vec2  e0 = b-a, e1 = c-b, e2 = a-c;
    vec2  v0 = p-a, v1 = p-b, v2 = p-c;
    vec2  pq0 = v0 - e0*clamp(dot(v0,e0)/dot(e0,e0), 0., 1.);
    vec2  pq1 = v1 - e1*clamp(dot(v1,e1)/dot(e1,e1), 0., 1.);
    vec2  pq2 = v2 - e2*clamp(dot(v2,e2)/dot(e2,e2), 0., 1.);
    float s   = sign(e0.x*e2.y - e0.y*e2.x);
    vec2  d   = min(min(vec2(dot(pq0,pq0), s*(v0.x*e0.y-v0.y*e0.x)),
                        vec2(dot(pq1,pq1), s*(v1.x*e1.y-v1.y*e1.x))),
                        vec2(dot(pq2,pq2), s*(v2.x*e2.y-v2.y*e2.x)));
    return -sqrt(d.x) * sign(d.y);
}

// ── Arrow SDF ──────────────────────────────────────────────────────────────

float sdArrow(vec2 p, vec2 dir, float half_len, float shaft_r, float head_r) {
    vec2  perp      = vec2(-dir.y, dir.x);
    float head_back = min(head_r * 2.6, half_len * 0.72);
    vec2  tail      = -dir * half_len;
    vec2  shaft_tip =  dir * (half_len - head_back);
    vec2  head_tip  =  dir * half_len;
    float d_shaft   = sdSeg(p, tail, shaft_tip, shaft_r);
    float d_head    = sdTriFilled(p, head_tip,
                                  shaft_tip + perp * head_r,
                                  shaft_tip - perp * head_r);
    return min(d_shaft, d_head);
}

// ── Field functions ─────────────────────────────────────────────────────────

vec2 fRadial(vec2 p, vec2 att, vec2 asp) {
    vec2  d = (p - att) * asp;
    float r = length(d);
    return (r > 1e-6) ? d / r : vec2(0., 1.);
}

vec2 fCurl(vec2 p, vec2 att, vec2 asp, float sgn) {
    vec2  d = (p - att) * asp;
    float r = length(d);
    return (r > 1e-6) ? sgn * vec2(-d.y, d.x) / r : vec2(1., 0.);
}

float fRadialMag(vec2 p, vec2 att, vec2 asp) {
    return clamp(1.0 / (length((p - att) * asp) + 0.04), 0., 6.);
}

vec2 fNoise(vec2 p, float t) {
    float a = sin(p.x * 4.3 + t) * cos(p.y * 3.7 + t * 0.7) * 3.14159265;
    return vec2(cos(a + p.y * 2.1), sin(a - p.x * 1.9));
}

vec2 sampleField(vec2 p, vec2 att, vec2 asp, int mode, float t, bool repel) {
    float s = repel ? 1.0 : -1.0;
    vec2  v;
    if (mode == 0 || mode == 1) {
        v = fRadial(p, att, asp) * s;
    } else if (mode == 2) {
        vec2 r = fRadial(p, att, asp) * s;
        vec2 c = fCurl(p, att, asp, -1.0);
        vec2 n = fNoise(p * 0.6, t) * 0.2;
        v = r + c * 0.65 + n;
    } else if (mode == 3) {
        v = fCurl(p, att, asp, s);
    } else {  // 4: dipole
        vec2 att2 = vec2(1.0 - att.x, att.y);
        v = fRadial(p, att,  asp) *  s
          + fRadial(p, att2, asp) * -s;
    }
    float l = length(v);
    return (l > 1e-6) ? v / l : vec2(0., 1.);
}

// ── Color utilities ─────────────────────────────────────────────────────────

vec3 dir2hue(vec2 dir) {
    float h = atan(dir.y, dir.x) / (2.0 * 3.14159265) + 0.5;
    vec3  k = fract(vec3(h, h + 0.333, h + 0.667));
    vec3  p = abs(k * 6.0 - 3.0);
    return clamp(p - 1.0, 0.0, 1.0) * 0.85 + 0.08;
}

// ── Mode renderers ──────────────────────────────────────────────────────────

vec3 modeArrows(vec2 uv, vec2 att, vec2 asp,
                float density, float arr_sc, float t,
                bool repel, int fmode,
                vec3 ca, vec3 cb, vec3 bg)
{
    float cell  = 1.0 / density;
    vec2  ci    = round(uv / cell) * cell;
    vec2  fv    = sampleField(ci, att, asp, fmode, t, repel);
    float mag   = clamp(fRadialMag(ci, att, asp) * 0.25, 0.05, 1.0);
    vec2  lp    = (uv - ci) * asp;
    float hl    = cell * asp.y * 0.42 * arr_sc;
    float d     = sdArrow(lp, fv, hl, hl * 0.09, hl * 0.22);
    float AA    = fwidth(d);
    float a     = 1.0 - smoothstep(-AA, AA, d);
    vec3  col   = mix(ca * 0.35, ca, mag);
    return mix(bg, col, a);
}

vec3 modeDenseHue(vec2 uv, vec2 att, vec2 asp, float t, bool repel, vec3 bg) {
    vec2  fv  = sampleField(uv, att, asp, 1, t, repel);
    vec3  hue = dir2hue(fv);
    float mag = clamp(fRadialMag(uv, att, asp) * 0.30, 0.0, 1.0);
    return mix(bg, hue, 0.15 + mag * 0.85);
}

vec3 modeFlowRings(vec2 uv, vec2 att, vec2 asp, float density, float t,
                   bool repel, vec3 ca, vec3 cb, vec3 bg)
{
    vec2  p   = (uv - 0.5) * asp;
    vec2  pa  = (att - 0.5) * asp;
    float r   = length(p - pa);
    float sgn = repel ? -1.0 : 1.0;
    float bnd = 0.5 + 0.5 * sin(r * density * 7.0 + sgn * t * 2.8);
    bnd       = smoothstep(0.12, 0.82, bnd);
    vec2  dir = sampleField(uv, att, asp, 0, t, repel);
    vec3  hue = dir2hue(dir);
    vec3  col = mix(ca * 0.6, hue, 0.45);
    col       = mix(bg, col, 0.2 + bnd * 0.8);
    return mix(bg, col, clamp(r * 4.0, 0., 1.));
}

vec3 modeContour(vec2 uv, vec2 att, vec2 asp, float density, float arr_sc,
                 float t, bool repel, vec3 ca, vec3 cb, vec3 bg)
{
    vec2  p   = (uv - 0.5) * asp;
    vec2  pa  = (att - 0.5) * asp;
    float r   = length(p - pa);
    float phi = r * density * 0.35;
    float AA  = fwidth(phi);
    float cnt = 1.0 - smoothstep(0., AA * density * 0.35 * 0.45,
                                  abs(fract(phi + 0.5) - 0.5) - 0.38);
    vec3  col = mix(bg, ca * 0.55, cnt * 0.85);
    float cell = 1.5 / density;
    vec2  ci   = round(uv / cell) * cell;
    vec2  fv   = sampleField(ci, att, asp, 4, t, repel);
    vec2  lp   = (uv - ci) * asp;
    float hl   = cell * asp.y * 0.42 * arr_sc;
    float d    = sdArrow(lp, fv, hl, hl * 0.09, hl * 0.22);
    AA = fwidth(d);
    col = mix(col, cb, (1.0 - smoothstep(-AA, AA, d)) * 0.88);
    return col;
}

vec3 drawShape(vec2 uv, vec2 att, vec2 asp,
               float r, vec3 shape_col, vec3 ring_col, vec3 bg,
               bool repel, float t)
{
    vec2  p    = (uv - att) * asp;
    float d    = length(p) - r;
    float AA   = fwidth(d);
    float pulse = 0.5 + 0.5 * sin(t * 2.8);
    float dh   = abs(length(p) - r * (1.30 + pulse * 0.18)) - r * 0.055;
    float AAh  = fwidth(dh);
    vec3  col  = bg;
    vec3  haloC = repel ? vec3(1., 0.35, 0.05) : ring_col;
    col = mix(col, haloC * 0.65, (1. - smoothstep(-AAh, AAh, dh)) * 0.60);
    vec3 discC = repel ? vec3(1., 0.20, 0.04) : shape_col;
    col = mix(col, discC, 1. - smoothstep(-AA, AA, d));
    float dhi = length(p + vec2(r * 0.30, r * 0.30)) - r * 0.38;
    col = mix(col, vec3(1.), (1. - smoothstep(-AA, AA, dhi)) * 0.28);
    return col;
}

// ── Main ────────────────────────────────────────────────────────────────────

void main() {
    vec2  uv  = v_uv;
    vec2  res = iResolution;
    float t   = iTime;
    vec2  asp = vec2(res.x / res.y, 1.0);

    int   mode    = clamp(int(iParams0.x + 0.5), 0, 4);
    float density = (iParams0.y > 0.) ? iParams0.y : 15.;
    float arr_sc  = (iParams0.z > 0.) ? iParams0.z :  0.7;
    float fspeed  = (iParams0.w > 0.) ? iParams0.w :  1.0;
    t *= fspeed;

    vec3  ca = (iParams1.x + iParams1.y + iParams1.z > 0.01) ? iParams1.xyz : vec3(0.30, 0.70, 1.00);
    vec3  cb = (iParams2.x + iParams2.y + iParams2.z > 0.01) ? iParams2.xyz : vec3(1.00, 0.40, 0.20);
    vec3  bg = vec3(0.05, 0.055, 0.09);

    vec2 att = vec2(
        (iParams3.x > 0.001) ? iParams3.x : 0.5,
        (iParams3.y > 0.001) ? iParams3.y : 0.5
    );
    bool  repel  = iParams3.z > 0.5;
    float attr_r = ((iParams3.w > 0.001) ? iParams3.w : 0.06) * asp.y;

    vec3 col;
    if      (mode == 0) col = modeArrows(uv, att, asp, density, arr_sc, t, repel, 0, ca, cb, bg);
    else if (mode == 1) col = modeDenseHue(uv, att, asp, t, repel, bg);
    else if (mode == 2) col = modeFlowRings(uv, att, asp, density, t, repel, ca, cb, bg);
    else if (mode == 3) col = modeArrows(uv, att, asp, density, arr_sc, t, repel, 3, ca, cb, bg);
    else if (mode == 4) col = modeContour(uv, att, asp, density, arr_sc, t, repel, ca, cb, bg);
    else                col = bg;

    col = drawShape(uv, att, asp, attr_r, cb, ca, col, repel, t);

    fragColor = vec4(col, 1.0);
}


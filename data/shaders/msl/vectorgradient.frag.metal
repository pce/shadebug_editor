#include <metal_stdlib>
using namespace metal;

// ═══════════════════════════════════════════════════════════════════════════════
//  VECTOR GRADIENT SHADER  —  vectorgradient.frag.metal
//  (self-contained runtime version; see lib/vectorfield.metal for the modules)
//
//  Visualises a 2-D gradient / vector field with five rendering modes.
//  An "attractor" point (driven by mouse/touch input) anchors the field.
//
//  Block overview
//  ──────────────
//   1. SDF primitives   – capsule, filled triangle
//   2. Arrow SDF        – shaft + head
//   3. Field functions  – radial, curl, drain, dipole, noise-twist
//   4. Field sampler    – mode dispatch
//   5. Color utilities  – direction→HSV, magnitude ramp
//   6. Mode renderers:
//       mode 0  flat-arrows   sparse grid, minimal flat-design look
//       mode 1  dense-hue     per-pixel HSV direction colour (continuous)
//       mode 2  flow-rings    animated concentric ripple rings + direction tint
//       mode 3  vortex        curl / rotational field arrows
//       mode 4  contour       iso-distance lines + arrow overlay (dipole)
//   7. Attractor glyph  – pulsing circle with highlight
//   8. fs_main          – param decode → mode dispatch → attractor composite
//
//  Params  (buffer 1 – ParamUniforms, 64 bytes = 4 × float4)
//  ──────────────────────────────────────────────────────────
//   p0.x  slot  0  mode          int   0..4
//   p0.y  slot  1  density       float [4..40]   arrow/ring grid density
//   p0.z  slot  2  arrow_size    float [0.2..1.5] arrow scale multiplier
//   p0.w  slot  3  field_speed   float [0..4]    animation speed
//   p1    slot 4-7  color_field  float4           arrow / field colour
//   p2    slot 8-11 color_shape  float4           attractor glyph colour
//   p3.x  slot 12  mouse_x      float [0..1]     normalised viewport X
//   p3.y  slot 13  mouse_y      float [0..1]     normalised viewport Y (Y=0 top)
//   p3.z  slot 14  mouse_pressed float 0|1        0=attract / 1=repel
//   p3.w  slot 15  attractor_r  float [0.02..0.3] attractor circle radius (uv frac)
//
//  Interactive input
//  ─────────────────
//  The render panel injects the viewport-relative mouse/touch position into
//  slots 12-13 (mouse_x / mouse_y) on every frame when hovering.
//  Mouse button 0 writes slot 14 (mouse_pressed = 1 → repel mode).
// ═══════════════════════════════════════════════════════════════════════════════

struct EffectUniforms { float2 iResolution; float iTime; float _pad; };
struct ParamUniforms  { float4 p0; float4 p1; float4 p2; float4 p3; };
struct Varyings       { float4 pos [[position]]; float2 uv; };

// ══════════════════════════════════════════════════════════════════════════════
// 1. SDF PRIMITIVES
// ══════════════════════════════════════════════════════════════════════════════

static float sdSeg(float2 p, float2 a, float2 b, float r) {
    float2 pa = p - a, ba = b - a;
    float  h  = clamp(dot(pa, ba) / dot(ba, ba), 0.f, 1.f);
    return length(pa - ba * h) - r;
}

static float sdTriFilled(float2 p, float2 a, float2 b, float2 c) {
    float2 e0 = b-a, e1 = c-b, e2 = a-c;
    float2 v0 = p-a, v1 = p-b, v2 = p-c;
    float2 pq0 = v0 - e0*clamp(dot(v0,e0)/dot(e0,e0), 0.f, 1.f);
    float2 pq1 = v1 - e1*clamp(dot(v1,e1)/dot(e1,e1), 0.f, 1.f);
    float2 pq2 = v2 - e2*clamp(dot(v2,e2)/dot(e2,e2), 0.f, 1.f);
    float  s   = sign(e0.x*e2.y - e0.y*e2.x);
    float2 d   = min(min(float2(dot(pq0,pq0), s*(v0.x*e0.y - v0.y*e0.x)),
                         float2(dot(pq1,pq1), s*(v1.x*e1.y - v1.y*e1.x))),
                         float2(dot(pq2,pq2), s*(v2.x*e2.y - v2.y*e2.x)));
    return -sqrt(d.x) * sign(d.y);
}

// ══════════════════════════════════════════════════════════════════════════════
// 2. ARROW SDF
//    p        – pixel in arrow-local space (origin = arrow centre)
//    dir      – normalised direction (toward arrowhead)
//    half_len – half total arrow length (in same space as p)
//    shaft_r  – shaft half-width
//    head_r   – arrowhead half-width at base
// ══════════════════════════════════════════════════════════════════════════════

static float sdArrow(float2 p, float2 dir, float half_len,
                     float shaft_r, float head_r)
{
    float2 perp      = float2(-dir.y, dir.x);
    float  head_back = min(head_r * 2.6f, half_len * 0.72f);
    float2 tail      = -dir * half_len;
    float2 shaft_tip =  dir * (half_len - head_back);
    float2 head_tip  =  dir * half_len;
    float  d_shaft   = sdSeg(p, tail, shaft_tip, shaft_r);
    float  d_head    = sdTriFilled(p, head_tip,
                                   shaft_tip + perp * head_r,
                                   shaft_tip - perp * head_r);
    return min(d_shaft, d_head);
}

// ══════════════════════════════════════════════════════════════════════════════
// 3. FIELD FUNCTIONS
//    All take p, att in UV space and asp = float2(res.x/res.y, 1.0).
//    Return normalised direction in aspect-corrected space.
// ══════════════════════════════════════════════════════════════════════════════

static float2 fRadial(float2 p, float2 att, float2 asp) {
    float2 d = (p - att) * asp;
    float  r = length(d);
    return (r > 1e-6f) ? d / r : float2(0.f, 1.f);
}

static float2 fCurl(float2 p, float2 att, float2 asp, float sgn) {
    float2 d = (p - att) * asp;
    float  r = length(d);
    return (r > 1e-6f) ? sgn * float2(-d.y, d.x) / r : float2(1.f, 0.f);
}

// Inverse-square magnitude (for colour mapping)
static float fRadialMag(float2 p, float2 att, float2 asp) {
    return clamp(1.f / (length((p - att) * asp) + 0.04f), 0.f, 6.f);
}

static float2 fNoise(float2 p, float t) {
    float a = sin(p.x * 4.3f + t) * cos(p.y * 3.7f + t * 0.7f) * 3.14159265f;
    return float2(cos(a + p.y * 2.1f), sin(a - p.x * 1.9f));
}

// ══════════════════════════════════════════════════════════════════════════════
// 4. UNIFIED FIELD SAMPLER  → normalised direction in asp-corrected space
// ══════════════════════════════════════════════════════════════════════════════

static float2 sampleField(float2 p, float2 att, float2 asp,
                           int mode, float t, bool repel)
{
    float s = repel ? 1.f : -1.f;
    float2 v;
    switch (mode) {
    case 0:
    case 1:
        v = fRadial(p, att, asp) * s;
        break;
    case 2: {
        float2 r = fRadial(p, att, asp) * s;
        float2 c = fCurl(p, att, asp, -1.f);
        float2 n = fNoise(p * 0.6f, t) * 0.2f;
        v = r + c * 0.65f + n;
        break;
    }
    case 3:
        v = fCurl(p, att, asp, s);
        break;
    case 4: {
        float2 att2 = float2(1.f - att.x, att.y);
        float2 r1   = fRadial(p, att,  asp) *  s;
        float2 r2   = fRadial(p, att2, asp) * -s;
        v = r1 + r2;
        break;
    }
    default:
        v = fRadial(p, att, asp) * s;
        break;
    }
    float l = length(v);
    return (l > 1e-6f) ? v / l : float2(0.f, 1.f);
}

// ══════════════════════════════════════════════════════════════════════════════
// 5. COLOR UTILITIES
// ══════════════════════════════════════════════════════════════════════════════

// Direction angle → HSV colour (S≈0.85, V≈1.0)
static float3 dir2hue(float2 dir) {
    float  h = atan2(dir.y, dir.x) / (2.f * 3.14159265f) + 0.5f;
    float3 k = fract(float3(h, h + 0.333f, h + 0.667f));
    float3 p = abs(k * 6.f - 3.f);
    return clamp(p - 1.f, 0.f, 1.f) * 0.85f + 0.08f;
}

// ══════════════════════════════════════════════════════════════════════════════
// 6. MODE RENDERERS
// ══════════════════════════════════════════════════════════════════════════════

// ── Mode 0 / 3: Sparse arrow grid ────────────────────────────────────────────
//  Uniform grid of arrows coloured by field magnitude.
//  fmode 0 → radial field, fmode 3 → curl / vortex field.
static float3 modeArrows(float2 uv, float2 att, float2 asp,
                          float density, float arr_sc, float t,
                          bool repel, int fmode,
                          float3 ca, float3 cb, float3 bg)
{
    // Grid in UV space; arrow SDF computed in aspect-corrected space.
    // density controls grid spacing; arr_sc scales only the arrow size within the cell.
    float  cell   = 1.f / density;                       // cell size (UV)
    float2 ci     = round(uv / cell) * cell;             // nearest cell centre
    float2 fv     = sampleField(ci, att, asp, fmode, t, repel); // normalised asp-dir
    float  mag    = clamp(fRadialMag(ci, att, asp) * 0.25f, 0.05f, 1.f);

    // Arrow geometry in aspect-corrected space
    float2 lp     = (uv - ci) * asp;   // local pixel position
    float  hl     = cell * asp.y * 0.42f * arr_sc;  // arr_sc scales only arrow size
    float  sw     = hl * 0.09f;
    float  hr     = hl * 0.22f;

    float  d      = sdArrow(lp, fv, hl, sw, hr);
    float  AA     = fwidth(d);
    float  a      = 1.f - smoothstep(-AA, AA, d);

    float3 col    = mix(ca * 0.35f, ca, mag);
    return mix(bg, col, a);
}

// ── Mode 1: Dense continuous HSV direction field ──────────────────────────────
//  Every pixel coloured by the field direction at that point.
static float3 modeDenseHue(float2 uv, float2 att, float2 asp,
                            float t, bool repel, float3 bg)
{
    float2 fv  = sampleField(uv, att, asp, 1, t, repel);
    float3 hue = dir2hue(fv);
    float  mag = clamp(fRadialMag(uv, att, asp) * 0.30f, 0.f, 1.f);
    // darken far edges slightly, brighten near attractor
    return mix(bg, hue, 0.15f + mag * 0.85f);
}

// ── Mode 2: Animated concentric flow rings ────────────────────────────────────
//  Ripple bands propagate toward / away from the attractor, tinted by direction.
static float3 modeFlowRings(float2 uv, float2 att, float2 asp,
                              float density, float t, bool repel,
                              float3 ca, float3 cb, float3 bg)
{
    float2 p   = (uv - 0.5f) * asp;
    float2 pa  = (att - 0.5f) * asp;
    float  r   = length(p - pa);

    // Animated bands: flow inward (attract) or outward (repel)
    float  sgn   = repel ? -1.f : 1.f;
    float  phase = r * density * 7.f + sgn * t * 2.8f;
    float  bands = 0.5f + 0.5f * sin(phase);
    bands = smoothstep(0.12f, 0.82f, bands);

    // Direction tint
    float2 dir = sampleField(uv, att, asp, 0, t, repel);
    float3 hue = dir2hue(dir);
    float3 col = mix(ca * 0.6f, hue, 0.45f);
    col        = mix(bg, col, 0.2f + bands * 0.8f);

    // Fade very close to attractor
    float  fade = clamp(r * 4.f, 0.f, 1.f);
    return mix(bg, col, fade);
}

// ── Mode 4: Iso-distance contour lines + sparse arrow overlay ─────────────────
//  Equidistant contour lines from the attractor with dipole arrow overlay.
static float3 modeContour(float2 uv, float2 att, float2 asp,
                           float density, float arr_sc, float t,
                           bool repel, float3 ca, float3 cb, float3 bg)
{
    float2 p  = (uv - 0.5f) * asp;
    float2 pa = (att - 0.5f) * asp;
    float  r  = length(p - pa);

    // AA iso-contour lines
    float  nbands = density * 0.35f;
    float  phi    = r * nbands;
    float  AA_phi = fwidth(phi);
    float  contour = 1.f - smoothstep(0.f, AA_phi * nbands * 0.45f,
                                      abs(fract(phi + 0.5f) - 0.5f) - 0.38f);
    float3 col = mix(bg, ca * 0.55f, contour * 0.85f);

    // Sparse arrow overlay (dipole field, mode 4)
    float  cell  = 1.5f / density;                    // grid-only, not arr_sc
    float2 ci    = round(uv / cell) * cell;
    float2 fv    = sampleField(ci, att, asp, 4, t, repel);
    float2 lp    = (uv - ci) * asp;
    float  hl    = cell * asp.y * 0.42f * arr_sc;     // arr_sc for size only
    float  d_arw = sdArrow(lp, fv, hl, hl * 0.09f, hl * 0.22f);
    float  AA    = fwidth(d_arw);
    col = mix(col, cb, (1.f - smoothstep(-AA, AA, d_arw)) * 0.88f);

    return col;
}

// ══════════════════════════════════════════════════════════════════════════════
// 7. ATTRACTOR GLYPH
//    Pulsing circle with inner highlight; colour indicates attract / repel mode.
// ══════════════════════════════════════════════════════════════════════════════

static float3 drawShape(float2 uv, float2 att, float2 asp,
                         float r, float3 shape_col, float3 ring_col,
                         float3 bg, bool repel, float t)
{
    float2 p = (uv - att) * asp;
    float  d = length(p) - r;
    float  AA = fwidth(d);

    // Pulsing outer halo
    float  pulse  = 0.5f + 0.5f * sin(t * 2.8f);
    float  halo_r = r * (1.30f + pulse * 0.18f);
    float  dh     = abs(length(p) - halo_r) - r * 0.055f;
    float  AAh    = fwidth(dh);

    float3 col     = bg;
    float3 halo_c  = repel ? float3(1.f, 0.35f, 0.05f) : ring_col;

    // Halo ring
    col = mix(col, halo_c * 0.65f, (1.f - smoothstep(-AAh, AAh, dh)) * 0.60f);
    // Filled disc
    float3 disc_c = repel ? float3(1.f, 0.20f, 0.04f) : shape_col;
    col = mix(col, disc_c, 1.f - smoothstep(-AA, AA, d));
    // Specular highlight (top-left offset)
    float  dhi = length(p + float2(r * 0.30f, r * 0.30f)) - r * 0.38f;
    col = mix(col, float3(1.f), (1.f - smoothstep(-AA, AA, dhi)) * 0.28f);

    return col;
}

// ══════════════════════════════════════════════════════════════════════════════
// 8. FRAGMENT MAIN
// ══════════════════════════════════════════════════════════════════════════════

fragment float4 fs_main(Varyings           in [[stage_in]],
                        constant EffectUniforms& u [[buffer(0)]],
                        constant ParamUniforms&  p [[buffer(1)]])
{
    float2 uv  = in.uv;
    float2 res = u.iResolution;
    float  t   = u.iTime;

    float2 asp = float2(res.x / res.y, 1.f);   // aspect corrector

    // ── Decode params ─────────────────────────────────────────────────────────
    int   mode    = clamp(int(p.p0.x + 0.5f), 0, 4);
    float density = (p.p0.y > 0.f) ? p.p0.y : 15.f;
    float arr_sc  = (p.p0.z > 0.f) ? p.p0.z :  0.7f;
    float fspeed  = (p.p0.w > 0.f) ? p.p0.w :  1.f;
    t *= fspeed;

    // Colors — fall back to defaults if all-zero
    float3 ca = (p.p1.x + p.p1.y + p.p1.z > 0.01f) ? p.p1.xyz : float3(0.30f, 0.70f, 1.00f);
    float3 cb = (p.p2.x + p.p2.y + p.p2.z > 0.01f) ? p.p2.xyz : float3(1.00f, 0.40f, 0.20f);
    float3 bg = float3(0.05f, 0.055f, 0.09f);   // dark navy background

    // Attractor position (mouse / touch, injected by render panel)
    float2 att = float2(
        (p.p3.x > 0.001f) ? p.p3.x : 0.5f,
        (p.p3.y > 0.001f) ? p.p3.y : 0.5f
    );
    bool   repel  = p.p3.z > 0.5f;
    float  attr_r = ((p.p3.w > 0.001f) ? p.p3.w : 0.06f) * asp.y;
    // attr_r is now in asp-corrected units (asp.y = 1.0 always)

    // ── Mode dispatch ─────────────────────────────────────────────────────────
    float3 col;
    switch (mode) {
    case 0:
        col = modeArrows(uv, att, asp, density, arr_sc, t, repel, 0, ca, cb, bg);
        break;
    case 1:
        col = modeDenseHue(uv, att, asp, t, repel, bg);
        break;
    case 2:
        col = modeFlowRings(uv, att, asp, density, t, repel, ca, cb, bg);
        break;
    case 3:
        col = modeArrows(uv, att, asp, density, arr_sc, t, repel, 3, ca, cb, bg);
        break;
    case 4:
        col = modeContour(uv, att, asp, density, arr_sc, t, repel, ca, cb, bg);
        break;
    default:
        col = bg;
        break;
    }

    // ── Attractor glyph (drawn on top of all modes) ───────────────────────────
    col = drawShape(uv, att, asp, attr_r, cb, ca, col, repel, t);

    return float4(col, 1.f);
}


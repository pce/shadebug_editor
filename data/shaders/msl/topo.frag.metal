#include <metal_stdlib>
using namespace metal;

// ═══════════════════════════════════════════════════════════════════════════════
//  TOPOGRAPHIC SHADER  — depthmap preprocessing → cartographic visualisation
//
//  Block overview:
//   1. hash / gnoise / fbm / fbm_warped  — procedural depthmap (swap with real
//                                          texture later via texture2d<float>)
//   2. depth_gradient()  — central differences  →  (dz/dx, dz/dy)
//   3. depth_normal()    — gradient → unit surface normal
//   4. hillshade()       — Lambert + ambient cartographic shading
//   5. hypsometric()     — altitude colour ramp (ocean → snow)
//   6. topo_lines()      — AA iso-contour lines  (thin + indexed "index contour")
//   7. slope_map()       — gradient magnitude → steepness
//   8. aspect_map()      — gradient direction → compass-rose HSV colour
//   9. sobel_edges()     — Sobel ridge / valley detector
//  10. fs_main()         — mode switch + composition
//
//  Params  (buffer 1 – ParamUniforms)
//  ────────────────────────────────────────────────────────────────────────────
//   p0.x  slot 0  mode        int   0=hypsometric  1=hillshade  2=topo
//                                    3=slope        4=aspect     5=combined
//                                    6=edges        7=normalmap  8=raw depth
//   p0.y  slot 1  scale       float [0.5 .. 8]     terrain zoom
//   p0.z  slot 2  light_az    float [0 .. 360]     sun azimuth  (° CW from N)
//   p0.w  slot 3  light_el    float [5 .. 85]      sun elevation (°)
//   p1.x  slot 4  num_bands   float [4 .. 40]      contour line count
//   p1.y  slot 5  line_sharp  float [0 .. 1]       contour sharpness
//   p1.w  slot 7  animate     float [0 .. 1]       slow terrain drift
// ═══════════════════════════════════════════════════════════════════════════════

struct EffectUniforms { float2 iResolution; float iTime; float _pad; };
struct ParamUniforms  { float4 p0; float4 p1; float4 p2; float4 p3; };
struct Varyings { float4 pos [[position]]; float2 uv; };

// ── 1. Procedural depthmap ────────────────────────────────────────────────────
//  Replace depth_field() with a texture sample once you have an actual depthmap:
//    float depth_field(float2 uv, texture2d<float> tex) {
//        return tex.sample(s, uv).r;   // normalised [0,1]
//    }

static float2 _hash2(float2 p) {
    p = float2(dot(p, float2(127.1f, 311.7f)),
               dot(p, float2(269.5f, 183.3f)));
    return fract(sin(p) * 43758.5453f);
}

static float _gnoise(float2 p) {
    float2 i = floor(p), f = fract(p);
    float2 u = f * f * (3.0f - 2.0f * f);
    float a = dot(_hash2(i             ) * 2.0f - 1.0f, f              );
    float b = dot(_hash2(i + float2(1,0)) * 2.0f - 1.0f, f - float2(1,0));
    float c = dot(_hash2(i + float2(0,1)) * 2.0f - 1.0f, f - float2(0,1));
    float d = dot(_hash2(i + float2(1,1)) * 2.0f - 1.0f, f - float2(1,1));
    return mix(mix(a,b,u.x), mix(c,d,u.x), u.y);
}

static float _fbm(float2 p, int oct) {
    float v = 0.0f, amp = 0.5f;
    for (int i = 0; i < oct; ++i) { v += _gnoise(p)*amp; p *= 2.0f; amp *= 0.5f; }
    return v * 0.5f + 0.5f;   // → [0,1]
}

static float _fbm_w(float2 p, int oct) {
    float2 w = float2(_fbm(p + float2(0.0f, 0.0f), 3),
                      _fbm(p + float2(5.2f, 1.3f), 3));
    w = w * 2.0f - 1.0f;
    return _fbm(p + w * 1.5f, oct);
}

// Layered terrain: broad shapes + medium detail + fine texture
static float depth_field(float2 p, float anim) {
    float h0 = _fbm_w(p * 1.0f + anim,           6);   // macro terrain
    float h1 = _fbm  (p * 3.2f + anim * 0.5f, 4) * 0.20f;
    float h2 = _fbm  (p * 9.0f,                3) * 0.05f;
    float h  = h0 + h1 + h2;
    return pow(clamp(h, 0.0f, 1.0f), 1.15f);   // mild contrast stretch
}

// ── 2. Gradient — central differences ────────────────────────────────────────
//
//  Returns (dz/dx, dz/dy) estimated by 2-tap central differences.
//  eps should be ~1–3 pixels in UV space.
//
static float2 depth_gradient(float2 uv, float scale, float eps, float anim) {
    float2 p = uv * scale;
    float r  = depth_field(p + float2( eps, 0.0f), anim);
    float l  = depth_field(p - float2( eps, 0.0f), anim);
    float up = depth_field(p + float2(0.0f,  eps), anim);
    float dn = depth_field(p - float2(0.0f,  eps), anim);
    return float2(r - l, up - dn) / (2.0f * eps);
}

// ── 3. Surface normal from gradient ──────────────────────────────────────────
//
//  Tangent vectors on the height surface:
//    T = (1, 0, dz/dx),  B = (0, 1, dz/dy)
//  Normal = cross(T, B) = (-dz/dx, -dz/dy, 1), then normalised.
//
static float3 depth_normal(float2 grad) {
    return normalize(float3(-grad.x, -grad.y, 1.0f));
}

// ── 4. Hillshading ────────────────────────────────────────────────────────────
//
//  Classic cartographic illumination used by GIS tools (ESRI / QGIS style).
//  azimuth_deg : degrees clockwise from North (0 = light from North)
//  elevation_deg: degrees above the horizon (0 = horizontal, 90 = overhead)
//
static float hillshade(float3 normal, float azimuth_deg, float elevation_deg) {
    float az = (azimuth_deg - 90.0f) * (M_PI_F / 180.0f);   // convert to maths angle
    float el = elevation_deg         * (M_PI_F / 180.0f);
    float3 light = normalize(float3(
        cos(el) * cos(az),
        cos(el) * sin(az),
        sin(el)
    ));
    float diff    = max(0.0f, dot(normal, light));
    float ambient = 0.12f;   // softer shadows in deep valleys
    return ambient + (1.0f - ambient) * diff;
}

// ── 5. Hypsometric colour ramp ─────────────────────────────────────────────────
//
//  Altitude-banded colouring: deep ocean → shallow → coast → land biomes → snow.
//  This is the standard "hypsometric tinting" used in physical atlas maps.
//
static float3 hypsometric(float h) {
    const float3 deep_ocean = float3(0.02f, 0.07f, 0.28f);
    const float3 ocean      = float3(0.04f, 0.17f, 0.50f);
    const float3 shallow    = float3(0.12f, 0.38f, 0.60f);
    const float3 coast      = float3(0.76f, 0.72f, 0.48f);
    const float3 lowland    = float3(0.52f, 0.72f, 0.26f);
    const float3 highland   = float3(0.36f, 0.56f, 0.18f);
    const float3 forest     = float3(0.14f, 0.38f, 0.14f);
    const float3 rock       = float3(0.50f, 0.44f, 0.34f);
    const float3 snowline   = float3(0.78f, 0.80f, 0.84f);
    const float3 snow       = float3(0.96f, 0.97f, 1.00f);

    float3 c = deep_ocean;
    c = mix(c, ocean,    smoothstep(0.20f, 0.32f, h));
    c = mix(c, shallow,  smoothstep(0.32f, 0.42f, h));
    c = mix(c, coast,    smoothstep(0.42f, 0.47f, h));
    c = mix(c, lowland,  smoothstep(0.47f, 0.55f, h));
    c = mix(c, highland, smoothstep(0.55f, 0.64f, h));
    c = mix(c, forest,   smoothstep(0.64f, 0.70f, h));
    c = mix(c, rock,     smoothstep(0.70f, 0.80f, h));
    c = mix(c, snowline, smoothstep(0.80f, 0.88f, h));
    c = mix(c, snow,     smoothstep(0.88f, 0.95f, h));
    return c;
}

// ── 6. Topographic contour lines ──────────────────────────────────────────────
//
//  Anti-aliased iso-lines at equal height steps.
//  Returns [0,1] mask — 1.0 = on a contour line.
//  "Index contours" (every 5th line) are drawn thicker, as on paper maps.
//
static float topo_line_single(float h, float freq, float sharpness) {
    float s = h * freq;
    float f = fract(s);
    float w = max(fwidth(s), 1e-5f);
    float t = mix(1.5f, 10.0f, sharpness);
    return 1.0f - clamp(min(f, 1.0f - f) * t / w, 0.0f, 1.0f);
}

static float topo_lines(float h, float num_bands, float sharpness) {
    float thin  = topo_line_single(h, num_bands,         sharpness);
    float thick = topo_line_single(h, num_bands / 5.0f, sharpness * 0.55f);
    return max(thin * 0.50f, thick);
}

// ── 7. Slope map ──────────────────────────────────────────────────────────────
//
//  Gradient magnitude → steepness [0,1].
//  Flat terrain = 0, near-vertical cliff = 1.
//
static float slope_map(float2 grad) {
    return clamp(length(grad) * 2.5f, 0.0f, 1.0f);
}

// ── 8. Aspect map ─────────────────────────────────────────────────────────────
//
//  Gradient direction → compass-rose hue (N=cyan, E=yellow, S=red, W=blue).
//  Flat terrain (low slope) desaturates to grey.
//
static float3 aspect_map(float2 grad, float slope) {
    float angle = atan2(grad.x, grad.y);        // [-π, π], 0 = North
    float hue   = angle / (2.0f * M_PI_F) + 0.5f;  // [0,1]
    // HSV → RGB  (S = slope so flat terrain → grey, V fixed)
    float s = saturate(slope * 2.0f);
    float v = 0.88f;
    float h6 = hue * 6.0f;
    float i  = floor(h6);
    float f  = h6 - i;
    float p  = v*(1.0f-s);
    float q  = v*(1.0f-s*f);
    float t_ = v*(1.0f-s*(1.0f-f));
    int   ii = int(i) % 6;
    if      (ii==0) return float3(v,  t_, p );
    else if (ii==1) return float3(q,  v,  p );
    else if (ii==2) return float3(p,  v,  t_);
    else if (ii==3) return float3(p,  q,  v );
    else if (ii==4) return float3(t_, p,  v );
    else            return float3(v,  p,  q );
}

// ── 9. Sobel edge detector on depth ──────────────────────────────────────────
//
//  3×3 Sobel kernel applied to the depth field.
//  Returns [0,1] edge strength — bright = ridgeline or sharp valley.
//
static float sobel_edges(float2 uv, float scale, float eps, float anim) {
    float2 p = uv * scale;
    float tl = depth_field(p + float2(-eps,  eps), anim);
    float tc = depth_field(p + float2( 0.f,  eps), anim);
    float tr = depth_field(p + float2( eps,  eps), anim);
    float ml = depth_field(p + float2(-eps,  0.f), anim);
    float mr = depth_field(p + float2( eps,  0.f), anim);
    float bl = depth_field(p + float2(-eps, -eps), anim);
    float bc = depth_field(p + float2( 0.f, -eps), anim);
    float br = depth_field(p + float2( eps, -eps), anim);
    float gx = -tl - 2.0f*ml - bl + tr + 2.0f*mr + br;
    float gy = -tl - 2.0f*tc - tr + bl + 2.0f*bc + br;
    // Normalise by kernel scale so result is ~[0,1] for typical depth range
    return saturate(sqrt(gx*gx + gy*gy) / (8.0f * eps));
}

// ── 10. Fragment main ─────────────────────────────────────────────────────────

fragment float4 fs_main(
    Varyings                 in [[stage_in]],
    constant EffectUniforms& u  [[buffer(0)]],
    constant ParamUniforms&  p  [[buffer(1)]])
{
    float2 uv = in.uv;

    // ── Unpack params (with sensible defaults when zero) ─────────────────────
    int   mode      = int(p.p0.x + 0.5f);
    float scale     = (p.p0.y > 0.1f) ? p.p0.y    : 3.0f;
    float light_az  = (p.p0.z > 0.0f) ? p.p0.z    : 315.0f; // NW sun by default
    float light_el  = (p.p0.w > 0.0f) ? p.p0.w    : 45.0f;
    float num_bands = (p.p1.x > 0.0f) ? p.p1.x    : 20.0f;
    float line_sh   = (p.p1.y > 0.0f) ? p.p1.y    : 0.55f;
    float animate   = p.p1.w;   // 0 = static, 1 = slow drift

    // Epsilon for finite-difference gradient: ~2 px in terrain UV space
    float anim = animate * u.iTime * 0.03f;
    float eps  = 2.5f / max(u.iResolution.x * scale, 1.0f);

    // ── Sample depth + derivatives ───────────────────────────────────────────
    float  h    = depth_field(uv * scale, anim);
    float2 grad = depth_gradient(uv, scale, eps, anim);
    float3 norm = depth_normal(grad);

    // ── Pre-compute all layers (cheap; lets you blend freely) ────────────────
    float  shade = hillshade(norm, light_az, light_el);
    float3 hypso = hypsometric(h);
    float  tline = topo_lines(h, num_bands, line_sh);
    float  slope = slope_map(grad);
    float3 asp   = aspect_map(grad, slope);
    float  edges = sobel_edges(uv, scale, eps, anim);

    // ── Mode switch ──────────────────────────────────────────────────────────
    float3 col;

    switch (mode) {

    // ─ 0: Hypsometric (altitude colour only) ─────────────────────────────────
    case 0:
        col = hypso;
        break;

    // ─ 1: Hillshade greyscale ────────────────────────────────────────────────
    case 1:
        col = float3(shade);
        break;

    // ─ 2: Topographic map lines only ─────────────────────────────────────────
    case 2: {
        float3 bg       = float3(0.96f, 0.94f, 0.88f);   // parchment
        float3 line_col = float3(0.28f, 0.16f, 0.04f);   // dark-brown contour
        col = mix(bg, line_col, tline);
        break;
    }

    // ─ 3: Slope / steepness map ──────────────────────────────────────────────
    case 3: {
        float3 flat_c  = float3(0.10f, 0.45f, 0.82f);   // blue = gentle
        float3 steep_c = float3(0.92f, 0.22f, 0.08f);   // red  = cliff
        col = mix(flat_c, steep_c, slope);
        break;
    }

    // ─ 4: Aspect / orientation map ────────────────────────────────────────────
    case 4:
        col = asp;
        break;

    // ─ 5: Combined — classic cartographic (hypso × hillshade + contours) ─────
    case 5: {
        // 1. Hypsometric base
        col = hypso;
        // 2. Hillshade modulation (multiplied → Eckert-style blend)
        col *= (0.35f + 0.65f * shade);
        // 3. Overlay thin contour lines
        float3 line_col = float3(0.0f, 0.0f, 0.0f);
        col = mix(col, line_col, tline * 0.50f);
        break;
    }

    // ─ 6: Edge / ridge detector ──────────────────────────────────────────────
    case 6: {
        float3 bg_c    = float3(0.02f, 0.02f, 0.04f);
        float3 ridge_c = float3(0.98f, 0.86f, 0.28f);
        col = mix(bg_c, ridge_c, smoothstep(0.08f, 0.55f, edges * 10.0f));
        // Faint depth hue underneath
        col = mix(float3(h * 0.12f, h * 0.15f, h * 0.25f), col, 0.80f);
        break;
    }

    // ─ 7: Normal map (tangent-space, RGB encoded) ────────────────────────────
    case 7:
        // Standard normal-map convention: R=X, G=Y, B=Z, remapped [-1,1]→[0,1]
        col = norm * 0.5f + 0.5f;
        break;

    // ─ 8: Raw depth greyscale ────────────────────────────────────────────────
    case 8:
        col = float3(h);
        break;

    // ─ 9: Gradient magnitude debug ───────────────────────────────────────────
    case 9: {
        float  gm  = length(grad);
        float3 bg_c = float3(0.0f, 0.0f, 0.02f);
        float3 gm_c = float3(0.0f, 1.0f, 0.8f);
        col = mix(bg_c, gm_c, smoothstep(0.0f, 0.5f, gm * 3.0f));
        break;
    }

    // ─ default: combined + contours  ─────────────────────────────────────────
    default: {
        col = hypso * (0.35f + 0.65f * shade);
        col = mix(col, float3(0.0f), tline * 0.50f);
        break;
    }
    }

    return float4(col, 1.0f);
}


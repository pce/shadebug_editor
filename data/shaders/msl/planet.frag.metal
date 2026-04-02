#include <metal_stdlib>
using namespace metal;

struct EffectUniforms { float2 iResolution; float iTime; float _pad; };
struct Varyings { float4 pos [[position]]; float2 uv; };

// ═══════════════════════════════════════════════════════════════════════════════
//  PLANET SHADER — proper spherical UV + modular surface functions
//
//  Block overview:
//   1. hash / noise / fbm          — base math
//   2. spherical_uv()              — correct equirectangular UV for a sphere
//   3. domain_warp()               — distort UV for irregular continent shapes
//   4. surface_color()             — ocean / land / sand / rock biome blend
//   5. polar_caps()                — latitude-based ice
//   6. cloud_layer()               — wispy cloud fbm draped over sphere
//   7. atmosphere_rim()            — Fresnel glow at limb
//   8. stars_background()          — background star field
//   9. fs_main()                   — compose everything
// ═══════════════════════════════════════════════════════════════════════════════

// ── 1. Hash / Noise / FBM ─────────────────────────────────────────────────────

static float hash1(float2 p) {
    return fract(sin(dot(p, float2(127.1, 311.7))) * 43758.5453);
}
static float2 hash2(float2 p) {
    p = float2(dot(p, float2(127.1, 311.7)), dot(p, float2(269.5, 183.3)));
    return fract(sin(p) * 43758.5453);
}
// Gradient noise
static float gnoise(float2 p) {
    float2 i = floor(p), f = fract(p);
    float2 u = f*f*(3.0 - 2.0*f);
    float a = dot(hash2(i          )*2.0-1.0, f          );
    float b = dot(hash2(i+float2(1,0))*2.0-1.0, f-float2(1,0));
    float c = dot(hash2(i+float2(0,1))*2.0-1.0, f-float2(0,1));
    float d = dot(hash2(i+float2(1,1))*2.0-1.0, f-float2(1,1));
    return mix(mix(a,b,u.x), mix(c,d,u.x), u.y);
}
// Standard FBM
static float fbm(float2 p, int oct) {
    float v = 0.0, amp = 0.5;
    for (int i = 0; i < oct; i++) { v += gnoise(p)*amp; p *= 2.0; amp *= 0.5; }
    return v * 0.5 + 0.5;  // [0,1]
}
// Domain-warped FBM — makes continents look more irregular
static float fbm_warped(float2 p, int oct) {
    float2 warp = float2(fbm(p + float2(0.0, 0.0), 3),
                         fbm(p + float2(5.2, 1.3), 3));
    warp = warp * 2.0 - 1.0;
    return fbm(p + warp * 1.2, oct);
}

// ── 2. Spherical UV — wraps correctly around the WHOLE sphere ─────────────────
//
//  Input:  sUV = (delta/radius) → disc coords in [-1,1]
//          z   = sqrt(1 - dot(sUV,sUV))   (sphere depth)
//  Output: equirectangular UV in [0,1] × [0,1]
//
static float2 spherical_uv(float2 sUV, float z) {
    float3 n = normalize(float3(sUV, z));
    // atan2 gives [-π, π] → remap to [0,1]
    float u = atan2(n.x, n.z) / (2.0 * M_PI_F) + 0.5;
    // asin gives [-π/2, π/2] → remap to [0,1]
    float v = asin(clamp(n.y, -1.0, 1.0)) / M_PI_F + 0.5;
    return float2(u, v);
}

// ── 3. Surface biome color ────────────────────────────────────────────────────
//
//  terrain  — fbm height in [0,1]
//  latitude — abs(sUV.y) in [0,1], used for polar blending
//
static float3 surface_color(float terrain, float latitude) {
    const float3 deep_ocean = float3(0.02, 0.10, 0.40);
    const float3 ocean      = float3(0.05, 0.22, 0.58);
    const float3 shallow    = float3(0.10, 0.40, 0.60);
    const float3 sand       = float3(0.78, 0.68, 0.42);
    const float3 grass      = float3(0.22, 0.52, 0.18);
    const float3 forest     = float3(0.10, 0.35, 0.12);
    const float3 rock       = float3(0.42, 0.38, 0.32);
    const float3 snow_rock  = float3(0.65, 0.62, 0.60);

    float3 col;
    // water layers
    col = mix(deep_ocean, ocean,   smoothstep(0.30, 0.42, terrain));
    col = mix(col, shallow,        smoothstep(0.42, 0.48, terrain));
    // shore
    col = mix(col, sand,           smoothstep(0.48, 0.52, terrain));
    // land biomes
    col = mix(col, grass,          smoothstep(0.52, 0.58, terrain));
    col = mix(col, forest,         smoothstep(0.58, 0.66, terrain));
    col = mix(col, rock,           smoothstep(0.66, 0.76, terrain));
    col = mix(col, snow_rock,      smoothstep(0.76, 0.88, terrain));

    return col;
}

// ── 4. Polar ice caps ─────────────────────────────────────────────────────────
static float3 polar_caps(float3 surface, float2 sUV, float2 sphereUV, float time) {
    float lat = abs(sUV.y);  // 0 at equator, 1 at poles
    // Add fbm noise to make cap edge jagged
    float cap_edge = 0.72 + fbm(sphereUV*8.0 + time*0.01, 3)*0.12 - 0.06;
    float ice = smoothstep(cap_edge, cap_edge + 0.06, lat);
    return mix(surface, float3(0.90, 0.95, 1.00), ice);
}

// ── 5. Cloud layer ────────────────────────────────────────────────────────────
//  draped over spherical UV — wraps correctly
static float3 cloud_layer(float3 surface, float2 sphereUV, float time, float z) {
    float2 cUV = sphereUV + float2(time * 0.012, 0.0);  // clouds drift east
    float cloud = fbm_warped(cUV * 4.0, 5);
    cloud = smoothstep(0.52, 0.72, cloud);
    float shadow = fbm_warped(cUV * 4.0 + float2(0.01, -0.01), 5);
    shadow = smoothstep(0.52, 0.72, shadow) * 0.25;
    surface = mix(surface, surface * (1.0 - shadow), 0.8);
    return mix(surface, float3(0.95, 0.97, 1.00), cloud * (0.7 + 0.3*z));
}

// ── 6. Atmosphere rim ─────────────────────────────────────────────────────────
static float3 atmosphere_rim(float3 col, float z, float3 light_dir, float3 normal) {
    float rim = pow(1.0 - z, 2.5);
    float3 atm_col = mix(float3(0.15, 0.45, 1.0),   // day side: blue
                         float3(0.90, 0.40, 0.10),   // terminator: orange
                         smoothstep(0.0, 0.5, 1.0 - dot(normal, light_dir)));
    return mix(col, atm_col, rim * 0.7);
}

// ── 7. Background star field ──────────────────────────────────────────────────
static float3 stars_background(float2 uv, float time) {
    float3 col = float3(0.0, 0.0, 0.015);
    // Layer 1: dense tiny stars
    float2 g1 = floor(uv * 200.0);
    float  h1 = hash1(g1);
    if (h1 > 0.988) {
        float twinkle = 0.7 + 0.3 * sin(time * (3.0 + h1*8.0) + h1*100.0);
        col += float3(0.6, 0.7, 1.0) * twinkle * (h1 - 0.988) / 0.012;
    }
    // Layer 2: bright foreground stars
    float2 g2 = floor(uv * 80.0);
    float  h2 = hash1(g2 + float2(43.7, 91.1));
    if (h2 > 0.994) {
        float twinkle = 0.6 + 0.4 * sin(time * (2.0 + h2*4.0));
        float bright  = pow((h2 - 0.994) / 0.006, 2.0);
        float3 star_col = mix(float3(1.0,0.8,0.6), float3(0.7,0.8,1.0), fract(h2*7.3));
        col += star_col * twinkle * bright * 3.0;
    }
    return col;
}

// ── 8. Fragment main ──────────────────────────────────────────────────────────

fragment float4 fs_main(Varyings in [[stage_in]],
                        constant EffectUniforms& u [[buffer(0)]]) {
    float2 uv    = in.uv;
    float  time  = u.iTime;
    float2 delta = uv - float2(0.5);
    float  dist  = length(delta * float2(u.iResolution.x / u.iResolution.y, 1.0));

    const float radius = 0.28;

    // Background stars
    float3 col = stars_background(uv, time);

    if (dist < radius) {
        // ── Sphere geometry ───────────────────────────────────────────────────
        float2 sUV = delta / radius;
        sUV.x *= u.iResolution.x / u.iResolution.y;   // aspect correct
        float z = sqrt(max(0.0, 1.0 - dot(sUV, sUV)));

        // ── Correct spherical UV — wraps the WHOLE sphere ─────────────────────
        float2 sphereUV = spherical_uv(sUV, z);
        // Add slow rotation via UV offset (no pole distortion!)
        sphereUV.x = fract(sphereUV.x + time * 0.018);

        // ── Terrain ───────────────────────────────────────────────────────────
        float terrain = fbm_warped(sphereUV * 5.0, 6);

        // ── Surface color ─────────────────────────────────────────────────────
        float3 surface = surface_color(terrain, abs(sUV.y));

        // ── Polar caps ────────────────────────────────────────────────────────
        surface = polar_caps(surface, sUV, sphereUV, time);

        // ── Clouds ────────────────────────────────────────────────────────────
        surface = cloud_layer(surface, sphereUV, time, z);

        // ── Lighting ──────────────────────────────────────────────────────────
        float3 light_dir = normalize(float3(-0.4, 0.5, 0.8));
        float3 normal    = normalize(float3(sUV.x, sUV.y, z));
        float  diff      = max(0.0, dot(normal, light_dir));
        float  spec      = pow(max(0.0, dot(reflect(-light_dir, normal), float3(0,0,1))), 32.0);
        surface *= (0.08 + 0.92 * diff);
        // Specular on ocean only
        float ocean_mask = smoothstep(0.52, 0.48, terrain);
        surface += float3(0.6, 0.7, 0.9) * spec * ocean_mask * diff;

        // ── Atmosphere rim ────────────────────────────────────────────────────
        col = atmosphere_rim(surface, z, light_dir, normal);

    } else {
        // Outer atmosphere glow
        float atmDist = (dist - radius) / radius;
        float glow    = exp(-atmDist * 12.0) * 0.5;
        col += float3(0.15, 0.40, 0.95) * glow;
    }

    return float4(col, 1.0);
}

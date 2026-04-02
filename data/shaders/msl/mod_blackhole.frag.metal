#include <metal_stdlib>
using namespace metal;

struct EffectUniforms { float2 iResolution; float iTime; float _pad; };
struct Varyings { float4 pos [[position]]; float2 uv; };

//  BLACK HOLE SHADER — mod_blackhole.frag.metal
//
//  Self-contained runtime version (sokol compiles from string → no #include).
//  For the clean module versions see:
//    data/shaders/msl/lib/blackhole.metal   ← geodesic, disk, photon ring
//    data/shaders/msl/lib/camera.metal      ← basis, ray gen, orbit
//    data/shaders/msl/lib/noise.metal       ← hash, fbm
//    data/shaders/msl/lib/post.metal        ← ACES tonemapping
//
//  Rendering pipeline (each block is swappable):
//   1.  hash / gnoise / fbm          [lib/noise.metal]
//   2.  star_color()                 [lib/stars.metal]
//   3.  stars_bg()       – 3-layer star field sampled from bent ray direction
//   4.  nebula_bg()      – per-channel fbm nebula (RGB independently displaced)
//   5.  bh_bend_ray()    – Euler integration of Schwarzschild geodesic
//   6.  bh_disk()        – Shakura-Sunyaev disk + Doppler beaming
//   7.  bh_ring()        – photon-sphere glow at r_ph = 1.5 rs
//   8.  CamBasis / orbit_pos / camera_ray   [lib/camera.metal]
//   9.  ACES + gamma     [lib/post.metal]
//  10.  fs_main()        – compose all layers
//
//  Key physics (see lib/blackhole.metal for full derivation):
//   rs   = Schwarzschild radius (event horizon)
//   r_ph = 1.5 rs  (photon sphere — unstable circular photon orbit)
//   ISCO = 3 rs    (innermost stable circular orbit — disk inner edge)
//   Geodesic: d²x/dλ² ≈ −(3/2)(rs²/r³) x̂   (Newtonian-analog deflection)
//   T(r) ∝ r^{-3/4}                           (disk temperature profile)


//  1. Hash / Noise / FBM  [lib/noise.metal]

static float hash1(float2 p) {
    return fract(sin(dot(p, float2(127.1, 311.7))) * 43758.5453);
}
static float2 hash2(float2 p) {
    p = float2(dot(p, float2(127.1, 311.7)), dot(p, float2(269.5, 183.3)));
    return fract(sin(p) * 43758.5453);
}
static float gnoise(float2 p) {
    float2 i = floor(p), f = fract(p);
    float2 u = f * f * (3.0 - 2.0 * f);
    float  a = dot(hash2(i            ) * 2.0 - 1.0, f            );
    float  b = dot(hash2(i + float2(1,0)) * 2.0 - 1.0, f - float2(1,0));
    float  c = dot(hash2(i + float2(0,1)) * 2.0 - 1.0, f - float2(0,1));
    float  d = dot(hash2(i + float2(1,1)) * 2.0 - 1.0, f - float2(1,1));
    return mix(mix(a, b, u.x), mix(c, d, u.x), u.y);
}
static float fbm(float2 p, int oct) {
    float v = 0.0, amp = 0.5;
    for (int i = 0; i < oct; i++) { v += gnoise(p) * amp; p *= 2.0; amp *= 0.5; }
    return v * 0.5 + 0.5;  // [0, 1]
}

//  2. Star blackbody colour  [lib/stars.metal]
//
//  Maps a normalised temperature t ∈ [0,1] to an RGB colour:
//    t = 0  →  red dwarf    (2 000 K)
//    t = 0.4 → sun-like     (5 800 K)
//    t = 1  →  blue giant   (12 000 K)
//  Approximation of Planck blackbody curve for visible wavelengths.
static float3 star_color(float t) {
    float temp = mix(2000.0, 12000.0, t);
    float r  = clamp(1.0 - max(0.0, temp - 6600.0) / 4000.0, 0.4, 1.0);
    float g  = (temp < 6600.0) ? clamp(temp / 6600.0, 0.4, 1.0)
                               : clamp(1.0 - (temp - 6600.0) / 8000.0, 0.5, 1.0);
    float b  = (temp < 7000.0) ? clamp((temp - 2000.0) / 5000.0, 0.0, 1.0) : 1.0;
    return float3(r, g, b);
}

// 3. Background star field
//
//  Samples the star sky from a 3D direction vector.
//  Three layers (density → brightness → giant) for visual depth.
//  Using spherical projection (atan2/asin) → angular UV → hash grid.
static float3 stars_bg(float3 dir, float time) {
    float3 col  = float3(0.0);
    // Stable 2D coordinates from 3D direction
    float2 ang = float2(atan2(dir.y, dir.x) / (2.0 * M_PI_F) + 0.5,
                        asin(clamp(dir.z, -1.0, 1.0)) / M_PI_F + 0.5);

    // Layer 1: dense micro-stars (Milky Way dust)
    {
        float2 g = floor(ang * 420.0);
        float  h = hash1(g);
        if (h > 0.984) {
            float tw = 0.6 + 0.4 * sin(time * (2.0 + h * 5.0) + h * 200.0);
            col += star_color(fract(h * 11.3))
                 * tw * pow((h - 0.984) / 0.016, 1.5) * 0.4;
        }
    }
    // Layer 2: bright foreground stars with 4-spike diffraction cross
    {
        float2 g   = floor(ang * 140.0);
        float2 f   = fract(ang * 140.0) - 0.5;
        float  h   = hash1(g + float2(17.3, 42.7));
        if (h > 0.993) {
            float2 jit    = hash2(g) - 0.5;
            float2 dd     = f - jit;
            float  r      = length(dd);
            float  tw     = 0.7 + 0.3 * sin(time * (1.5 + h * 4.0));
            float  bright = pow((h - 0.993) / 0.007, 2.0) * 4.0;
            float3 sc     = star_color(fract(h * 7.1));
            col += sc * exp(-r * 50.0) * bright * tw;
            // Diffraction spikes
            float sx = exp(-abs(dd.y) * 110.0) * exp(-abs(dd.x) * 9.0);
            float sy = exp(-abs(dd.x) * 110.0) * exp(-abs(dd.y) * 9.0);
            col += sc * (sx + sy) * bright * tw * 0.3;
        }
    }
    // Layer 3: giant stars with soft corona + ring glow
    {
        float2 g   = floor(ang * 38.0);
        float2 f   = fract(ang * 38.0) - 0.5;
        float  h   = hash1(g + float2(99.1, 3.3));
        if (h > 0.92) {
            float2 jit = hash2(g) - 0.5;
            float  r   = length(f - jit);
            float  tw  = 0.8 + 0.2 * sin(time * (0.4 + h * 1.5));
            float  br  = pow((h - 0.92) / 0.08, 3.0) * 6.0;
            float3 sc  = star_color(fract(h * 3.7));
            col += sc * exp(-r * 18.0) * br * tw;
            col += sc * exp(-abs(r - 0.04) * 60.0) * br * 0.25 * tw;   // ring 1
            col += sc * exp(-abs(r - 0.09) * 45.0) * br * 0.10 * tw;   // ring 2
        }
    }
    return col;
}

// 4. Background nebula  [lib/noise.metal]
//
//  Per-channel fbm with independent seeds → natural RGB colour variance.
//  Cluster mask shapes the nebula into cloud regions (not uniform).
static float3 nebula_bg(float3 dir, float time) {
    float2 ang = float2(atan2(dir.y, dir.x) / (2.0 * M_PI_F) + 0.5,
                        asin(clamp(dir.z, -1.0, 1.0)) / M_PI_F + 0.5);
    float2 p       = ang * 3.2 + time * 0.004;
    float  cluster = smoothstep(0.30, 0.65, fbm(ang * 1.8, 4));
    float  nr = fbm(p,                   6);
    float  ng = fbm(p + float2(1.7, 9.2), 6);
    float  nb = fbm(p + float2(8.3, 2.8), 6);
    nr = smoothstep(0.46, 0.68, nr);
    ng = smoothstep(0.48, 0.70, ng);
    nb = smoothstep(0.44, 0.66, nb);
    return float3(nr, ng * 0.65, nb) * cluster * 0.40;
}

// 5. Geodesic ray bending  [lib/blackhole.metal]
//
//  Euler integration of  d²x/dλ² = −(3/2)(rs²/r³) x̂
//  where λ is the affine parameter along the photon path.
//
//  Accuracy vs. performance:
//    Larger dt  → cheaper but misses sharp photon ring features.
//    Smaller dt → more accurate, higher cost.
//    dt=0.07, 80 steps covers ~5.6 units → adequate for rs=0.6, eye at r=6.
static float3 bh_bend_ray(float3 ro, float3 rd, float rs, thread bool& absorbed) {
    absorbed   = false;
    float3 pos = ro;
    float3 dir = normalize(rd);
    const float dt = 0.07;

    for (int i = 0; i < 80; i++) {
        float  r = length(pos);
        if (r < rs * 0.55) { absorbed = true; return float3(0.0); }

        // Deflection: gravitational pull toward origin
        float3 accel = -normalize(pos) * (1.5 * rs * rs) / (r * r * r);
        dir  = normalize(dir + accel * dt);  // keep |dir|=1 for stability
        pos += dir * dt;

        if (r > length(ro) * 1.5 + 8.0) break;  // escaped to background
    }
    return dir;
}

// 6. Accretion disk  [lib/blackhole.metal]
//
//  Shakura-Sunyaev temperature gradient: T(r) ∝ (1 − r_in/r)^{3/4}
//  Disk lives in y=0 plane (intersect with t = −ro.y / rd.y).
static float3 bh_disk(float3 ro, float3 rd, float rs, float time) {
    if (abs(rd.y) < 1e-4) return float3(0.0);
    float  t_hit = -ro.y / rd.y;
    if (t_hit < 0.0) return float3(0.0);

    float3 hit  = ro + rd * t_hit;
    float  r    = length(hit.xz);
    float  r_in = rs * 1.5, r_out = rs * 10.0;
    if (r < r_in || r > r_out) return float3(0.0);

    float rho   = (r - r_in) / (r_out - r_in);
    float temp  = pow(1.0 - rho, 0.75);

    float3 hot  = float3(0.82, 0.88, 1.00);
    float3 warm = float3(1.00, 0.52, 0.12);
    float3 cool = float3(0.55, 0.04, 0.02);
    float3 col  = (temp > 0.5) ? mix(warm, hot,  (temp - 0.5) * 2.0)
                               : mix(cool, warm,  temp        * 2.0);

    // Relativistic Doppler beaming: approaching side boosted
    float phi     = atan2(hit.z, hit.x) + time * 0.5;
    float doppler = 0.55 + 0.45 * sin(phi);
    float beaming = mix(0.15, 2.8, doppler * doppler);

    // Disk vertical visibility:
    //   Looking edge-on (|rd.y| → 0): long path through hot gas → bright.
    //   Looking face-on (|rd.y| → 1): thin slab intersection → dimmer.
    //   smoothstep(0.7, 0.0, |rd.y|) maps edge-on→1, face-on→0.
    float  thick = smoothstep(0.65, 0.0, abs(rd.y));

    return col * temp * temp * beaming * thick * 3.5;
}

// 7. Photon sphere ring  [lib/blackhole.metal]
//
//  Bright glow centred on impact parameter  b = r_ph = 1.5 rs.
//  Uses the un-bent ray for stable visual (avoids feedback issues).
static float3 bh_ring(float3 ro, float3 rd, float rs) {
    float  b     = length(cross(ro, normalize(rd)));
    float  r_ph  = rs * 1.5;
    float  ring  = exp(-pow((b - r_ph) / (rs * 0.11), 2.0));
    float3 col   = mix(float3(1.0, 0.65, 0.25), float3(0.65, 0.90, 1.0),
                       smoothstep(0.0, 1.0, b / (r_ph * 1.3)));
    return col * ring * 1.6;
}

// 8. Camera utilities  [lib/camera.metal]

struct CamBasis { float3 right, up, fwd; };

static CamBasis camera_basis(float3 eye, float3 target, float3 world_up) {
    CamBasis b;
    b.fwd   = normalize(target - eye);
    b.right = normalize(cross(b.fwd, world_up));
    b.up    = cross(b.right, b.fwd);
    return b;
}
static float3 camera_ray(CamBasis b, float2 uv, float fov_rad) {
    float fl = 1.0 / tan(fov_rad * 0.5);
    return normalize(b.fwd * fl + uv.x * b.right + uv.y * b.up);
}
// Smooth cinematic orbit on a tilted ellipse
static float3 orbit_pos(float3 target, float radius, float tilt,
                        float time, float speed) {
    float a = time * speed;
    return target + float3(cos(a) * radius,
                           sin(tilt) * sin(a) * radius,
                           cos(tilt) * sin(a) * radius);
}

// ── 9. ACES tonemapping  [lib/post.metal] ────────────────────────────────────
//
//  Academy Color Encoding System (ACES) filmic curve.
//  Maps HDR linear radiance → LDR display value, preserving colour saturation.
static float3 ACES(float3 x) {
    return clamp((x * (2.51 * x + 0.03)) / (x * (2.43 * x + 0.59) + 0.14),
                 0.0, 1.0);
}

// 10. Fragment main — compose layers

fragment float4 fs_main(Varyings in [[stage_in]],
                        constant EffectUniforms& u [[buffer(0)]]) {
    float2 uv   = in.uv * 2.0 - 1.0;
    uv.x *= u.iResolution.x / u.iResolution.y;
    float  time = u.iTime * 0.22;

    // Camera orbit
    //  Camera circles the black hole on a slightly tilted ellipse.
    //  tilt = 0.4 rad → we see the disk at ≈23° inclination (like M87*).
    float3 target   = float3(0.0);
    float3 eye      = orbit_pos(target, 6.0, 0.4, time, 0.16);
    float3 world_up = normalize(float3(0.0, 1.0, 0.15));  // slight roll
    CamBasis basis  = camera_basis(eye, target, world_up);
    float3 rd0      = camera_ray(basis, uv, M_PI_F * 0.52);  // 94° FoV

    // Schwarzschild radius
    const float rs = 0.6;

    // Bend the ray through the gravitational field
    bool   absorbed = false;
    float3 rd_bent  = bh_bend_ray(eye, rd0, rs, absorbed);

    float3 col = float3(0.0);

    if (absorbed) {
        // Ray fell past event horizon → pure black.
        // Add a faint thermal glow just inside (Hawking radiation — artistic).
        float b = length(cross(eye, normalize(rd0)));
        col = float3(0.08, 0.03, 0.01) * exp(-pow(b / rs - 1.0, 2.0) * 20.0);

    } else {
        // Background: lensed nebula + star field
        col  = nebula_bg(rd_bent, time * 0.4);
        col += stars_bg(rd_bent, time * 0.4);

        // Accretion disk emission
        col += bh_disk(eye, rd_bent, rs, time);

        // Photon ring glow (on original ray — gives stable bright ring)
        col += bh_ring(eye, rd0, rs);
    }

    // Outer atmosphere / jet glow
    //  Faint polar jets (along ±Y) — seen in many active galactic nuclei.
    {
        float3 jet_axis = normalize(float3(0.0, 1.0, 0.0));
        float  jet_dist = length(rd0 - dot(rd0, jet_axis) * jet_axis);
        float  jet_para = abs(dot(rd0, jet_axis));
        float  jet      = exp(-jet_dist * 8.0) * exp(-pow(jet_para - 0.95, 2.0) * 30.0)
                        * smoothstep(0.8, 1.0, jet_para);
        col += float3(0.5, 0.8, 1.0) * jet * 0.5;
    }

    // Tone map + gamma
    col = ACES(col);
    col = pow(max(col, 0.0), float3(0.4545));  // γ = 2.2 decoding

    return float4(col, 1.0);
}


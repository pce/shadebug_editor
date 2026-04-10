#include <metal_stdlib>
using namespace metal;

struct EffectUniforms { float2 iResolution; float iTime; float _pad; };
struct Varyings { float4 pos [[position]]; float2 uv; };

// ═══════════════════════════════════════════════════════════════════════════════
//  DEPTH-OF-FIELD CAMERA SHADER — mod_dof.frag.metal
//
//  Self-contained runtime version (sokol compiles from string → no #include).
//  For the clean module versions see:
//    data/shaders/msl/lib/camera.metal      ← thin-lens DoF theory + implementation
//    data/shaders/msl/lib/noise.metal       ← hash, fbm
//    data/shaders/msl/lib/post.metal        ← ACES tonemapping
//
//  Rendering pipeline (each block is swappable):
//   1.  hash / gnoise / fbm          [lib/noise.metal]
//   2.  golden_disk()                [lib/camera.metal]  — aperture sampling
//   3.  CamBasis / camera_ray()      [lib/camera.metal]  — ray generation
//   4.  aperture_ray()               [lib/camera.metal]  — thin-lens jitter
//   5.  orbit_pos()                  [lib/camera.metal]  — cinematic camera
//   6.  scene()                      — 3 glowing orbs + background
//   7.  nebula_bg() / stars_bg()     [lib/noise.metal / lib/stars.metal]
//   8.  ACES + gamma                 [lib/post.metal]
//   9.  fs_main()                    — DoF accumulation loop
//
//  Depth of Field (thin-lens model):
//  ──────────────────────────────────
//   Pinhole: one ray per pixel → everything sharp.
//   Thin lens with aperture A:
//     • Rays from the focal plane converge exactly → sharp.
//     • Rays from depth d ≠ focal spread into a disk of radius
//         CoC = A · f · |d − focal| / (d · |focal − f|)
//       where f = focal length.
//   Implementation (Monte-Carlo):
//     • Sample N origins on the aperture disk using golden-angle spiral.
//     • Each origin sends its ray to the SAME focal-plane point.
//     • Average N colour samples → smooth bokeh with no temporal reprojection.
//   Focal sweep: focal_dist oscillates slowly with iTime to demonstrate
//     which object is in focus at any given moment.
//
//  Swap ideas (comment/uncomment in fs_main):
//   • Change N_SAMPLES: 4 = fast but grainy; 32 = smooth but heavy
//   • Change APERTURE:  0.02 = subtle;  0.40 = extreme cinema look
//   • Change scene():   replace orbs with raymarch geometry
//   • Change focal animation: use mouse (add iMouse to EffectUniforms)
// ═══════════════════════════════════════════════════════════════════════════════

// ── 1. Hash / Noise / FBM  [lib/noise.metal] ─────────────────────────────────

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
    return v * 0.5 + 0.5;
}

//  2. Golden-angle disk sampler  [lib/camera.metal]
//
//  Places N samples on the unit disk with minimal clumping for any N.
//  Golden angle = 2π(1−1/φ) ≈ 2.3999632 rad (φ = golden ratio).
//  Radius: sqrt((i+0.5)/N) gives uniform area distribution.
//
//  index : current sample [0, n)
//  n     : total samples
static float2 golden_disk(int index, int n) {
    const float GOLDEN_ANGLE = 2.399963229;
    float r   = sqrt((float(index) + 0.5) / float(n));
    float phi = float(index) * GOLDEN_ANGLE;
    return float2(cos(phi), sin(phi)) * r;
}

// 3. Camera utilities  [lib/camera.metal]

struct CamBasis { float3 right, up, fwd; };

static CamBasis camera_basis(float3 eye, float3 target, float3 world_up) {
    CamBasis b;
    b.fwd   = normalize(target - eye);
    b.right = normalize(cross(b.fwd, world_up));
    b.up    = cross(b.right, b.fwd);
    return b;
}
// Pinhole primary ray
static float3 camera_ray(CamBasis b, float2 uv, float fov_rad) {
    float fl = 1.0 / tan(fov_rad * 0.5);
    return normalize(b.fwd * fl + uv.x * b.right + uv.y * b.up);
}

//  4. Thin-lens aperture ray  [lib/camera.metal]
//
//  1. Find focal_pt = where pinhole ray hits the focal plane.
//  2. Jitter origin on aperture disk (disk_offset ∈ unit disk, scaled by aperture).
//  3. New direction = normalise(focal_pt − jittered_origin).
//  All aperture samples converge at focal_pt → focal plane stays sharp.
//  Other depths receive a spread of directions → blur proportional to |d−focal|.
static float3 aperture_ray(float3 ro_base, float3 rd_pin,
                            CamBasis basis,  float   focal_dist,
                            float aperture,  float2  disk_off,
                            thread float3&   ro_out) {
    float3 focal_pt = ro_base + rd_pin * focal_dist;
    ro_out = ro_base + (disk_off.x * basis.right + disk_off.y * basis.up) * aperture;
    return normalize(focal_pt - ro_out);
}

//  5. Camera orbit  [lib/camera.metal]
static float3 orbit_pos(float3 target, float radius, float tilt,
                        float time, float speed) {
    float a = time * speed;
    return target + float3(cos(a) * radius,
                           sin(tilt) * sin(a) * radius,
                           cos(tilt) * sin(a) * radius);
}

//  6. Scene: 3 glowing orbs at different depths + background
//
//  Orbs are coloured spheres with lambertian + specular lighting and an
//  additive glow halo.  Exact sphere intersection gives correct depth
//  for thin-lens DoF.
//
//  Depths (camera-space z, camera at z=−3 looking toward +z):
//    Orb 0 — NEAR   z ≈  4  (orange, small)
//    Orb 1 — MID    z ≈  9  (cyan,   medium)
//    Orb 2 — FAR    z ≈ 17  (violet, large)

struct SceneResult { float3 col; float depth; };

// Evaluate a single sphere's contribution to a ray.
// Returns the hit distance (1e9 if no hit) and adds emission to col.
static float hit_orb(float3 ro, float3 rd,
                     float3 center, float radius,
                     float3 orb_col,
                     thread float3& col_out) {
    float3 oc   = ro - center;
    float  a    = dot(rd, rd);
    float  half_b = dot(oc, rd);
    float  c    = dot(oc, oc) - radius * radius;
    float  disc = half_b * half_b - a * c;
    if (disc < 0.0) return 1e9;

    float sq = sqrt(disc);
    float t  = (-half_b - sq) / a;
    if (t < 0.0) t = (-half_b + sq) / a;
    if (t < 0.0) return 1e9;

    float3 hit_pt  = ro + rd * t;
    float3 normal  = normalize(hit_pt - center);
    float3 light   = normalize(float3(1.2, 2.0, 0.6));
    float  diff    = max(0.0, dot(normal, light));
    float  spec    = pow(max(0.0, dot(reflect(-light, normal), -rd)), 48.0);

    col_out = orb_col * (0.08 + 0.92 * diff) + float3(0.6, 0.7, 0.9) * spec * 0.5;
    return t;
}

// Background star field (angular hash)
static float3 stars_bg(float3 rd, float time) {
    float3 col  = float3(0.0);
    float2 ang = float2(atan2(rd.y, rd.x) / (2.0 * M_PI_F) + 0.5,
                        asin(clamp(rd.z, -1.0, 1.0)) / M_PI_F + 0.5);
    // Tiny dense stars
    float2 g1 = floor(ang * 360.0);
    float  h1 = hash1(g1);
    if (h1 > 0.984) {
        float tw = 0.6 + 0.4 * sin(time * (2.0 + h1 * 5.0) + h1 * 200.0);
        col += float3(0.5, 0.6, 0.9) * tw * pow((h1 - 0.984) / 0.016, 1.5) * 0.4;
    }
    // Bright foreground stars
    float2 g2 = floor(ang * 120.0);
    float2 f2 = fract(ang * 120.0) - 0.5;
    float  h2 = hash1(g2 + float2(17.3, 42.7));
    if (h2 > 0.993) {
        float2 jit = hash2(g2) - 0.5;
        float  r2  = length(f2 - jit);
        float  tw  = 0.7 + 0.3 * sin(time * (1.5 + h2 * 4.0));
        float3 sc  = mix(float3(1.0, 0.85, 0.6), float3(0.6, 0.8, 1.0), fract(h2 * 7.3));
        col += sc * exp(-r2 * 55.0) * pow((h2 - 0.993) / 0.007, 2.0) * 4.0 * tw;
    }
    return col;
}

// Background nebula
static float3 nebula_bg(float3 rd, float time) {
    float2 ang = float2(atan2(rd.y, rd.x) / (2.0 * M_PI_F) + 0.5,
                        asin(clamp(rd.z, -1.0, 1.0)) / M_PI_F + 0.5);
    float2 p = ang * 3.0 + time * 0.003;
    float  cluster = smoothstep(0.30, 0.62, fbm(ang * 1.7, 4));
    float  nr = fbm(p,                   6);
    float  ng = fbm(p + float2(2.1, 8.7), 6);
    float  nb = fbm(p + float2(7.4, 3.2), 6);
    nr = smoothstep(0.44, 0.66, nr);
    ng = smoothstep(0.46, 0.68, ng);
    nb = smoothstep(0.42, 0.64, nb);
    return float3(nr * 0.9, ng * 0.55, nb) * cluster * 0.40;
}

// Full scene: return best (nearest) hit colour + depth
static SceneResult scene(float3 ro, float3 rd, float time) {
    SceneResult res;
    res.col   = float3(0.0);
    res.depth = 1e9;

    // Orb definitions: center, radius, colour
    float3 orb_centers[3] = {
        float3( 0.20, -0.10,  4.0),   // near  — orange
        float3(-0.35,  0.20,  9.0),   // mid   — cyan
        float3( 0.50, -0.25, 17.0)    // far   — violet
    };
    float orb_radii[3] = { 0.40, 0.55, 0.90 };
    float3 orb_colors[3] = {
        float3(1.0, 0.55, 0.10),   // orange
        float3(0.1, 0.80, 0.90),   // cyan
        float3(0.7, 0.20, 1.00)    // violet
    };

    for (int i = 0; i < 3; i++) {
        float3 hit_col = float3(0.0);
        float  t = hit_orb(ro, rd,
                           orb_centers[i], orb_radii[i],
                           orb_colors[i],
                           hit_col);
        if (t < res.depth) {
            res.depth = t;
            res.col   = hit_col;
        }

        // Additive glow halo around each orb (independent of depth sort)
        float3 oc   = orb_centers[i] - ro;
        float  t_cl = dot(oc, rd);
        if (t_cl > 0.0) {
            float  d_cl = length(ro + rd * t_cl - orb_centers[i]);
            float  glow = exp(-pow(d_cl / (orb_radii[i] * 2.5), 2.0))
                        * exp(-pow(d_cl / (orb_radii[i] * 0.6), 2.0) * 0.3);
            res.col += orb_colors[i] * glow * 0.5;
        }
    }

    // Deep-space background (infinite depth — never closer than any orb)
    res.col += nebula_bg(rd, time);
    res.col += stars_bg(rd, time);

    return res;
}

// ── 7. ACES tonemapping  [lib/post.metal] ────────────────────────────────────
static float3 ACES(float3 x) {
    return clamp((x * (2.51 * x + 0.03)) / (x * (2.43 * x + 0.59) + 0.14),
                 0.0, 1.0);
}

// 8. Fragment main — thin-lens DoF accumulation

fragment float4 fs_main(Varyings in [[stage_in]],
                        constant EffectUniforms& u [[buffer(0)]]) {
    float2 uv   = in.uv * 2.0 - 1.0;
    uv.x *= u.iResolution.x / u.iResolution.y;
    float  time = u.iTime * 0.3;

    // ── Camera ────────────────────────────────────────────────────────────────
    //  Static position — looking along +Z toward the three orbs.
    //  Slight orbit so the scene isn't perfectly flat.
    float3 target   = float3(0.0, 0.0, 9.0);
    float3 eye      = orbit_pos(target, 0.4, 0.12, time, 0.25)
                    + float3(0.0, 0.0, -12.0);

    CamBasis basis  = camera_basis(eye, target, float3(0.0, 1.0, 0.0));
    float3 rd0      = camera_ray(basis, uv, M_PI_F / 3.0);   // 60° FoV

    //  Thin-lens parameters
    //  focal_dist sweeps: near orb (4) → far orb (17) over ~21 seconds.
    //  Watch the depth rack change as focal distance crosses each orb depth.
    float  focal_dist = mix(4.5, 17.5, 0.5 + 0.5 * sin(u.iTime * 0.25));

    // Aperture:
    //   0.0  = pinhole (no blur)
    //   0.08 = cinematic prime lens look
    //   0.25 = aggressive portrait/macro blur
    const float APERTURE  = 0.10;

    // Samples: more samples → smoother bokeh disks, higher GPU cost.
    //   8 = fast, some visible noise in out-of-focus regions
    //  16 = good balance (default)
    //  32 = smooth, matches offline renders closely
    const int N_SAMPLES = 16;

    // Accumulate N aperture-jittered samples
    float3 acc = float3(0.0);
    for (int i = 0; i < N_SAMPLES; i++) {
        float2 disk = golden_disk(i, N_SAMPLES);  // unit disk sample
        float3 ro_j;
        float3 rd_j = aperture_ray(eye, rd0, basis, focal_dist,
                                   APERTURE, disk, ro_j);
        SceneResult s = scene(ro_j, rd_j, time);
        acc += s.col;
    }
    float3 col = acc / float(N_SAMPLES);

    //  Subtle in-focus sharpening vignette (optional, comment to remove)
    //  Slightly boost saturation near the focal plane to mimic lens microcontrast.
    float lum    = dot(col, float3(0.299, 0.587, 0.114));
    col          = mix(float3(lum), col, 1.08);  // mild saturation boost

    // Tone map + gamma
    col = ACES(col);
    col = pow(max(col, 0.0), float3(0.4545));  // γ = 2.2 decode

    return float4(col, 1.0);
}


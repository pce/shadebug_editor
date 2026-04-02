// ═══════════════════════════════════════════════════════════════════════════════
//  lib/blackhole.metal  — Black Hole Physics Module
//
//  #pragma once + pure functions → your Metal "module" system.
//  Include this in any raymarching shader:
//      #include "lib/blackhole.metal"
//
//  Contents
//  ────────
//   bh_bend_ray()         – integrate photon geodesic (Schwarzschild metric)
//   bh_accretion_disk()   – thermal disk with Doppler beaming
//   bh_photon_ring()      – glow at the photon sphere (r = 1.5 rs)
//   bh_shadow_mask()      – shadow disk inside the photon capture radius
//
//  Physics background
//  ──────────────────
//   Schwarzschild metric (non-rotating black hole):
//     ds² = -(1-rs/r)c²dt² + (1-rs/r)⁻¹dr² + r²dΩ²
//     rs = 2GM/c²  (Schwarzschild radius — "event horizon" for r=rs)
//
//   Photon sphere:  r_ph = 1.5 rs
//     Photons here orbit in unstable circular paths.
//     Rays with impact parameter b ≈ r_ph either escape or are captured.
//     This creates the bright "ring" around the shadow in real BH images.
//
//   Photon capture: b < b_crit = r_ph / sqrt(1 - rs/r_ph) ≈ 2.6 rs
//     Rays within this impact parameter fall into the hole.
//
//   Accretion disk (Shakura-Sunyaev thin disk model):
//     Temperature: T(r) ∝ r^{-3/4}
//       Inner edge (ISCO): r_ISCO = 3 rs (for Schwarzschild BH)
//       Here we use r_in = 1.5 rs for visual richness.
//     Colour: T → blue-white (inner, ~10⁴ K) to red (outer, ~2000 K)
//
//   Doppler beaming:
//     The disk gas orbits at relativistic speeds (v ~ 0.1–0.5 c near ISCO).
//     The approaching side is Doppler-boosted → brighter.
//     The receding side is de-boosted → dimmer.
//     Observed intensity: I_obs ∝ (ν_obs/ν_emit)⁴ ≈ (1 + v·cosφ/c)⁴
//     Approximated here as a smooth sinusoidal gain based on azimuth.
//
//   Lensing implementation (bh_bend_ray):
//     We numerically integrate the geodesic equation:
//       d²x/dλ² = -(3/2)(rs/r²) · (x/r)
//     This is the Newtonian-analog force that correctly captures ray deflection
//     for impact parameters b >> rs and qualitatively correct for b ~ rs.
//     Step size dt chosen small enough that adjacent pixel rays give
//     smooth continuous lensing (dt ≈ 0.07 for rs ≈ 0.5).
//
// ═══════════════════════════════════════════════════════════════════════════════
#pragma once
#include <metal_stdlib>
using namespace metal;

/// Number of geodesic integration steps.
/// More steps → more accurate bending at the cost of performance.
/// 80 steps with dt=0.07 covers ~5.6 units of path (good for rs=0.5, eye at r=6).
constant int BH_STEPS = 80;

// ── Geodesic ray bending ──────────────────────────────────────────────────────

/// Integrate photon path through a Schwarzschild black hole at the ORIGIN.
///
///   ro       : ray origin (camera position, should be at r >> rs)
///   rd       : initial ray direction (will be normalised internally)
///   rs       : Schwarzschild radius (event horizon size)
///   absorbed : OUTPUT — set to true if ray crosses the event horizon
///   Returns: final bent ray direction (unit vector pointing toward background)
///
/// Algorithm: Euler integration of  d²x/dλ² = −(3/2)(rs²/r³) x̂
///   Each step: dir += accel*dt;  dir = normalise(dir);  pos += dir*dt
///   This preserves |dir|=1 and gives stable integration.
///
float3 bh_bend_ray(float3 ro, float3 rd, float rs, thread bool& absorbed) {
    absorbed   = false;
    float3 pos = ro;
    float3 dir = normalize(rd);
    const float dt = 0.07;

    for (int i = 0; i < BH_STEPS; i++) {
        float  r = length(pos);

        // Absorption: ray crosses event horizon
        if (r < rs * 0.55) {
            absorbed = true;
            return float3(0.0);
        }

        // Gravitational deflection: approximate geodesic equation
        // accel ∝ 1/r³ toward origin, scaled by rs² (coupling strength)
        float3 accel = -normalize(pos) * (1.5 * rs * rs) / (r * r * r);

        // Euler step (keep direction normalised for numerical stability)
        dir  = normalize(dir + accel * dt);
        pos += dir * dt;

        // Early exit: ray has escaped to background (negligible lensing remains)
        if (r > length(ro) * 1.5 + 8.0) break;
    }
    return dir;
}

// ── Accretion disk ────────────────────────────────────────────────────────────

/// Evaluate emission from an accretion disk in the y = 0 plane.
///
///   ro   : ray origin
///   rd   : BENT ray direction (from bh_bend_ray)
///   rs   : Schwarzschild radius
///   time : animation time (drives Doppler spin)
///
/// Physical model:
///   • Disk geometry: annulus in y=0, r_in = 1.5 rs … r_out = 10 rs
///   • Temperature gradient: T ∝ (1 − r_in/r)^{3/4}  (Shakura-Sunyaev)
///   • Color: map T → blue-white (hot inner) → orange → deep red (cool outer)
///   • Doppler beaming: approaching side (sin φ > 0) brightened, receding dimmed
///   • Vertical profile: thin Gaussian (height ≈ 0.08 rs)
///
float3 bh_accretion_disk(float3 ro, float3 rd, float rs, float time) {
    // Intersect with the disk plane (y = 0)
    if (abs(rd.y) < 1e-4) return float3(0.0);  // ray nearly parallel to disk
    float  t_hit = -ro.y / rd.y;
    if (t_hit < 0.0)       return float3(0.0);  // intersection behind camera

    float3 hit   = ro + rd * t_hit;
    float  r     = length(hit.xz);

    // ISCO (innermost stable circular orbit) and outer edge
    float  r_in  = rs * 1.5;
    float  r_out = rs * 10.0;
    if (r < r_in || r > r_out) return float3(0.0);

    // Shakura-Sunyaev temperature profile: hotter at inner edge
    float rho  = (r - r_in) / (r_out - r_in);       // 0 = inner, 1 = outer
    float temp = pow(1.0 - rho, 0.75);               // ≈1 inner, ≈0 outer

    // Colour temperature: blue-white → orange → deep red
    float3 hot  = float3(0.82, 0.88, 1.00);          // ≈14 000 K  blue-white
    float3 warm = float3(1.00, 0.52, 0.12);           // ≈  5 000 K  orange
    float3 cool = float3(0.55, 0.04, 0.02);           // ≈  2 000 K  deep red
    float3 disk_col = (temp > 0.5)
        ? mix(warm, hot,  (temp - 0.5) * 2.0)
        : mix(cool, warm,  temp        * 2.0);

    // Relativistic Doppler beaming
    //   Disk rotates counter-clockwise (positive z → negative x in XZ plane).
    //   Approaching side: sin(φ) > 0  → brightened by (1+β)⁴ factor.
    float  phi     = atan2(hit.z, hit.x) + time * 0.5;  // spin animation
    float  doppler = 0.55 + 0.45 * sin(phi);             // [0.1, 1.0]
    float  beaming = mix(0.2, 2.5, doppler * doppler);   // quadratic boost

    // Disk vertical visibility:
    //   Edge-on (|rd.y| → 0): long path through hot gas → bright ring.
    //   Face-on (|rd.y| → 1): thin slab → dimmer.
    float  thick = smoothstep(0.65, 0.0, abs(rd.y));

    float emit = temp * temp * beaming * thick * 3.0;
    return disk_col * emit;
}

// ── Photon sphere ring ────────────────────────────────────────────────────────

/// Additive glow at the photon sphere radius  r_ph = 1.5 rs.
///
///   ro, rd : ray origin and UN-BENT direction
///   rs     : Schwarzschild radius
///
/// Uses the impact parameter  b = |ro × rd| / |rd|
/// (perpendicular distance from the black hole centre to the ray line).
/// The photon ring is a bright glow centred at b = r_ph.
///
float3 bh_photon_ring(float3 ro, float3 rd, float rs) {
    float  b    = length(cross(ro, normalize(rd)));
    float  r_ph = rs * 1.5;
    float  ring = exp(-pow((b - r_ph) / (rs * 0.12), 2.0));

    // Colour: heated plasma near photon sphere — warm orange to bluish white
    float3 ring_col = mix(float3(1.0, 0.7, 0.3),
                          float3(0.7, 0.9, 1.0),
                          smoothstep(0.0, 1.0, b / (r_ph * 1.2)));
    return ring_col * ring * 1.4;
}

// ── Shadow mask ───────────────────────────────────────────────────────────────

/// Returns 1.0 for rays fully inside the black hole shadow, 0.0 outside.
/// The shadow edge is at the photon capture impact parameter  b_crit ≈ 2.6 rs.
float bh_shadow(float3 ro, float3 rd, float rs) {
    float b      = length(cross(ro, normalize(rd)));
    float b_crit = rs * 2.6;
    return 1.0 - smoothstep(b_crit * 0.9, b_crit * 1.1, b);
}


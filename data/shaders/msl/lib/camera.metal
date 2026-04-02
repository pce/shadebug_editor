// ═══════════════════════════════════════════════════════════════════════════════
//  lib/camera.metal  — Camera Module
//
//  #pragma once + pure functions → your Metal "module" system.
//  Include this in any raymarching shader:
//      #include "lib/camera.metal"
//
//  Contents
//  ────────
//   CamBasis           – orthonormal camera frame (right, up, fwd)
//   camera_basis()     – build frame from eye + look-at + world-up
//   camera_ray()       – generate primary ray direction from NDC UV
//   orbit_pos()        – smooth cinematic orbit around a focus point
//   golden_disk()      – Poisson/golden-angle unit-disk sample  (N samples)
//   aperture_ray()     – thin-lens jittered ray for Depth of Field
//
//  Depth of Field (DoF) quick theory
//  ───────────────────────────────────
//   Pinhole camera:  single ray per pixel → perfectly sharp everywhere.
//
//   Thin lens:  the lens aperture has a finite radius A.
//     • Rays from the focal plane converge exactly on the sensor  → sharp.
//     • Rays from other depths defocus into a "circle of confusion" (CoC).
//     • CoC radius ≈ A · f · |depth − focal| / (depth · |focal − f|)
//       where f = focal length, A = aperture, focal = focus distance.
//
//   Implementation (Monte-Carlo):
//     For each pixel, sample N ray origins uniformly on the aperture disk.
//     Every jittered ray points toward the SAME focal-plane point as the
//     pinhole ray.  Average N colours → smooth bokeh.
//
//   Low-discrepancy sampling:
//     Random jitter produces variance (noise).  The golden-angle spiral
//     (golden_disk) places N samples with minimal clumping for any N,
//     giving smooth bokeh without temporal accumulation.
//
// ═══════════════════════════════════════════════════════════════════════════════
#pragma once
#include <metal_stdlib>
using namespace metal;

// ── Camera frame ──────────────────────────────────────────────────────────────

/// Orthonormal camera basis.
struct CamBasis {
    float3 right;   // +X  (screen right)
    float3 up;      // +Y  (screen up)
    float3 fwd;     // +Z  (into the scene / view direction)
};

/// Build a camera basis from eye position, look-at target, and world-up vector.
///   world_up = float3(0,1,0)  for Y-up scenes (default).
///   Returns an orthonormal frame.
CamBasis camera_basis(float3 eye, float3 target, float3 world_up) {
    CamBasis b;
    b.fwd   = normalize(target - eye);
    b.right = normalize(cross(b.fwd, world_up));
    b.up    = cross(b.right, b.fwd);   // already unit length
    return b;
}

// ── Ray generation ────────────────────────────────────────────────────────────

/// Generate a pinhole (primary) ray direction for a given screen-space UV.
///
///   uv_ndc  : aspect-corrected NDC coordinates:
///               uv.x ∈ [-aspect, +aspect],  uv.y ∈ [-1, +1]
///             Typical setup:
///               float2 uv = in.uv * 2.0 - 1.0;
///               uv.x *= iResolution.x / iResolution.y;
///
///   fov_rad : vertical field of view in radians.
///             e.g. 60° = M_PI_F/3,  90° = M_PI_F/2
///
float3 camera_ray(CamBasis b, float2 uv_ndc, float fov_rad) {
    // focal_length maps [-1,+1] vertical range onto the film plane at unit distance
    float focal_length = 1.0 / tan(fov_rad * 0.5);
    return normalize(b.fwd * focal_length
                   + uv_ndc.x * b.right
                   + uv_ndc.y * b.up);
}

// ── Cinematic orbit ───────────────────────────────────────────────────────────

/// Smooth camera orbit — camera circles `target` on a tilted ellipse.
///
///   target  : the point the camera orbits around
///   radius  : orbit radius
///   tilt    : orbital plane tilt from XZ plane (radians); 0 = flat orbit
///   time    : animation time (from iTime)
///   speed   : angular speed (rad/sec)
///
float3 orbit_pos(float3 target, float radius, float tilt, float time, float speed) {
    float a  = time * speed;
    float ca = cos(a), sa = sin(a);
    // Tilted ellipse: full circle in XZ, scaled vertical by sin(tilt)
    return target + float3(ca * radius,
                           sa * sin(tilt) * radius,
                           sa * cos(tilt) * radius);
}

// ── Low-discrepancy aperture sampling ────────────────────────────────────────

/// Golden-angle spiral disk sample — near-optimal for any N.
///
///   index : sample index in [0, n)
///   n     : total sample count
///   Returns a 2D point uniformly distributed on the unit disk.
///
/// Theory: golden angle = 2π(1−1/φ) ≈ 137.5°
///   Rotating each point by the golden angle and distributing radii
///   by sqrt(i/n) gives the lowest-discrepancy sequence for arbitrary N.
///   This avoids the clumping of random sampling without needing a
///   precomputed table.
float2 golden_disk(int index, int n) {
    const float GOLDEN_ANGLE = 2.399963229;  // 2π × (1 − 1/φ)
    float r   = sqrt((float(index) + 0.5) / float(n));  // sqrt for uniform area dist
    float phi = float(index) * GOLDEN_ANGLE;
    return float2(cos(phi), sin(phi)) * r;
}

// ── Thin-lens aperture ray ────────────────────────────────────────────────────

/// Generate a jittered aperture ray for thin-lens Depth of Field.
///
///   ro_base      : pinhole camera position
///   rd_pinhole   : primary (pinhole) ray direction
///   basis        : camera frame (for aperture disk orientation)
///   focal_dist   : focus distance — scene at this depth will be sharp
///   aperture     : lens aperture radius (0 = pinhole, no DoF)
///   disk_offset  : 2D offset on unit disk from golden_disk()
///   ro_out       : OUTPUT jittered ray origin
///   Returns: jittered ray direction
///
/// How it works:
///   1. Find the focal point = where the pinhole ray hits the focal plane.
///   2. Jitter the ray origin on the aperture disk (perpendicular to view).
///   3. New direction = normalize(focal_point − jittered_origin).
///   All jittered rays still converge at the focal point → focal plane is sharp.
///   Objects at other depths receive contributions from all disk positions → blur.
float3 aperture_ray(float3 ro_base, float3 rd_pinhole,
                    CamBasis basis,  float  focal_dist,
                    float aperture,  float2 disk_offset,
                    thread float3&   ro_out) {
    float3 focal_pt = ro_base + rd_pinhole * focal_dist;
    ro_out = ro_base + (disk_offset.x * basis.right
                      + disk_offset.y * basis.up) * aperture;
    return normalize(focal_pt - ro_out);
}


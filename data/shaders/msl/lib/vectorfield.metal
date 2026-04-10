#pragma once
#include <metal_stdlib>
using namespace metal;

// ═══════════════════════════════════════════════════════════════════════════════
//  lib/vectorfield.metal  —  Modular 2-D vector-field utilities
//
//  Coordinate convention
//  ─────────────────────
//    uv   – normalised [0,1]×[0,1] viewport UV (origin = top-left in Metal)
//    asp  – float2(res.x/res.y, 1.0)   aspect corrector (keeps circles round)
//    Distances / radii are measured in *aspect-corrected* space where
//        p_asp = (uv - 0.5) * asp   →   x ∈ [-asp.x/2, asp.x/2], y ∈ [-0.5, 0.5]
//
//  Field functions
//  ───────────────
//    All vf_*  functions take p, att in UV space [0,1] and asp.
//    They return NORMALISED directions in aspect-corrected space.
//    Call vf_mag_*() separately if you need magnitude for colouring.
//
//  SDF helpers
//  ───────────
//    vf_sd_seg()   – capsule (segment + radius)
//    vf_sd_tri()   – filled triangle (negative inside)
//    vf_arrow()    – composite arrow (shaft capsule + head triangle)
//
//  Color utilities
//  ───────────────
//    vf_dir2hue()   – direction angle → HSV colour (saturation 0.85)
//    vf_mag_ramp()  – scalar → two-colour linear ramp
// ═══════════════════════════════════════════════════════════════════════════════

// ── SDF: capsule segment ──────────────────────────────────────────────────────
//  Returns signed distance to a rounded line segment [a,b] with radius r.
inline float vf_sd_seg(float2 p, float2 a, float2 b, float r) {
    float2 pa = p - a, ba = b - a;
    float  h  = clamp(dot(pa, ba) / dot(ba, ba), 0.f, 1.f);
    return length(pa - ba * h) - r;
}

// ── SDF: filled triangle ─────────────────────────────────────────────────────
//  Returns signed distance (negative = inside) for triangle (a,b,c).
inline float vf_sd_tri(float2 p, float2 a, float2 b, float2 c) {
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

// ── SDF: directed arrow ───────────────────────────────────────────────────────
//  p        – pixel in arrow-local space (origin = arrow centre)
//  dir      – normalised direction (points toward arrowhead)
//  half_len – half total arrow length
//  shaft_r  – half-width of shaft
//  head_r   – half-width of arrowhead base
inline float vf_arrow(float2 p, float2 dir, float half_len,
                       float shaft_r, float head_r)
{
    float2 perp      = float2(-dir.y, dir.x);
    float  head_back = min(head_r * 2.6f, half_len * 0.72f);
    float2 tail      = -dir * half_len;
    float2 shaft_tip =  dir * (half_len - head_back);
    float2 head_tip  =  dir * half_len;

    float d_shaft = vf_sd_seg(p, tail, shaft_tip, shaft_r);
    float d_head  = vf_sd_tri(p, head_tip,
                               shaft_tip + perp * head_r,
                               shaft_tip - perp * head_r);
    return min(d_shaft, d_head);
}

// ── Radial direction (unit vector pointing away from att) ─────────────────────
inline float2 vf_radial_dir(float2 p, float2 att, float2 asp) {
    float2 d = (p - att) * asp;
    float  r = length(d);
    return (r > 1e-6f) ? d / r : float2(0.f, 1.f);
}

// ── Radial magnitude (inverse-square, clamped) ────────────────────────────────
inline float vf_radial_mag(float2 p, float2 att, float2 asp) {
    float r = length((p - att) * asp);
    return clamp(1.f / (r + 0.04f), 0.f, 6.f);
}

// ── Curl (tangential / rotational) direction ──────────────────────────────────
//  sign = +1 → counter-clockwise,  -1 → clockwise
inline float2 vf_curl_dir(float2 p, float2 att, float2 asp, float sgn) {
    float2 d = (p - att) * asp;
    float  r = length(d);
    if (r < 1e-6f) return float2(1.f, 0.f);
    return sgn * float2(-d.y, d.x) / r;
}

// ── Dipole field (attract + repel at mirrored positions) ─────────────────────
inline float2 vf_dipole_dir(float2 p, float2 att, float2 asp, float sgn) {
    float2 att2 = float2(1.f - att.x, att.y);   // mirror across centre-x
    float2 r1 = vf_radial_dir(p, att,  asp) * sgn;
    float2 r2 = vf_radial_dir(p, att2, asp) * (-sgn);
    float2 v  = r1 + r2;
    float  l  = length(v);
    return (l > 1e-6f) ? v / l : float2(1.f, 0.f);
}

// ── Noise-twist perturbation ──────────────────────────────────────────────────
//  Returns a smooth pseudo-random 2-D direction (un-normalised).
inline float2 vf_noise_dir(float2 p, float t) {
    float a = sin(p.x * 4.3f + t) * cos(p.y * 3.7f + t * 0.7f) * 3.14159265f;
    return float2(cos(a + p.y * 2.1f), sin(a - p.x * 1.9f));
}

// ── Unified field sampler ─────────────────────────────────────────────────────
//  mode 0/1 – radial (attractor/repeller)
//  mode 2   – drain (radial + curl + noise)
//  mode 3   – pure curl / vortex
//  mode 4   – dipole (two opposing sources)
//
//  Returns a NORMALISED direction in aspect-corrected space.
inline float2 vf_sample(float2 p, float2 att, float2 asp,
                         int mode, float t, bool repel)
{
    float s = repel ? 1.f : -1.f;          // +1 = outward, -1 = inward
    float2 v;
    switch (mode) {
    case 0:
    case 1:
        v = vf_radial_dir(p, att, asp) * s;
        break;
    case 2: {
        float2 r = vf_radial_dir(p, att, asp) * s;
        float2 c = vf_curl_dir(p, att, asp, -1.f);
        float2 n = vf_noise_dir(p * 0.6f, t) * 0.18f;
        v = r + c * 0.65f + n;
        break;
    }
    case 3:
        v = vf_curl_dir(p, att, asp, s);
        break;
    case 4:
        v = vf_dipole_dir(p, att, asp, s);
        break;
    default:
        v = vf_radial_dir(p, att, asp) * s;
        break;
    }
    float l = length(v);
    return (l > 1e-6f) ? v / l : float2(0.f, 1.f);
}

// ── Direction angle → HSV colour ─────────────────────────────────────────────
//  Maps a 2-D direction to a hue-cycle colour (S=0.85, V=0.95).
inline float3 vf_dir2hue(float2 dir) {
    float  h = atan2(dir.y, dir.x) / (2.f * 3.14159265f) + 0.5f;
    float3 k = fract(float3(h, h + 0.333f, h + 0.667f));
    float3 p = abs(k * 6.f - 3.f);
    return clamp(p - 1.f, 0.f, 1.f) * 0.85f + 0.08f;
}

// ── Two-colour magnitude ramp ─────────────────────────────────────────────────
inline float3 vf_mag_ramp(float mag, float3 lo, float3 hi) {
    return mix(lo, hi, clamp(mag, 0.f, 1.f));
}


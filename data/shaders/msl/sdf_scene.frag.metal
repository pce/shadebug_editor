#include <metal_stdlib>
using namespace metal;

// ── Uniforms ──────────────────────────────────────────────────────────────────
struct EffectUniforms { float2 iResolution; float iTime; float _pad; };

// Each SDF node: 3 × float4 = 48 bytes
//   p0: x, y, rx (radius / half-width), type  (0=circle, 1=box, 2=round_box)
//   p1: ry (half-height), corner_r, smooth_k, _pad
//   p2: r, g, b,  op  (0=union, 1=smooth_union, 2=subtract, 3=intersect)
struct SdfNode { float4 p0; float4 p1; float4 p2; };

// Uniform block 1: node list + metadata (max 10 nodes = 16 + 10×48 = 496 bytes)
struct SdfBlock {
    int4    meta;         // x = node_count, y = debug_mode (0-3)
    SdfNode nodes[10];
};

struct Varyings { float4 pos [[position]]; float2 uv; };

// SDF primitives

float sd_circle(float2 p, float2 c, float r) {
    return length(p - c) - r;
}

float sd_box(float2 p, float2 c, float2 h) {
    float2 d = abs(p - c) - h;
    return length(max(d, 0.0f)) + min(max(d.x, d.y), 0.0f);
}

float sd_round_box(float2 p, float2 c, float2 h, float r) {
    float2 d = abs(p - c) - h + r;
    return length(max(d, 0.0f)) + min(max(d.x, d.y), 0.0f) - r;
}

// ─ Blend operations ──────────────────────────────────────────────────────────

float op_smooth_union(float d1, float d2, float k) {
    float h = clamp(0.5f + 0.5f * (d2 - d1) / max(k, 1e-6f), 0.0f, 1.0f);
    return mix(d2, d1, h) - k * h * (1.0f - h);
}

// ── Visualization helpers ─────────────────────────────────────────────────────

// Cool-warm distance heatmap (negative = blue/inside, positive = orange/outside)
float3 heatmap(float d) {
    float t = tanh(d * 4.0f);
    float3 inside  = mix(float3(0.05f, 0.20f, 0.80f), float3(0.10f, 0.50f, 1.00f), -min(t, 0.0f));
    float3 outside = mix(float3(0.04f, 0.04f, 0.06f), float3(1.00f, 0.55f, 0.10f),  max(t, 0.0f));
    return (t < 0.0f) ? inside : outside;
}

// Iso-contour lines at equal spacing
float isolines(float d, float spacing) {
    float w = max(fwidth(d), 1e-5f);
    return 1.0f - clamp(abs(fract(d / spacing - 0.5f) - 0.5f) / w, 0.0f, 1.0f);
}

// Background dot-grid
float dotgrid(float2 uv, float spacing) {
    float2 g  = fract(uv / spacing) - 0.5f;
    float  r  = length(g);
    float  w  = fwidth(r);
    return 1.0f - smoothstep(0.06f - w, 0.06f + w, r);
}

// ── Fragment main ─────────────────────────────────────────────────────────────

fragment float4 fs_main(
    Varyings                 in  [[stage_in]],
    constant EffectUniforms& u   [[buffer(0)]],
    constant SdfBlock&       nb  [[buffer(1)]])
{
    float2 uv    = in.uv;
    int    count = nb.meta.x;
    int    mode  = nb.meta.y;

    // Aspect-corrected space: circles look round, boxes look correct.
    // Positions and sizes are stored in "y-fraction" units [0,1].
    float  asp = u.iResolution.x / max(u.iResolution.y, 1.0f);
    float2 p   = float2(uv.x * asp, uv.y);   // current fragment in asp-space

    // ── Evaluate SDF scene ─────────────────────────────────────────────────
    float  d_total = 1e5f;
    float3 col_out = float3(0.04f, 0.04f, 0.06f);

    for (int i = 0; i < count && i < 10; ++i) {
        constant SdfNode& n = nb.nodes[i];

        // Unpack node
        float2 c_uv  = float2(n.p0.x, n.p0.y);
        float2 c_asp = float2(c_uv.x * asp, c_uv.y);

        float  rx     = n.p0.z;               // radius OR half-width in y-fraction units
        int    type   = int(n.p0.w + 0.5f);
        float  ry     = n.p1.x;               // half-height
        float  cr     = n.p1.y;               // corner radius
        float  sk     = n.p1.z;               // smooth_k
        float3 color  = n.p2.xyz;
        int    op     = int(n.p2.w + 0.5f);

        // SDF evaluation (all in asp-corrected space)
        float d;
        if      (type == 0) d = sd_circle   (p, c_asp, rx);
        else if (type == 1) d = sd_box      (p, c_asp, float2(rx, ry));
        else                d = sd_round_box(p, c_asp, float2(rx, ry), cr);

        // Blend operation
        if (op == 1) {
            // Smooth union: smoothly blend colors too
            float h = clamp(0.5f + 0.5f * (d_total - d) / max(sk, 1e-6f), 0.0f, 1.0f);
            col_out = mix(col_out, color, (d < 0.0f) ? h : 0.0f);
            d_total = op_smooth_union(d_total, d, sk);
        } else if (op == 2) {
            // Subtract: remove shape from the current field
            d_total = max(d_total, -d);
        } else if (op == 3) {
            // Intersect
            if (d > d_total) col_out = color;
            d_total = max(d_total, d);
        } else {
            // Union (default)
            if (d < d_total) {
                d_total = d;
                if (d < 0.0f) col_out = color;
            }
        }
    }

    // ── Render output based on debug mode ─────────────────────────────────────
    float  aa  = max(fwidth(d_total), 1e-5f);
    float3 bg  = float3(0.04f, 0.04f, 0.06f);
    float3 result;

    switch (mode) {

    case 1: {
        // Distance heatmap + thin iso-lines
        result = heatmap(d_total);
        result += isolines(d_total, 0.05f) * 0.20f;
        result += isolines(d_total, 0.01f) * 0.10f;
        break;
    }

    case 2: {
        // Fill + iso-lines overlay (great for understanding shape boundaries)
        float fill = 1.0f - smoothstep(-aa, aa, d_total);
        result = mix(bg, col_out * 0.7f, fill);
        result += isolines(d_total, 0.05f) * float3(0.20f, 0.75f, 1.00f);
        result += isolines(d_total, 0.01f) * float3(0.10f, 0.40f, 0.60f) * 0.5f;
        break;
    }

    case 3: {
        // Iso-lines only (dark background + cyan contours, like a topographic map)
        float3 deep_bg = float3(0.02f, 0.03f, 0.06f);
        result  = deep_bg;
        result += isolines(d_total, 0.05f) * float3(0.15f, 0.55f, 1.00f) * 0.85f;
        result += isolines(d_total, 0.01f) * float3(0.40f, 0.90f, 1.00f) * 0.35f;
        // Mark zero-crossing (the actual shape boundary)
        float edge = exp(-abs(d_total) / (aa * 3.0f));
        result += edge * float3(0.50f, 1.00f, 0.80f) * 0.40f;
        break;
    }

    default: {
        // Normal mode: anti-aliased fill with soft edge glow
        float fill = 1.0f - smoothstep(-aa, aa, d_total);
        result = mix(bg, col_out, fill);

        // Soft inner glow near the edge
        float glow = exp(-abs(d_total) * 40.0f);
        result += glow * (col_out * 0.35f + 0.05f);

        // Subtle grid background (only visible outside shapes)
        result += dotgrid(uv, 0.1f) * (1.0f - fill) * 0.035f;
        break;
    }
    }

    return float4(result, 1.0f);
}


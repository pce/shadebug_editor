#include <metal_stdlib>
using namespace metal;

struct EffectUniforms { float2 iResolution; float iTime; float _pad; };
struct Varyings { float4 pos [[position]]; float2 uv; };

// ═══════════════════════════════════════════════════════════════════════════════
//  UNIVERSE SHADER — fully modular, each function is a swappable block
//
//  Block overview:
//   1. hash / noise / fbm          — base math
//   2. star_color()                — blackbody temperature → RGB
//   3. stars_tiny()                — dense background micro-stars
//   4. stars_bright()              — foreground bright stars with diffraction
//   5. stars_giant()               — rare giant colored stars with glow corona
//   6. nebula_layer()              — per-channel fbm nebula cloud
//   7. asteroid_shape()            — SDF of a tumbling irregular rock
//   8. asteroid_field()            — procedural asteroid belt
//   9. comet()                     — single comet with tail
//  10. fs_main()                   — compose: stars + nebula + asteroids + comets
//
//  To swap: comment/uncomment the layer calls in fs_main().
// ═══════════════════════════════════════════════════════════════════════════════

// ── 1. Hash / Noise / FBM ─────────────────────────────────────────────────────

static float hash1(float2 p) {
    return fract(sin(dot(p, float2(127.1, 311.7))) * 43758.5453);
}
static float hash1f(float p) {
    return fract(sin(p * 78.233) * 43758.5453);
}
static float2 hash2(float2 p) {
    p = float2(dot(p, float2(127.1,311.7)), dot(p, float2(269.5,183.3)));
    return fract(sin(p) * 43758.5453);
}
static float gnoise(float2 p) {
    float2 i = floor(p), f = fract(p);
    float2 u = f*f*(3.0 - 2.0*f);
    float a = dot(hash2(i          )*2.0-1.0, f          );
    float b = dot(hash2(i+float2(1,0))*2.0-1.0, f-float2(1,0));
    float c = dot(hash2(i+float2(0,1))*2.0-1.0, f-float2(0,1));
    float d = dot(hash2(i+float2(1,1))*2.0-1.0, f-float2(1,1));
    return mix(mix(a,b,u.x), mix(c,d,u.x), u.y);
}
static float fbm(float2 p, int oct) {
    float v = 0.0, amp = 0.5;
    for (int i = 0; i < oct; i++) { v += gnoise(p)*amp; p *= 2.0; amp *= 0.5; }
    return v * 0.5 + 0.5;
}

// ── 2. Star blackbody color ───────────────────────────────────────────────────
//  t in [0,1]:  0 = red dwarf (2000K), 0.5 = sun (5800K), 1 = blue giant (12000K)
static float3 star_color(float t) {
    float temp = mix(2000.0, 12000.0, t);
    float r = clamp(1.0 - max(0.0, temp - 6600.0) / 4000.0, 0.4, 1.0);
    float g = (temp < 6600.0)
            ? clamp(temp / 6600.0, 0.4, 1.0)
            : clamp(1.0 - (temp - 6600.0) / 8000.0, 0.5, 1.0);
    float b = (temp < 7000.0) ? clamp((temp - 2000.0) / 5000.0, 0.0, 1.0) : 1.0;
    return float3(r, g, b);
}

// ── 3. Dense background micro-stars ──────────────────────────────────────────
//  Very fine grid, simple point stars — the "Milky Way dust" layer
static float3 stars_tiny(float2 uv, float time) {
    float3 col = float3(0.0);
    float2 g   = floor(uv * 320.0);
    float  h   = hash1(g);
    if (h > 0.984) {
        float twinkle = 0.5 + 0.5 * sin(time * (1.0 + h * 4.0) + h * 200.0);
        float brightness = pow((h - 0.984) / 0.016, 1.5);
        col = float3(0.5, 0.6, 0.8) * twinkle * brightness * 0.6;
    }
    return col;
}

// ── 4. Bright foreground stars with 4-spike diffraction cross ─────────────────
static float3 stars_bright(float2 uv, float time) {
    float3 col = float3(0.0);
    float2 g   = floor(uv * 90.0);
    float2 f   = fract(uv * 90.0) - 0.5;
    float  h   = hash1(g + float2(13.7, 57.3));
    if (h > 0.990) {
        float2 seed = hash2(g + float2(h));
        float2 center = seed - 0.5;  // sub-cell jitter
        float2 d = f - center;
        float  r = length(d);

        float twinkle    = 0.7 + 0.3 * sin(time * (2.0 + h*6.0));
        float brightness = pow((h - 0.990) / 0.010, 2.0) * 4.0;
        float3 sc        = star_color(fract(h * 7.13));

        // Soft glow
        col += sc * exp(-r * 60.0) * brightness * twinkle;

        // 4-spike diffraction cross  (spike along x and y axes)
        float spike_x = exp(-abs(d.y) * 120.0) * exp(-abs(d.x) * 8.0);
        float spike_y = exp(-abs(d.x) * 120.0) * exp(-abs(d.y) * 8.0);
        col += sc * (spike_x + spike_y) * brightness * twinkle * 0.4;
    }
    return col;
}

// ── 5. Rare giant stars — large corona + color ────────────────────────────────
static float3 stars_giant(float2 uv, float time) {
    float3 col = float3(0.0);
    float2 g   = floor(uv * 20.0);
    float2 f   = fract(uv * 20.0) - 0.5;
    float  h   = hash1(g + float2(99.1, 33.7));
    if (h > 0.94) {
        float2 seed   = hash2(g + float2(h));
        float2 d      = f - (seed - 0.5);
        float  r      = length(d);
        float  twinkle = 0.8 + 0.2 * sin(time * (0.5 + h*2.0));
        float3 sc      = star_color(fract(h * 3.7));
        float  bright  = pow((h - 0.94) / 0.06, 3.0) * 6.0;

        // Large soft corona
        col += sc * exp(-r * 25.0) * bright * twinkle;
        // Radial glow rings
        col += sc * exp(-abs(r - 0.04) * 80.0) * bright * 0.3 * twinkle;
        col += sc * exp(-abs(r - 0.08) * 60.0) * bright * 0.15 * twinkle;
    }
    return col;
}

// ── 6. Nebula — per-channel fbm cloud volumes ──────────────────────────────────
//  Each channel uses different seed so RGB channels don't match → natural color variance
static float3 nebula_layer(float2 uv, float time) {
    float2 p = uv * 2.8 + time * 0.008;
    float  cluster = smoothstep(0.35, 0.70, fbm(uv * 1.8, 4));

    float nr = fbm(p              , 6) * 0.5 + 0.5;
    float ng = fbm(p+float2(1.7, 9.2), 6) * 0.5 + 0.5;
    float nb = fbm(p+float2(8.3, 2.8), 6) * 0.5 + 0.5;

    // Sharpen to make denser cloud cores visible
    nr = smoothstep(0.48, 0.70, nr);
    ng = smoothstep(0.50, 0.72, ng);
    nb = smoothstep(0.46, 0.68, nb);

    return float3(nr, ng * 0.7, nb) * cluster * 0.50;
}

// ── 7. Asteroid SDF — irregular convex rock ──────────────────────────────────
//  Returns signed distance to a procedural polygon with fbm-bumped edges.
//  center: asteroid position in UV space
//  seed:   unique per-asteroid hash seed
//  scale:  size
//  angle:  current tumble rotation angle
static float asteroid_sdf(float2 p, float2 center, float scale, float angle) {
    float2 d = p - center;
    // Rotate into asteroid-local frame
    float c = cos(angle), s = sin(angle);
    float2 r = float2(c*d.x + s*d.y, -s*d.x + c*d.y);
    float  a = atan2(r.y, r.x);  // polar angle

    // Irregular radius: base + 5 harmonics of varying amplitude
    float rad = 1.0
        + 0.20 * cos(a * 3.0 + 1.2)
        + 0.15 * cos(a * 5.0 + 0.7)
        + 0.10 * cos(a * 7.0 + 2.1)
        + 0.08 * cos(a * 11.0 + 0.4)
        + 0.05 * cos(a * 13.0 + 1.8);

    return (length(r) / scale) - rad;
}

// ── 8. Asteroid surface shading ───────────────────────────────────────────────
//  Returns a shaded rock color given SDF dist and position
static float3 asteroid_color(float2 p, float2 center, float seed, float time) {
    float3 light = normalize(float3(-0.6, 0.5, 0.7));
    // Fake 3D normal via fbm bump
    float bumpScale = 80.0;
    float bx = gnoise(p * bumpScale            );
    float by = gnoise(p * bumpScale + float2(0.1, 0.0));
    float3 normal   = normalize(float3(bx - by, 0.3, 0.7));

    float diff = max(0.0, dot(normal, light));
    float spec = pow(max(0.0, dot(reflect(-light, normal), float3(0,0,1))), 12.0);

    // Base rock tint — varies per asteroid
    float3 rock_a = mix(float3(0.28, 0.24, 0.20), float3(0.22, 0.20, 0.18), fract(seed));
    float3 rock_b = mix(float3(0.40, 0.36, 0.30), float3(0.35, 0.28, 0.22), fract(seed*2.3));
    // fbm crater/surface variation
    float surface = fbm(p * 12.0 + seed, 4);
    float3 col = mix(rock_a, rock_b, surface);

    col *= (0.10 + 0.90 * diff);
    col += float3(0.5, 0.45, 0.4) * spec * 0.3;

    return col;
}

// ── 9. Asteroid field ─────────────────────────────────────────────────────────
//  Procedural belt of ~N asteroids per grid cell, drifting slowly
//  Returns RGBA where .a is the asteroid mask (for compositing)
static float4 asteroid_field(float2 uv, float time) {
    const int GRID = 8;   // cells per axis — increase for denser field
    float3 col  = float3(0.0);
    float  mask = 0.0;

    for (int ix = -1; ix <= 1; ix++) {
    for (int iy = -1; iy <= 1; iy++) {
        float2 cell_id = floor(uv * float(GRID)) + float2(ix, iy);
        float  seed    = hash1(cell_id);

        if (seed < 0.55) continue;   // ~45% of cells have an asteroid

        // Asteroid centre: jittered within cell, slowly drifting
        float2 jitter = hash2(cell_id + float2(3.7, 9.1));
        float  drift_speed = (seed - 0.55) * 0.4;
        float2 center = (cell_id + jitter) / float(GRID)
                      + float2(drift_speed, drift_speed * 0.4) * time * 0.002;
        center = fract(center);  // wrap around

        // Size: small to medium rocks
        float scale = mix(0.008, 0.030, fract(seed * 5.3)) / float(GRID);

        // Tumble: each asteroid rotates at its own speed
        float tumble = time * mix(0.3, 1.8, fract(seed * 3.1))
                     + seed * 10.0;

        float sdf = asteroid_sdf(uv, center, scale, tumble);

        if (sdf < 0.002) {
            float edge   = 1.0 - smoothstep(-0.002, 0.0, sdf);  // soft antialiased edge
            float3 ac    = asteroid_color(uv, center, seed, time);
            col  = mix(col,  ac,   edge);
            mask = mix(mask, edge, edge);
        }
    }}

    return float4(col, mask);
}

// ── 9b. Comet ─────────────────────────────────────────────────────────────────
//  A single comet streaking across — head glow + ion tail + dust tail
static float3 comet(float2 uv, float time) {
    float3 col = float3(0.0);

    // Orbit parameters — adjust seed/speed for different comets
    float speed  = 0.08;
    float2 dir   = normalize(float2(0.7, -0.3));
    float2 perp  = float2(-dir.y, dir.x);
    float  t     = fract(time * speed + 0.3);      // [0,1] loop
    float2 pos   = float2(-0.3, 0.8) + dir * t * 2.5;  // enter left, exit right

    float2 d    = uv - pos;
    float  para = dot(d, dir);    // distance along tail
    float  perp_d = dot(d, perp); // distance perpendicular to tail

    // ── Head ─────────────────────────────────────────────────────────────────
    float  head_r = length(d);
    col += float3(0.9, 0.95, 1.0) * exp(-head_r * 300.0) * 3.0;   // bright nucleus
    col += float3(0.5, 0.7, 1.0)  * exp(-head_r *  60.0) * 0.8;   // coma glow

    // ── Ion tail (narrow, blue-white) ────────────────────────────────────────
    //  points exactly opposite to motion direction, wiggles with fbm
    if (para < 0.0) {
        float tail_len  = -para;                             // distance along tail
        float wiggle    = gnoise(float2(tail_len * 8.0, time * 2.0)) * 0.004;
        float ion_width = 0.0015 + tail_len * 0.002;        // fans out slightly
        float ion       = exp(-pow((perp_d + wiggle) / ion_width, 2.0));
        float ion_fade  = exp(-tail_len * 3.5);
        col += float3(0.6, 0.8, 1.0) * ion * ion_fade * 1.2;
    }

    // ── Dust tail (wide, yellowish, curved slightly) ──────────────────────────
    if (para < 0.0) {
        float tail_len   = -para;
        float curve      = tail_len * tail_len * 0.3;       // slight curve due to radiation
        float dust_width = 0.004 + tail_len * 0.012;
        float dust       = exp(-pow((perp_d - curve) / dust_width, 2.0));
        float dust_fade  = exp(-tail_len * 2.2);
        col += float3(1.0, 0.85, 0.6) * dust * dust_fade * 0.5;
    }

    return col;
}

// ── 10. Fragment main — compose layers ────────────────────────────────────────
//
//  Swap order or comment out any layer here:

fragment float4 fs_main(Varyings in [[stage_in]],
                        constant EffectUniforms& u [[buffer(0)]]) {
    float2 uv   = in.uv;
    float  time = u.iTime;

    // Slow panoramic rotation
    float  rot = time * 0.012;
    float2 ruv = float2(cos(rot)*uv.x - sin(rot)*uv.y,
                        sin(rot)*uv.x + cos(rot)*uv.y);

    // ── Layer: deep space background ─────────────────────────────────────────
    float3 col = float3(0.0, 0.0, 0.018);

    // ── Layer: nebula clouds (behind stars) ───────────────────────────────────
    col += nebula_layer(ruv, time);

    // ── Layer: micro stars ────────────────────────────────────────────────────
    col += stars_tiny(ruv, time);

    // ── Layer: bright stars with diffraction ──────────────────────────────────
    col += stars_bright(ruv, time);

    // ── Layer: giant coloured stars with corona ───────────────────────────────
    col += stars_giant(ruv, time);

    // ── Layer: asteroid field ─────────────────────────────────────────────────
    float4 ast = asteroid_field(uv, time);
    col = mix(col, ast.rgb, ast.a);

    // ── Layer: comet ──────────────────────────────────────────────────────────
    col += comet(uv, time);

    // Tone map + gamma
    col = col / (col + float3(0.8));    // Reinhard-like compress
    col = pow(max(col, 0.0), float3(0.4545));

    return float4(col, 1.0);
}

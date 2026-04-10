#pragma once
// shader_params.hpp
//
// JSON-driven shader parameter system.
//
//  JSON schema (inside gpu_pipeline.json, per pipeline entry):
//  ─────────────────────────────────────────────────────────────
//  "params": [
//    {
//      "name":      "speed",          // internal / shader identifier
//      "label":     "Speed",          // display label in ImGui panel
//      "type":      "float",          // float | float2 | float3 | color4 | bool | int
//      "slot":      0,                // index into ParamUniforms flat buffer (0..15)
//      "default":   1.0,              // scalar or array [r,g,b,a]
//      "range":     [0.0, 5.0],       // [min, max] – used for slider
//      "motion": {
//        "mode":      "oscillate",    // static | oscillate | keyframes
//        "amplitude": 0.5,
//        "frequency": 0.3,
//        "phase":     0.0,
//        "loop":      true,
//        "keyframes": [               // for mode = "keyframes"
//          { "t": 0.0, "v": 1.0 },
//          { "t": 2.0, "v": 5.0 },
//          { "t": 4.0, "v": 1.0 }
//        ]
//      }
//    }
//  ]
//
//  Uniform block layout (buffer slot 1, 64 bytes):
//  ─────────────────────────────────────────────────────────────
//  Metal MSL:
//    struct ParamUniforms { float4 p0, p1, p2, p3; };
//    // slots: p0 = [0..3], p1 = [4..7], p2 = [8..11], p3 = [12..15]
//    fragment float4 fs_main(..., constant ParamUniforms& p [[buffer(1)]]) { ... }
//
//  GLSL:
//    uniform vec4 iParams0;  // slots 0-3
//    uniform vec4 iParams1;  // slots 4-7
//    uniform vec4 iParams2;  // slots 8-11
//    uniform vec4 iParams3;  // slots 12-15

#include <string>
#include <vector>
#include <cstdint>

#include <nlohmann/json_fwd.hpp>

namespace shadebug::renderer {

// ── ParamType ────────────────────────────────────────────────────────────────

enum class ParamType : uint8_t {
    Float  = 0,   ///< 1 float  → slider
    Float2 = 1,   ///< 2 floats → 2 sliders (x,y)
    Float3 = 2,   ///< 3 floats → 3 sliders (x,y,z)
    Color4 = 3,   ///< 4 floats → color-picker (r,g,b,a)
    Bool   = 4,   ///< 1 float (0/1) → checkbox
    Int    = 5,   ///< 1 float (int-valued) → int slider
};

[[nodiscard]] inline const char* param_type_name(ParamType t) noexcept {
    switch (t) {
    case ParamType::Float:  return "float";
    case ParamType::Float2: return "float2";
    case ParamType::Float3: return "float3";
    case ParamType::Color4: return "color4";
    case ParamType::Bool:   return "bool";
    case ParamType::Int:    return "int";
    default:                return "?";
    }
}

// ── MotionMode ───────────────────────────────────────────────────────────────

enum class MotionMode : uint8_t {
    Static    = 0,   ///< Fixed – only interactive via ImGui drag
    Oscillate = 1,   ///< base + amplitude × sin(time × freq × 2π + phase)
    Keyframes = 2,   ///< Piecewise-linear keyframes, optional loop
};

// ── Keyframe ─────────────────────────────────────────────────────────────────

struct Keyframe {
    float t = 0.f;   ///< time in seconds
    float v = 0.f;   ///< scalar value at that time
};

// ── ShaderMotion ─────────────────────────────────────────────────────────────

struct ShaderMotion {
    MotionMode mode      = MotionMode::Static;
    float      amplitude = 0.f;
    float      frequency = 1.f;
    float      phase     = 0.f;
    bool       loop      = true;
    std::vector<Keyframe> keyframes;

    /// Evaluate at [time], using [base_val] as center / reference value.
    [[nodiscard]] float evaluate(float base_val, float time) const noexcept;
};

// ── ShaderParam ──────────────────────────────────────────────────────────────

struct ShaderParam {
    std::string name;          ///< internal name (matches JSON + shader)
    std::string label;         ///< ImGui display label
    ParamType   type    = ParamType::Float;
    int         slot    = 0;   ///< index into ParamUniforms flat buffer [0..15]
    float       val[4]  = {};  ///< current runtime value (r,g,b,a / x,y,z,w)
    float       def[4]  = {};  ///< default from JSON
    float       range_min = 0.f;
    float       range_max = 1.f;
    bool        motion_enabled = false;
    ShaderMotion motion;

    [[nodiscard]] int  component_count() const noexcept;
    void reset_to_default() noexcept;

    /// Apply motion to val[0] (for Float/Bool/Int types).
    /// Multi-component types (Float2, Float3, Color4) animate x-component only.
    void update_from_motion(float time) noexcept;
};

// ── ParamUniforms ─────────────────────────────────────────────────────────────
//
//  Flat uniform buffer uploaded to shader uniform block 1.
//  16 float slots packed as 4 × float4.

struct alignas(16) ParamUniforms {
    float p0[4] = {};   ///< slots  0-3
    float p1[4] = {};   ///< slots  4-7
    float p2[4] = {};   ///< slots  8-11
    float p3[4] = {};   ///< slots 12-15
};
static_assert(sizeof(ParamUniforms) == 64, "ParamUniforms must be 64 bytes");

// ── Utilities ─────────────────────────────────────────────────────────────────

/// Evaluate all params at [time] and pack into a ParamUniforms buffer.
[[nodiscard]] ParamUniforms pack_params(std::vector<ShaderParam>& params,
                                        float time) noexcept;

/// Parse a JSON `"params"` array into a ShaderParam vector.
/// Call this once when loading gpu_pipeline.json.
[[nodiscard]] std::vector<ShaderParam> parse_shader_params(const nlohmann::json& arr);

} // namespace shadebug::renderer


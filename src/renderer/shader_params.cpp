// shader_params.cpp
#include "shader_params.hpp"
#include <nlohmann/json.hpp>
#include <algorithm>
#include <cmath>

namespace shadebug::renderer {

// ── ShaderMotion ─────────────────────────────────────────────────────────────

float ShaderMotion::evaluate(float base_val, float time) const noexcept {
    switch (mode) {
    case MotionMode::Oscillate:
        return base_val + amplitude * std::sin(time * frequency * 6.28318530f + phase);
    case MotionMode::Keyframes: {
        if (keyframes.empty()) return base_val;
        if (keyframes.size() == 1) return keyframes[0].v;
        float t = time;
        if (loop) {
            const float t0  = keyframes.front().t;
            const float dur = keyframes.back().t - t0;
            if (dur > 0.f) {
                t = t0 + std::fmod(time - t0, dur);
                if (t < t0) t += dur;
            }
        }
        for (std::size_t i = 0; i + 1 < keyframes.size(); ++i) {
            if (t <= keyframes[i + 1].t) {
                const float dt    = keyframes[i + 1].t - keyframes[i].t;
                const float alpha = (dt > 1e-7f) ? (t - keyframes[i].t) / dt : 0.f;
                return keyframes[i].v + alpha * (keyframes[i + 1].v - keyframes[i].v);
            }
        }
        return keyframes.back().v;
    }
    default: return base_val;
    }
}

// ── ShaderParam ──────────────────────────────────────────────────────────────

int ShaderParam::component_count() const noexcept {
    switch (type) {
    case ParamType::Float2: return 2;
    case ParamType::Float3: return 3;
    case ParamType::Color4: return 4;
    default:                return 1;
    }
}

void ShaderParam::reset_to_default() noexcept {
    for (int i = 0; i < 4; ++i) val[i] = def[i];
}

void ShaderParam::update_from_motion(float time) noexcept {
    if (!motion_enabled || motion.mode == MotionMode::Static) return;
    val[0] = motion.evaluate(def[0], time);
}

// ── Pack ─────────────────────────────────────────────────────────────────────

ParamUniforms pack_params(std::vector<ShaderParam>& params, float time) noexcept {
    ParamUniforms out{};
    float* slots = reinterpret_cast<float*>(&out);
    for (auto& p : params) {
        p.update_from_motion(time);
        const int slot = std::clamp(p.slot, 0, 15);
        const int n    = std::min(p.component_count(), 16 - slot);
        for (int i = 0; i < n; ++i) slots[slot + i] = p.val[i];
    }
    return out;
}

// ── JSON parsing ──────────────────────────────────────────────────────────────

std::vector<ShaderParam> parse_shader_params(const nlohmann::json& arr) {
    std::vector<ShaderParam> result;
    result.reserve(arr.size());

    for (const auto& j : arr) {
        ShaderParam p;
        p.name  = j.value("name",  "param");
        p.label = j.value("label", p.name);
        p.slot  = std::clamp(j.value("slot", 0), 0, 15);

        const std::string tstr = j.value("type", "float");
        if      (tstr == "float2")                        p.type = ParamType::Float2;
        else if (tstr == "float3")                        p.type = ParamType::Float3;
        else if (tstr == "color4" || tstr == "color")     p.type = ParamType::Color4;
        else if (tstr == "bool")                          p.type = ParamType::Bool;
        else if (tstr == "int")                           p.type = ParamType::Int;
        else                                              p.type = ParamType::Float;

        if (j.contains("default")) {
            const auto& dv = j["default"];
            if (dv.is_array()) {
                int i = 0;
                for (const auto& v : dv) { if (i < 4) p.def[i++] = v.get<float>(); }
            } else {
                p.def[0] = dv.get<float>();
            }
        }

        p.range_min = j.value("range_min", 0.f);
        p.range_max = j.value("range_max", 1.f);
        if (j.contains("range") && j["range"].is_array() && j["range"].size() >= 2) {
            p.range_min = j["range"][0].get<float>();
            p.range_max = j["range"][1].get<float>();
        }
        for (int i = 0; i < 4; ++i) p.val[i] = p.def[i];

        if (j.contains("motion")) {
            const auto& m = j["motion"];
            p.motion_enabled = true;
            const std::string mmode = m.value("mode", "static");
            if      (mmode == "oscillate") p.motion.mode = MotionMode::Oscillate;
            else if (mmode == "keyframes") p.motion.mode = MotionMode::Keyframes;
            else                           p.motion.mode = MotionMode::Static;
            p.motion.amplitude = m.value("amplitude", 0.f);
            p.motion.frequency = m.value("frequency", 1.f);
            p.motion.phase     = m.value("phase",     0.f);
            p.motion.loop      = m.value("loop",      true);
            if (m.contains("keyframes"))
                for (const auto& kf : m["keyframes"])
                    p.motion.keyframes.push_back({ kf["t"].get<float>(), kf["v"].get<float>() });
        }
        result.push_back(std::move(p));
    }
    return result;
}

} // namespace shadebug::renderer

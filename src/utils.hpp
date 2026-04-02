#pragma once

#include "imgui.h"
#include <string_view>
#include <span>

namespace shadebug::utils {

// ── ImGui layout helpers ──────────────────────────────────────────────────────

/// Centered text inside the current column / window width.
inline void centered_text(std::string_view text, float column_width = 0.f) noexcept {
    if (column_width <= 0.f)
        column_width = ImGui::GetContentRegionAvail().x;
    const float tw = ImGui::CalcTextSize(text.data(), text.data() + text.size()).x;
    ImGui::SetCursorPosX(ImGui::GetCursorPosX() + (column_width - tw) * 0.5f);
    ImGui::TextUnformatted(text.data(), text.data() + text.size());
}

/// Text clipped to max_width with "…" suffix.
void truncated_text(std::string_view text, float max_width);

/// Horizontally centered button. Returns true when clicked.
bool centered_button(std::string_view label, float width = 120.f);

/// Full-width button spanning the current content region.
inline bool full_button(std::string_view label) noexcept {
    return ImGui::Button(label.data(), ImVec2(-FLT_MIN, 0));
}

/// Tooltip on the previous item (only if hovered and non-empty).
void maybe_tooltip(std::string_view tip);

// ── Geometry helpers ──────────────────────────────────────────────────────────

struct FitResult {
    ImVec2 offset;   // top-left position within available area
    ImVec2 size;     // fitted size
    float  scale;    // uniform scale factor applied
};

/// Fit a rectangle of (content_w × content_h) into (avail_w × avail_h),
/// maintaining aspect ratio. Returns position and size centred in the area.
[[nodiscard]] FitResult fit_rect(
    float content_w, float content_h,
    float avail_w,   float avail_h,
    float margin = 0.f);

// ── Color conversion ──────────────────────────────────────────────────────────

[[nodiscard]] inline ImVec4 from_hex(std::uint32_t rgba) noexcept {
    return {
        static_cast<float>((rgba >> 24) & 0xFF) / 255.f,
        static_cast<float>((rgba >> 16) & 0xFF) / 255.f,
        static_cast<float>((rgba >>  8) & 0xFF) / 255.f,
        static_cast<float>( rgba        & 0xFF) / 255.f,
    };
}

// ── DPI / scale helpers ───────────────────────────────────────────────────────

/// Scale a point-size value by DPI.
[[nodiscard]] inline float dpi(float pts, float dpi_scale) noexcept {
    return pts * dpi_scale;
}

} // namespace shadebug::utils

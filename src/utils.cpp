#include "utils.hpp"
#include <string>

namespace shadebug::utils {

void truncated_text(std::string_view text, float max_width) {
    const float tw = ImGui::CalcTextSize(text.data(), text.data() + text.size()).x;
    if (tw <= max_width) {
        ImGui::TextUnformatted(text.data(), text.data() + text.size());
        return;
    }
    constexpr std::string_view kEllipsis = "…";  // UTF-8: 3 bytes + null
    const float ew     = ImGui::CalcTextSize(kEllipsis.data()).x;
    const float target = max_width - ew;

    // Binary-search for the longest prefix that fits with "…" appended.
    std::size_t lo = 0, hi = text.size();
    while (lo + 1 < hi) {
        const auto mid = (lo + hi) / 2;
        if (ImGui::CalcTextSize(text.data(), text.data() + mid).x <= target)
            lo = mid;
        else
            hi = mid;
    }

    // Build truncated string on the stack (prefix + UTF-8 ellipsis)
    std::string result;
    result.reserve(lo + kEllipsis.size());
    result.append(text.data(), lo);
    result.append(kEllipsis);
    ImGui::TextUnformatted(result.c_str());
}

bool centered_button(std::string_view label, float width) {
    const float avail = ImGui::GetContentRegionAvail().x;
    ImGui::SetCursorPosX(ImGui::GetCursorPosX() + (avail - width) * 0.5f);
    return ImGui::Button(label.data(), ImVec2(width, 0));
}

void maybe_tooltip(std::string_view tip) {
    if (tip.empty()) return;
    if (ImGui::IsItemHovered(ImGuiHoveredFlags_DelayShort))
        ImGui::SetTooltip("%.*s", static_cast<int>(tip.size()), tip.data());
}

FitResult fit_rect(float cw, float ch, float aw, float ah, float margin) {
    const float usable_w = aw - margin * 2.f;
    const float usable_h = ah - margin * 2.f;

    float scale;
    if (ch / cw * usable_w <= usable_h)
        scale = cw > 0.f ? usable_w / cw : 1.f;
    else
        scale = ch > 0.f ? usable_h / ch : 1.f;

    const float fw = cw * scale;
    const float fh = ch * scale;
    return FitResult{
        .offset = { margin + (usable_w - fw) * 0.5f,
                    margin + (usable_h - fh) * 0.5f },
        .size   = { fw, fh },
        .scale  = scale,
    };
}

} // namespace shadebug::utils

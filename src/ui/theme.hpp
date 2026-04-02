#pragma once

#include "imgui.h"
#include <string>
#include <string_view>

namespace shadebug::ui {

// ── ThemeType ─────────────────────────────────────────────────────────────────

enum class ThemeType {
    SolarizedDark,
    SolarizedLight,
    Monokai,
    ArcDark,
    OneDark,
    PixelDark,      // retro pixel look — ProggyClean + FontAwesome icons
    Count,          // sentinel
};

[[nodiscard]] inline constexpr std::string_view theme_name(ThemeType t) noexcept {
    switch (t) {
    case ThemeType::SolarizedDark:  return "Solarized Dark";
    case ThemeType::SolarizedLight: return "Solarized Light";
    case ThemeType::Monokai:        return "Monokai";
    case ThemeType::ArcDark:        return "Arc Dark";
    case ThemeType::OneDark:        return "One Dark";
    case ThemeType::PixelDark:      return "Pixel Dark";
    default:                        return "Unknown";
    }
}

[[nodiscard]] inline constexpr std::string_view theme_id(ThemeType t) noexcept {
    switch (t) {
    case ThemeType::SolarizedDark:  return "SolarizedDark";
    case ThemeType::SolarizedLight: return "SolarizedLight";
    case ThemeType::Monokai:        return "Monokai";
    case ThemeType::ArcDark:        return "ArcDark";
    case ThemeType::OneDark:        return "OneDark";
    case ThemeType::PixelDark:      return "PixelDark";
    default:                        return "SolarizedDark";
    }
}

[[nodiscard]] inline ThemeType theme_from_id(std::string_view id) noexcept {
    if (id == "SolarizedLight") return ThemeType::SolarizedLight;
    if (id == "Monokai")        return ThemeType::Monokai;
    if (id == "ArcDark")        return ThemeType::ArcDark;
    if (id == "OneDark")        return ThemeType::OneDark;
    if (id == "PixelDark")      return ThemeType::PixelDark;
    return ThemeType::SolarizedDark;  // default
}

// ── StyleParams ───────────────────────────────────────────────────────────────
//
//  Adjustable style values — tweaked at runtime and persisted to settings.json.
//

struct StyleParams {
    float rounding      = 4.f;   // WindowRounding / FrameRounding / …
    float item_spacing  = 8.f;   // ItemSpacing.x
    float frame_padding = 6.f;   // FramePadding.x
    float font_scale    = 1.f;   // ImGui::SetWindowFontScale equivalent (global)
};

// ── FontConfig ────────────────────────────────────────────────────────────────
//
//  A single font slot.  path is relative to the executable directory.
//  Empty path means "use the theme's built-in default (or ImGui's default)".
//  Changes take effect on next restart.
//

struct FontConfig {
    std::string path;           // URI: "data://fonts/ProggyClean.ttf" or plain path
    float       size_px = 13.f;
};

// Per-theme default fonts (returned when FontConfig::path is empty)
[[nodiscard]] inline FontConfig default_main_font(ThemeType t) noexcept {
    if (t == ThemeType::PixelDark)
        return { "data://fonts/ProggyClean.ttf", 13.f };
    return {};   // empty → ImGui built-in default
}

[[nodiscard]] inline FontConfig default_icon_font(ThemeType t) noexcept {
    // Icons are useful in every theme; size matches the main font.
    const float sz = (t == ThemeType::PixelDark) ? 13.f : 14.f;
    return { "data://fonts/fontawesome-webfont.ttf", sz };
}

// ── ThemeSettings ─────────────────────────────────────────────────────────────

struct ThemeSettings {
    ThemeType   active     = ThemeType::ArcDark;
    StyleParams style      = {};
    // Empty path = use the theme's default.  Changing these requires restart.
    FontConfig  main_font  = {};
    FontConfig  icon_font  = {};
};

// ── Theme ─────────────────────────────────────────────────────────────────────

class Theme {
public:
    void ApplyImGuiStyle(float dpi_scale = 1.f,
                         const StyleParams& params = {}) noexcept;
    void ApplyColorTheme(ThemeType t) noexcept;

    /// Returns the tight StyleParams for PixelDark (zero rounding, small padding).
    [[nodiscard]] static StyleParams pixel_dark_style() noexcept {
        return { .rounding = 0.f, .item_spacing = 4.f, .frame_padding = 2.f, .font_scale = 1.f };
    }
};

} // namespace shadebug::ui

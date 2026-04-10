#pragma once

#include "theme.hpp"
#include "../io/vfs.hpp"
#include <filesystem>
#include <string>

namespace shadebug::ui {

// ── Settings ──────────────────────────────────────────────────────────────────
//
//  Persisted app preferences — saved to/loaded from `settings.json` in the
//  executable directory.  Serialised with nlohmann/json.
//
//  Layout (panel visibility) and theme are the two main sections.
//  Future sections (recently opened files, etc.) can be added here.
//

// ── PanelSettings ─────────────────────────────────────────────────────────────

struct PanelSettings {
    bool show_layers        = true;
    bool show_properties    = true;
    bool show_canvas        = true;
    bool show_text_editor   = false;
    bool show_shader_list   = false;
    bool show_shader_render = false;
    bool show_sdf_scene     = false;
};

// ── Settings ──────────────────────────────────────────────────────────────────

struct Settings {
    int            version = 1;
    ThemeSettings  theme;
    PanelSettings  panels;

    // ── IO ────────────────────────────────────────────────────────────────────

    /// Load from disk. Returns true on success; on failure keeps default values.
    bool load(const std::filesystem::path& path) noexcept;

    /// Save to disk. Returns true on success.
    bool save(const std::filesystem::path& path) const noexcept;

    /// Convenience: settings file path next to the running executable.
    [[nodiscard]] static std::filesystem::path default_path() noexcept;

    /// Directory that contains the running executable.
    [[nodiscard]] static std::filesystem::path exe_dir() noexcept;

    // ── Apply to live ImGui state ─────────────────────────────────────────────

    void apply_theme(float dpi_scale = 1.f) const noexcept;

    /// Build the ImGui font atlas from theme.main_font / theme.icon_font.
    /// Paths are resolved via the provided VirtualFileSystem instance.
    /// Must be called AFTER simgui_setup() with no_default_font=true.
    void load_fonts(const vfs::VirtualFileSystem& vfs, float dpi_scale = 1.f) const noexcept;
};

} // namespace shadebug::ui

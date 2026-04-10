#pragma once

#include "document/document.hpp"
#include "ui/responsive_layout.hpp"
#include "ui/image_cache.hpp"
#include "ui/settings.hpp"
#include "renderer/draw_ctx.hpp"
#include "renderer/gpu_renderer.hpp"
#include "renderer/effect_renderer.hpp"
#include "panels/text_editor_panel.hpp"
#include "panels/shader_list_panel.hpp"
#include "panels/shader_render_panel.hpp"
#include "panels/svg_preview.hpp"
#include "panels/sdf_panel.hpp"
#include "drag_drop_dialog.hpp"
#include "io/vfs.hpp"
#include "sokol_app.h"
#include "ui/fab_nav.hpp"

#include <filesystem>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

namespace shadebug {

// ── Selection ─────────────────────────────────────────────────────────────────

struct Selection {
    std::optional<int>         page_idx;
    std::optional<std::string> element_id;

    void clear() { page_idx.reset(); element_id.reset(); }

    [[nodiscard]] bool has_element() const { return element_id.has_value(); }
    [[nodiscard]] bool has_page()    const { return page_idx.has_value(); }
    [[nodiscard]] bool empty()       const { return !has_element() && !has_page(); }
};

// ── Layout presets ────────────────────────────────────────────────────────────

enum class LayoutPreset {
    ShaderStudio,   ///< Shader list + text editor + render panel
    CanvasLayers,   ///< Canvas (wide) + Layers + Properties
    SdfScene,       ///< SDF scene (wide) + Layers + Properties
};

// ── Panel visibility ──────────────────────────────────────────────────────────
//  Mirrors PanelSettings — loaded from / saved to settings.json.

struct Panels {
    bool show_layers        = true;
    bool show_properties    = true;
    bool show_canvas        = true;
    bool show_text_editor   = false;
    bool show_shader_list   = false;
    bool show_shader_render = false;
    bool show_sdf_scene     = false;   ///< SDF interactive scene editor

    void from_settings(const ui::PanelSettings& s) noexcept;
    void to_settings(ui::PanelSettings& s)    const noexcept;
};

// ── Application ───────────────────────────────────────────────────────────────

class App {
public:
    // ── Sokol callbacks ───────────────────────────────────────────────────────
    static void init_cb()    noexcept;
    static void frame_cb()   noexcept;
    static void cleanup_cb() noexcept;
    static void event_cb(const sapp_event*) noexcept;

    // ── Singleton ─────────────────────────────────────────────────────────────
    static App& get() noexcept;

    // ── State ─────────────────────────────────────────────────────────────────
    doc::Document             document;
    ui::ResponsiveLayout      layout;
    ui::ImageCache            image_cache;
    renderer::DrawCtx         draw_ctx;
    // Cache for parsed SVG previews (element.id -> parsed primitives)
    std::unordered_map<std::string, panels::SvgData> svg_cache;
    std::unordered_map<std::string, std::size_t> svg_cache_hash;
    renderer::GpuRenderer     gpu_renderer;
    renderer::EffectRenderer  effect_renderer;
    TextEditorPanel           text_editor;
    ShaderListPanel           shader_list;
    ShaderRenderPanel         shader_render;
    panels::SdfPanel          sdf_panel;     ///< interactive SDF scene editor
    Panels                    panels;
    Selection                 selection;
    vfs::VirtualFileSystem    vfs;
    ui::Settings              settings;
    std::filesystem::path     current_path;
    bool                      unsaved_changes = false;
    std::string               status_message;
    DragDropDialog            drag_drop;

    // ── Commands ──────────────────────────────────────────────────────────────
    void new_document();
    void open_document(const std::filesystem::path&);
    void open_file(const std::filesystem::path&);
    void save_document();
    void save_document_as(const std::filesystem::path&);

    // ── Convenience ───────────────────────────────────────────────────────────
    [[nodiscard]] std::string window_title() const;
    void set_status(std::string msg) { status_message = std::move(msg); }

private:
    enum class FileDialogMode { None, Open, SaveAs };
    FileDialogMode file_dialog_mode_ = FileDialogMode::None;

    App()  = default;
    ~App() = default;

    ui::FabNav fab_nav_{"MainNav", ImVec2(40, 100),
                        ui::FabNav::Position::TopRight};
    bool show_menu_bar_ = false;  // toggle: classic top bar vs FAB-only
    std::optional<LayoutPreset> pending_preset_;  ///< applied next draw_dockspace()
    void init_fab_navigation();
    void draw_fab_nav();

    void draw_ui();
    void draw_menu_bar();
    void draw_appearance_menu();  // View > Appearance submenu
    void draw_file_dialog();      // ImGuiFileDialog open/save
    void draw_dockspace();
    void draw_status_bar();
    void register_default_shaders();
    void apply_layout_preset(LayoutPreset preset);
    /// Walk data://shaders/{backend}/ via VFS and register any .frag.* files
    /// that are not yet in the ShaderRegistry (i.e. not listed in gpu_pipeline.json).
    void scan_vfs_shaders();
};

} // namespace shadebug

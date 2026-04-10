#include "app.hpp"
#include "panels/canvas_panel.hpp"
#include "panels/properties_panel.hpp"
#include "panels/layers_panel.hpp"
#include "drag_drop_dialog.hpp"
#include "ui/theme.hpp"
#include "ui/settings.hpp"
#include "ui/icons_fa.hpp"
#include "utils.hpp"
#include "platform.hpp"
#include "renderer/shader_registry.hpp"
#include "renderer/shader_params.hpp"
#include "renderer/shaders.hpp"

#include "sokol_gfx.h"
#include "sokol_glue.h"
#include "sokol_log.h"
#include "imgui.h"
#include "misc/cpp/imgui_stdlib.h"
#include "util/sokol_imgui.h"
#include "ImGuiFileDialog.h"

#include <print>
#include <format>
#include <filesystem>
#include <fstream>
#include <unordered_set>
#include <nlohmann/json.hpp>
#if defined(__APPLE__)
#include <mach-o/dyld.h>
#endif

namespace shadebug {

// ── Panels ↔ PanelSettings ────────────────────────────────────────────────────

void Panels::from_settings(const ui::PanelSettings& s) noexcept {
    show_layers        = s.show_layers;
    show_properties    = s.show_properties;
    show_canvas        = s.show_canvas;
    show_text_editor   = s.show_text_editor;
    show_shader_list   = s.show_shader_list;
    show_shader_render = s.show_shader_render;
    show_sdf_scene     = s.show_sdf_scene;
}

void Panels::to_settings(ui::PanelSettings& s) const noexcept {
    s.show_layers        = show_layers;
    s.show_properties    = show_properties;
    s.show_canvas        = show_canvas;
    s.show_text_editor   = show_text_editor;
    s.show_shader_list   = show_shader_list;
    s.show_shader_render = show_shader_render;
    s.show_sdf_scene     = show_sdf_scene;
}

// ── Singleton ─────────────────────────────────────────────────────────────────

App& App::get() noexcept {
    static App instance;
    return instance;
}

// ── Sokol callbacks ───────────────────────────────────────────────────────────

void App::init_cb() noexcept {
    sg_setup(sg_desc{
        .environment = sglue_environment(),
        .logger      = { .func = slog_func },
    });

    // Load settings first (theme, fonts, panels) — no ImGui calls yet.
    auto& a = get();
    const auto exe = ui::Settings::exe_dir();

    // Bootstrap VFS: bind the App-owned instance, then mount standard paths.
    a.vfs.init(exe);

    const auto settings_path = ui::Settings::default_path();
    if (a.settings.load(settings_path))
        std::println("[Settings] loaded from {}", settings_path.string());
    else
        std::println("[Settings] using defaults ({})", settings_path.string());

    // simgui_setup creates the ImGui context (ImGui::CreateContext).
    // Use no_default_font so we can populate the atlas ourselves right after.
    simgui_setup(simgui_desc_t{
        .no_default_font = true,
        .logger          = { .func = slog_func },
    });

    // Now the ImGui context exists — safe to call GetIO() / Fonts->*.
    // Font paths are resolved via VFS (data://fonts/...).
    a.settings.load_fonts(a.vfs, sapp_dpi_scale());

    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
    io.ConfigDockingWithShift           = false;
    io.ConfigWindowsMoveFromTitleBarOnly = true;   // drag only from title bar / border

    a.settings.apply_theme(sapp_dpi_scale());
    a.panels.from_settings(a.settings.panels);

    a.gpu_renderer.init();
    a.effect_renderer.init();
    a.register_default_shaders();

    // VFS scan: pick up any .frag.* in data://shaders/ not listed in gpu_pipeline.json
    a.scan_vfs_shaders();

    // Wire "Rescan VFS" button in the shader panel back to scan_vfs_shaders()
    a.shader_list.set_rescan_callback([&a]() { a.scan_vfs_shaders(); });

    renderer::ShaderRegistry::get().add_listener(
        [](int /*idx*/, const renderer::ShaderEntry& e) {
            auto& a = App::get();
            if (e.pipeline_type == renderer::PipelineType::Effect)
                std::ignore = a.effect_renderer.recompile(e.vs_src, e.fs_src);
            else
                std::ignore = a.gpu_renderer.recompile(e.vs_src, e.fs_src);
        });

    // ── Wire text editor callbacks ────────────────────────────────────────────

    // "Select a shader" hint when no tabs are open
    a.text_editor.set_empty_hint("← Select a shader from the Pipeline panel");

    // Ctrl+S: write current tab back to its on-disk shader file
    a.text_editor.set_save_callback([](int tab_idx, std::string_view src) {
        auto& app = App::get();
        auto& reg = renderer::ShaderRegistry::get();
        if (!reg.has_selection()) return;

        const int sel = reg.selected();
        if (reg.save_to_disk(sel, tab_idx, src)) {
            app.text_editor.mark_saved(tab_idx);
            const char* stage = (tab_idx == 0) ? "vertex" : "fragment";
            app.set_status(std::format("Saved {} shader: {}",
                stage, reg.entry(sel).name));
        } else {
            app.set_status("Save failed: " + reg.last_error());
        }
    });

    // Ctrl+Enter: update in-memory sources from both editor tabs, then recompile
    a.text_editor.set_apply_callback([](int /*tab_idx*/, std::string_view /*src*/) {
        auto& app = App::get();
        auto& reg = renderer::ShaderRegistry::get();
        if (!reg.has_selection()) return;

        const int sel = reg.selected();
        reg.update_sources(sel, app.text_editor.get_tab(0),
                                app.text_editor.get_tab(1));
        const auto& e   = reg.entry(sel);
        const std::string err =
            (e.pipeline_type == renderer::PipelineType::Effect)
            ? app.effect_renderer.recompile(e.vs_src, e.fs_src)
            : app.gpu_renderer.recompile(e.vs_src, e.fs_src);
        if (err.empty())
            app.set_status(std::format("Shader '{}' recompiled OK", reg.entry(sel).name));
        else
            app.set_status("Recompile error: " + err);
    });

    a.new_document();
    a.init_fab_navigation();

    // Initialise SDF scene panel (loads shaders from exe_dir/data/shaders/msl/)
    a.sdf_panel.init(exe);

    std::println("Shadebug initialised");
}

void App::frame_cb() noexcept {
    auto& a = get();

    const int w = sapp_width();
    const int h = sapp_height();
    const auto dpi = sapp_dpi_scale();
    a.layout.Update(w, h, static_cast<int>(static_cast<float>(w) * dpi),
                          static_cast<int>(static_cast<float>(h) * dpi));

    // Clear CPU command buffer for this frame
    a.draw_ctx.clear();

    simgui_new_frame(simgui_frame_desc_t{
        .width      = w,
        .height     = h,
        .delta_time = sapp_frame_duration(),
        .dpi_scale  = sapp_dpi_scale(),
    });

    a.draw_ui();

    sg_pass_action pa = {};
    pa.colors[0].load_action  = SG_LOADACTION_CLEAR;
    pa.colors[0].clear_value  = { 0.12f, 0.12f, 0.12f, 1.f };

    sg_begin_pass(sg_pass{ .action = pa, .swapchain = sglue_swapchain() });

    // GPU-rendered instanced rects (below ImGui)
    a.gpu_renderer.flush(a.draw_ctx, static_cast<float>(w), static_cast<float>(h));

    // ImGui on top
    simgui_render();

    sg_end_pass();
    sg_commit();
}

void App::cleanup_cb() noexcept {
    auto& a = get();
    // Persist current panel state + theme before shutdown
    a.panels.to_settings(a.settings.panels);
    a.settings.save(ui::Settings::default_path());

    a.effect_renderer.cleanup();
    a.gpu_renderer.cleanup();
    a.sdf_panel.shutdown();
    a.shader_render.destroy_render_target();
    a.image_cache.Clear();
    simgui_shutdown();
    sg_shutdown();
}

void App::event_cb(const sapp_event* e) noexcept {
    if (simgui_handle_event(e)) return;

    if (e->type == SAPP_EVENTTYPE_FILES_DROPPED) {
        std::vector<std::filesystem::path> paths;
        const int n = sapp_get_num_dropped_files();
        for (int i = 0; i < n; ++i)
            paths.emplace_back(sapp_get_dropped_file_path(i));
        get().drag_drop.on_files_dropped(get(), paths);
    }

    // Keyboard shortcuts
    if (e->type == SAPP_EVENTTYPE_KEY_DOWN && !e->key_repeat) {
        const bool mod = (e->modifiers & SAPP_MODIFIER_SUPER) ||
                         (e->modifiers & SAPP_MODIFIER_CTRL);
        auto& a = get();
        if (mod) {
            switch (e->key_code) {
            case SAPP_KEYCODE_N: a.new_document();                             break;
            case SAPP_KEYCODE_S: a.save_document();                            break;
            case SAPP_KEYCODE_O: a.file_dialog_mode_ = FileDialogMode::Open;   break;
            default: break;
            }
        }
    }
}

// ── UI drawing ────────────────────────────────────────────────────────────────

void App::draw_ui() {
    draw_menu_bar();
    draw_dockspace();

    panels::draw_layers_panel(*this);
    panels::draw_canvas_panel(*this);
    panels::draw_properties_panel(*this);

    // Text editor (generic code/text editor)
    text_editor.draw(panels.show_text_editor);

    // Shader pipeline list — syncs text editor on selection
    shader_list.draw(panels.show_shader_list, text_editor, &gpu_renderer, &effect_renderer);

    // Offscreen shader render window — uses its own private DrawCtx
    shader_render.draw(panels.show_shader_render, gpu_renderer, effect_renderer);

    // Interactive SDF scene editor
    sdf_panel.draw(panels.show_sdf_scene);

    draw_file_dialog();
    drag_drop.draw(*this);

    draw_status_bar();

    // FAB nav overlay (drawn last so it's on top)
    draw_fab_nav();
}

void App::draw_menu_bar() {
    if (!ImGui::BeginMainMenuBar()) return;

    const char* mod = platform::kModName;

    // Stack-allocated shortcut strings — no heap alloc
    char sc[16];
    const auto sc_str = [&](char key) -> const char* {
        std::format_to_n(sc, sizeof(sc) - 1, "{}+{}", mod, key).out[0] = '\0';
        return sc;
    };

    if (ImGui::BeginMenu("File")) {
        if (ImGui::MenuItem(FA_FILE "  New",          sc_str('N'))) new_document();
        if (ImGui::MenuItem(FA_FOLDER_OPEN "  Open…", sc_str('O')))
            file_dialog_mode_ = FileDialogMode::Open;
        ImGui::Separator();
        if (ImGui::MenuItem(FA_SAVE "  Save",         sc_str('S'))) save_document();
        if (ImGui::MenuItem(FA_SAVE "  Save As…"))
            file_dialog_mode_ = FileDialogMode::SaveAs;
        ImGui::Separator();
        if (ImGui::MenuItem(FA_TIMES "  Quit")) sapp_quit();
        ImGui::EndMenu();
    }

    if (ImGui::BeginMenu("View")) {
        ImGui::MenuItem(FA_LAYER_GROUP "  Layers",       nullptr, &panels.show_layers);
        ImGui::MenuItem(FA_EDIT "  Properties",          nullptr, &panels.show_properties);
        ImGui::Separator();
        ImGui::MenuItem(FA_FILE_TEXT "  Text Editor",    nullptr, &panels.show_text_editor);
        ImGui::MenuItem(FA_CODE "  Shader Pipeline",     nullptr, &panels.show_shader_list);
        ImGui::MenuItem(FA_IMAGE "  Shader Render",      nullptr, &panels.show_shader_render);
        ImGui::MenuItem(FA_MAGIC "  SDF Scene",          nullptr, &panels.show_sdf_scene);
        ImGui::MenuItem(FA_CUBE "  3D Scene",            nullptr, &panels.show_shader_render);
        ImGui::Separator();
        draw_appearance_menu();
        ImGui::EndMenu();
    }

    if (ImGui::BeginMenu("Page")) {
        if (ImGui::MenuItem("Add Page")) {
            document.add_page();
            unsaved_changes = true;
        }
        ImGui::EndMenu();
    }

    if (ImGui::BeginMenu("Debug")) {
        bool v = gpu_renderer.verbose();
        if (ImGui::MenuItem("GPU Verbose Logging", nullptr, &v))
            gpu_renderer.set_verbose(v);
        if (ImGui::IsItemHovered())
            ImGui::SetTooltip("Print GpuRenderer::flush() diagnostics to stdout\n"
                              "(rect count, pass type, buffer offset)");
        ImGui::Separator();
        if (ImGui::MenuItem("Dump DrawCtx Stats")) {
            const auto& ctx = draw_ctx;
            std::println("[Debug] DrawCtx: {}/{} rects used ({} bytes)",
                ctx.count(), shadebug::renderer::DrawCtx::kMaxRects,
                ctx.count() * static_cast<int>(sizeof(shadebug::renderer::UiRect)));
        }
        ImGui::EndMenu();
    }

    // Right-aligned unsaved indicator
    if (unsaved_changes) {
        const float w = ImGui::CalcTextSize("●  ").x + 8.f;
        ImGui::SetCursorPosX(ImGui::GetContentRegionMax().x - w);
        ImGui::TextColored({ 1.f, 0.8f, 0.2f, 1.f }, "●");
        utils::maybe_tooltip("Unsaved changes");
    }

    ImGui::EndMainMenuBar();
}

void App::init_fab_navigation() {
    // ── File menu ─────────────────────────────────────────────────────────────
    fab_nav_.add_button("file", FA_FILE);
    fab_nav_.add_submenu("file", "new",    FA_FILE,        "New",      [this]() { new_document(); });
    fab_nav_.add_submenu("file", "open",   FA_FOLDER_OPEN, "Open",     [this]() { file_dialog_mode_ = FileDialogMode::Open; });
    fab_nav_.add_submenu("file", "save",   FA_SAVE,        "Save",     [this]() { save_document(); });
    fab_nav_.add_submenu("file", "saveas", FA_SAVE,        "Save As",  [this]() { file_dialog_mode_ = FileDialogMode::SaveAs; });

    // ── View menu ─────────────────────────────────────────────────────────────
    fab_nav_.add_button("view", FA_EYE);
    fab_nav_.add_submenu("view", "layers", FA_LAYER_GROUP, "Layers",     [this]() { panels.show_layers     = !panels.show_layers; });
    fab_nav_.add_submenu("view", "props",  FA_COG,         "Properties", [this]() { panels.show_properties = !panels.show_properties; });
    fab_nav_.add_submenu("view", "editor",  FA_CODE,        "Editor",     [this]() { panels.show_text_editor = !panels.show_text_editor; });
    fab_nav_.add_submenu("view", "shader",  FA_FILM,        "Shaders",    [this]() { panels.show_shader_list = !panels.show_shader_list; });
    fab_nav_.add_submenu("view", "sdf",     FA_MAGIC,       "SDF Scene",  [this]() { panels.show_sdf_scene   = !panels.show_sdf_scene;   });
    fab_nav_.add_submenu("view", "render3d", FA_CUBE,       "3D Scene",   [this]() {
        panels.show_shader_render = !panels.show_shader_render;
        if (panels.show_shader_render)
            shader_render.set_mode(RenderMode::Solid3D);
    });

    // ── Layout toggle (no submenu — fires immediately) ────────────────────────
    fab_nav_.add_button("nav", FA_BARS, "",
        [this]() { show_menu_bar_ = !show_menu_bar_; });
}

void App::draw_fab_nav() {
    // No window needed — FabNav renders directly onto the foreground draw list.
    fab_nav_.draw();
}

void App::draw_appearance_menu() {
    if (!ImGui::BeginMenu("Appearance")) return;

    // ── Theme picker ─────────────────────────────────────────────────────────
    ImGui::SeparatorText("Theme");
    for (int i = 0; i < static_cast<int>(ui::ThemeType::Count); ++i) {
        const auto t    = static_cast<ui::ThemeType>(i);
        const bool sel  = (settings.theme.active == t);
        if (ImGui::MenuItem(ui::theme_name(t).data(), nullptr, sel)) {
            settings.theme.active = t;
            // PixelDark ships with its own tight style defaults
            if (t == ui::ThemeType::PixelDark)
                settings.theme.style = ui::Theme::pixel_dark_style();
            settings.apply_theme(sapp_dpi_scale());
        }
    }

    // ── Style sliders ─────────────────────────────────────────────────────────
    ImGui::SeparatorText("Style");
    auto& sp = settings.theme.style;
    bool changed = false;

    // Default StyleParams for the active theme (used by reset buttons)
    const ui::StyleParams def = (settings.theme.active == ui::ThemeType::PixelDark)
        ? ui::Theme::pixel_dark_style()
        : ui::StyleParams{};

    // Helper: small reset button — returns true if clicked
    const auto reset_btn = [](const char* id, float& val, float default_val, bool& ch) {
        ImGui::SameLine();
        ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(2.f, 1.f));
        ImGui::PushStyleColor(ImGuiCol_Text, ImGui::GetStyleColorVec4(ImGuiCol_TextDisabled));
        const bool hit = ImGui::SmallButton(id);
        ImGui::PopStyleColor();
        ImGui::PopStyleVar();
        if (hit) { val = default_val; ch = true; }
        if (ImGui::IsItemHovered()) ImGui::SetTooltip("Reset to %.4g", default_val);
    };

    ImGui::SetNextItemWidth(160.f);
    changed |= ImGui::SliderFloat("Rounding##r",       &sp.rounding,      0.f, 12.f, "%.1f");
    reset_btn("↺##rst_r",  sp.rounding,      def.rounding,      changed);

    ImGui::SetNextItemWidth(160.f);
    changed |= ImGui::SliderFloat("Item Spacing##is",  &sp.item_spacing,  2.f, 16.f, "%.1f");
    reset_btn("↺##rst_is", sp.item_spacing,  def.item_spacing,  changed);

    ImGui::SetNextItemWidth(160.f);
    changed |= ImGui::SliderFloat("Frame Padding##fp",  &sp.frame_padding, 2.f, 14.f, "%.1f");
    reset_btn("↺##rst_fp", sp.frame_padding, def.frame_padding, changed);

    ImGui::SetNextItemWidth(160.f);
    changed |= ImGui::SliderFloat("Font Scale##fs",    &sp.font_scale,    0.7f, 2.0f, "%.2f");
    reset_btn("↺##rst_fs", sp.font_scale,    def.font_scale,    changed);
    // Quick-snap to ×1.0
    ImGui::SameLine();
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(2.f, 1.f));
    if (ImGui::SmallButton("1×")) { sp.font_scale = 1.f; changed = true; }
    ImGui::PopStyleVar();
    if (ImGui::IsItemHovered()) ImGui::SetTooltip("Snap font scale to 1.0");

    ImGui::Spacing();
    if (ImGui::SmallButton("Reset all style")) {
        sp = def;
        changed = true;
    }

    if (changed)
        settings.apply_theme(sapp_dpi_scale());

    // ── Font settings (require restart) ──────────────────────────────────────
    ImGui::SeparatorText("Fonts  (requires restart)");

    {
        auto& mf = settings.theme.main_font;
        ImGui::SetNextItemWidth(240.f);
        ImGui::InputText("Main font##mfpath", &mf.path);
        ImGui::SameLine();
        ImGui::SetNextItemWidth(70.f);
        ImGui::SliderFloat("##mfsize", &mf.size_px, 8.f, 32.f, "%.0fpx");
        ImGui::TextDisabled("e.g. data://fonts/ProggyClean.ttf");
    }
    {
        auto& icf = settings.theme.icon_font;
        ImGui::SetNextItemWidth(240.f);
        ImGui::InputText("Icon font##icpath", &icf.path);
        ImGui::SameLine();
        ImGui::SetNextItemWidth(70.f);
        ImGui::SliderFloat("##icsize", &icf.size_px, 8.f, 32.f, "%.0fpx");
        ImGui::TextDisabled("e.g. data://fonts/fontawesome-webfont.ttf");
    }

    // ── Layout presets ────────────────────────────────────────────────────────
    ImGui::SeparatorText("Layout Presets");

    struct PresetEntry { const char* icon; const char* label; LayoutPreset preset; const char* tip; };
    constexpr PresetEntry kPresets[] = {
        { FA_CODE,       "Shader Studio",  LayoutPreset::ShaderStudio,
          "Shader list • Text editor • Render panel" },
        { FA_IMAGE,      "Canvas + Layers",LayoutPreset::CanvasLayers,
          "Canvas (wide) • Layers • Properties" },
        { FA_MAGIC,      "SDF Scene",      LayoutPreset::SdfScene,
          "SDF editor (wide) • Layers • Properties" },
    };
    for (const auto& e : kPresets) {
        const std::string lbl = std::string(e.icon) + "  " + e.label + "##lp";
        if (ImGui::MenuItem(lbl.c_str())) {
            pending_preset_ = e.preset;
        }
        if (ImGui::IsItemHovered()) ImGui::SetTooltip("%s", e.tip);
    }

    ImGui::Separator();
    if (ImGui::MenuItem("Save Settings")) {
        panels.to_settings(settings.panels);
        if (settings.save(ui::Settings::default_path()))
            set_status("Settings saved.");
    }

    ImGui::EndMenu();
}

void App::draw_file_dialog() {
    // Open pending dialogs (triggered from menu or keyboard shortcut)
    if (file_dialog_mode_ == FileDialogMode::Open) {
        IGFD::FileDialogConfig cfg;
        cfg.path  = current_path.empty()
                  ? std::filesystem::current_path().string()
                  : current_path.parent_path().string();
        cfg.flags = ImGuiFileDialogFlags_Modal;
        ImGuiFileDialog::Instance()->OpenDialog(
            "OpenFileDlg", FA_FOLDER_OPEN "  Open File",
            ".son,.json", cfg);
        file_dialog_mode_ = FileDialogMode::None;
    }
    if (file_dialog_mode_ == FileDialogMode::SaveAs) {
        IGFD::FileDialogConfig cfg;
        cfg.path     = current_path.empty()
                     ? std::filesystem::current_path().string()
                     : current_path.parent_path().string();
        cfg.fileName = current_path.empty() ? "untitled.docjson"
                                            : current_path.filename().string();
        cfg.flags    = ImGuiFileDialogFlags_Modal
                     | ImGuiFileDialogFlags_ConfirmOverwrite;
        ImGuiFileDialog::Instance()->OpenDialog(
            "SaveFileDlg", FA_SAVE "  Save As",
            ".json", cfg);
        file_dialog_mode_ = FileDialogMode::None;
    }

    // Display open dialog
    const ImVec2 dlg_size{ 720.f, 480.f };
    if (ImGuiFileDialog::Instance()->Display("OpenFileDlg", ImGuiWindowFlags_NoCollapse, dlg_size)) {
        if (ImGuiFileDialog::Instance()->IsOk())
            open_file(ImGuiFileDialog::Instance()->GetFilePathName());
        ImGuiFileDialog::Instance()->Close();
    }

    // Display save-as dialog
    if (ImGuiFileDialog::Instance()->Display("SaveFileDlg", ImGuiWindowFlags_NoCollapse, dlg_size)) {
        if (ImGuiFileDialog::Instance()->IsOk())
            save_document_as(ImGuiFileDialog::Instance()->GetFilePathName());
        ImGuiFileDialog::Instance()->Close();
    }
}

void App::draw_dockspace() {
    const ImGuiViewport* vp = ImGui::GetMainViewport();
    ImGui::SetNextWindowPos(vp->WorkPos);
    ImGui::SetNextWindowSize(vp->WorkSize);
    ImGui::SetNextWindowViewport(vp->ID);

    constexpr ImGuiWindowFlags kFlags =
        ImGuiWindowFlags_NoDocking             |
        ImGuiWindowFlags_NoTitleBar            |
        ImGuiWindowFlags_NoCollapse            |
        ImGuiWindowFlags_NoResize              |
        ImGuiWindowFlags_NoMove                |
        ImGuiWindowFlags_NoBringToFrontOnFocus |
        ImGuiWindowFlags_NoNavFocus            |
        ImGuiWindowFlags_NoBackground;

    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding,    { 0.f, 0.f });
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding,   0.f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.f);
    ImGui::Begin("##root_dock", nullptr, kFlags);
    ImGui::PopStyleVar(3);

    const ImGuiID dsid = ImGui::GetID("MainDock");
    ImGui::DockSpace(dsid, { 0.f, 0.f }, ImGuiDockNodeFlags_PassthruCentralNode);

    // Apply a layout preset if one was requested last frame
    if (pending_preset_.has_value()) {
        apply_layout_preset(*pending_preset_);
        pending_preset_.reset();
    }

    ImGui::End();
}

// ── Layout presets ────────────────────────────────────────────────────────────

void App::apply_layout_preset(LayoutPreset preset) {
    const ImGuiViewport* vp   = ImGui::GetMainViewport();
    const ImGuiID        dsid = ImGui::GetID("MainDock");

    ImGui::DockBuilderRemoveNode(dsid);
    ImGui::DockBuilderAddNode(dsid, ImGuiDockNodeFlags_DockSpace);
    ImGui::DockBuilderSetNodeSize(dsid, vp->WorkSize);

    switch (preset) {

    // ── Shader Studio ─────────────────────────────────────────────────────────
    //  [ Shader Pipeline | Text Editor (top) ]
    //  [                 | Shader Render (btm)]
    case LayoutPreset::ShaderStudio: {
        panels.show_layers        = false;
        panels.show_properties    = false;
        panels.show_canvas        = false;
        panels.show_sdf_scene     = false;
        panels.show_text_editor   = true;
        panels.show_shader_list   = true;
        panels.show_shader_render = true;

        ImGuiID left, right;
        ImGui::DockBuilderSplitNode(dsid, ImGuiDir_Left, 0.22f, &left, &right);

        ImGuiID right_top, right_btm;
        ImGui::DockBuilderSplitNode(right, ImGuiDir_Up, 0.55f, &right_top, &right_btm);

        ImGui::DockBuilderDockWindow("Shader Pipeline##list",       left);
        ImGui::DockBuilderDockWindow("###shadebug_text_editor",     right_top);
        ImGui::DockBuilderDockWindow("Shader Render##render_panel", right_btm);
        break;
    }

    // ── Canvas + Layers ───────────────────────────────────────────────────────
    //  [ Canvas (wide) | Layers     ]
    //  [               | Properties ]
    case LayoutPreset::CanvasLayers: {
        panels.show_layers        = true;
        panels.show_properties    = true;
        panels.show_canvas        = true;
        panels.show_sdf_scene     = false;
        panels.show_text_editor   = false;
        panels.show_shader_list   = false;
        panels.show_shader_render = false;

        ImGuiID center, right;
        ImGui::DockBuilderSplitNode(dsid, ImGuiDir_Right, 0.22f, &right, &center);

        ImGuiID right_top, right_btm;
        ImGui::DockBuilderSplitNode(right, ImGuiDir_Up, 0.55f, &right_top, &right_btm);

        ImGui::DockBuilderDockWindow("Canvas",     center);
        ImGui::DockBuilderDockWindow("Layers",     right_top);
        ImGui::DockBuilderDockWindow("Properties", right_btm);
        break;
    }

    // ── SDF Scene ─────────────────────────────────────────────────────────────
    //  [ SDF Scene (wide) | Layers     ]
    //  [                  | Properties ]
    case LayoutPreset::SdfScene: {
        panels.show_layers        = true;
        panels.show_properties    = true;
        panels.show_canvas        = false;
        panels.show_sdf_scene     = true;
        panels.show_text_editor   = false;
        panels.show_shader_list   = false;
        panels.show_shader_render = false;

        ImGuiID center, right;
        ImGui::DockBuilderSplitNode(dsid, ImGuiDir_Right, 0.22f, &right, &center);

        ImGuiID right_top, right_btm;
        ImGui::DockBuilderSplitNode(right, ImGuiDir_Up, 0.55f, &right_top, &right_btm);

        ImGui::DockBuilderDockWindow("SDF Scene##sdf_panel", center);
        ImGui::DockBuilderDockWindow("Layers",               right_top);
        ImGui::DockBuilderDockWindow("Properties",           right_btm);
        break;
    }
    }

    ImGui::DockBuilderFinish(dsid);
}

void App::draw_status_bar() {
    const ImGuiViewport* vp = ImGui::GetMainViewport();
    const float bar_h = ImGui::GetFrameHeight();

    ImGui::SetNextWindowPos({ vp->WorkPos.x, vp->WorkPos.y + vp->WorkSize.y - bar_h });
    ImGui::SetNextWindowSize({ vp->WorkSize.x, bar_h });
    ImGui::SetNextWindowViewport(vp->ID);

    constexpr ImGuiWindowFlags kFlags =
        ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoInputs   |
        ImGuiWindowFlags_NoMove       | ImGuiWindowFlags_NoNav      |
        ImGuiWindowFlags_NoScrollbar  | ImGuiWindowFlags_NoDocking;

    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, { 8.f, 2.f });
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.f);
    ImGui::Begin("##status_bar", nullptr, kFlags);
    ImGui::PopStyleVar(2);

    ImGui::TextDisabled("%s", status_message.c_str());

    // Layout mode tag (right-aligned)
    const char* mode = layout.isPhone() ? "Phone" :
                       layout.isTablet()? "Tablet" : "Desktop";
    const float tw = ImGui::CalcTextSize(mode).x + 12.f;
    ImGui::SameLine(ImGui::GetContentRegionMax().x - tw);
    ImGui::TextDisabled("%s", mode);

    ImGui::End();
}

// ── Commands ──────────────────────────────────────────────────────────────────

void App::new_document() {
    document        = doc::Document::make_default();
    current_path    = std::filesystem::path{};
    unsaved_changes = false;
    selection.clear();
    set_status("New document created");
}

void App::open_document(const std::filesystem::path& path) {
    auto result = doc::Document::load(path);
    if (result) {
        document        = std::move(*result);
        current_path    = path;
        unsaved_changes = false;
        selection.clear();
        set_status("Opened: " + path.filename().string());
    } else {
        set_status("Error: " + result.error());
    }
}

void App::save_document() {
    if (current_path.empty()) {
        set_status("No path — use Save As…");
        return;
    }
    save_document_as(current_path);
}

void App::save_document_as(const std::filesystem::path& path) {
    auto result = document.save(path);
    if (result) {
        current_path    = path;
        unsaved_changes = false;
        set_status("Saved: " + path.filename().string());
    } else {
        set_status("Save failed: " + result.error());
    }
}

std::string App::window_title() const {
    std::string t = document.title.empty() ? "Untitled" : document.title;
    if (unsaved_changes) t += " *";
    t += " — Folio";
    return t;
}

void App::open_file(const std::filesystem::path& path) {
    const auto ext = path.extension().string();
    if (ext == ".json" || ext == ".docjson") {
        open_document(path);
    } else if (ext == ".txt" || ext == ".md" || ext == ".glsl" ||
               ext == ".metal" || ext == ".hlsl") {
        // Open as text in the text editor
        std::ifstream f(path, std::ios::ate | std::ios::binary);
        if (!f) { set_status("Cannot open: " + path.filename().string()); return; }
        const auto sz = f.tellg(); f.seekg(0);
        std::string src(static_cast<std::size_t>(sz), '\0');
        f.read(src.data(), sz);
        text_editor.clear(path.filename().string());
        text_editor.set_tab(path.stem().string(), std::move(src), ext.substr(1));
        panels.show_text_editor = true;
        set_status("Opened in text editor: " + path.filename().string());
    } else {
        set_status("Unknown file type: " + path.filename().string());
    }
}

void App::register_default_shaders() {
    auto& reg = renderer::ShaderRegistry::get();

    namespace fs = std::filesystem;
    const fs::path exe_dir = []() -> fs::path {
        char buf[4096] = {};
#if defined(__APPLE__)
        uint32_t sz = sizeof(buf);
        if (_NSGetExecutablePath(buf, &sz) == 0)
            return fs::path(buf).parent_path();
#endif
        return fs::current_path();
    }();

    // ── Try gpu_pipeline.json first ───────────────────────────────────────────
    const fs::path json_path = exe_dir / "data" / "gpu_pipeline.json";
    if (fs::exists(json_path)) {
        std::ifstream f(json_path);
        if (f) {
            try {
                const auto j = nlohmann::json::parse(f);
                for (const auto& pip : j.at("pipelines")) {
                    const std::string name = pip.at("name");

#if defined(SOKOL_METAL)
                    const std::string backend = "metal";
#elif defined(SOKOL_D3D11)
                    const std::string backend = "hlsl";
#else
                    const std::string backend = "glsl";
#endif
                    if (!pip.contains("shaders") || !pip["shaders"].contains(backend))
                        continue;

                    const auto& shd     = pip["shaders"][backend];
                    const fs::path vs_p = exe_dir / shd.at("vertex").get<std::string>();
                    const fs::path fs_p = exe_dir / shd.at("fragment").get<std::string>();

                    const int idx = reg.add_from_files(vs_p, fs_p, name);
                    if (idx >= 0) {
                        // Store draw roles as metadata on the entry
                        auto& entry = reg.entry(idx);
                        if (pip.contains("roles"))
                            entry.roles = pip["roles"].get<std::vector<std::string>>();
                        if (pip.contains("draw"))
                            entry.draw_desc = pip["draw"].dump();
                        if (pip.contains("description"))
                            entry.description = pip["description"].get<std::string>();
                        if (pip.contains("type")) {
                            const auto t = pip["type"].get<std::string>();
                            entry.pipeline_type = (t == "effect")
                                ? renderer::PipelineType::Effect
                                : renderer::PipelineType::Rect;
                        }
                        if (pip.contains("params"))
                            entry.params = renderer::parse_shader_params(pip["params"]);

                        std::println("[App] Pipeline '{}' loaded from gpu_pipeline.json", name);
                    } else {
                        std::println("[App] Pipeline '{}' failed: {}", name, reg.last_error());
                        // Fall through to embedded fallback below
                    }
                }

                if (reg.count() > 0) return;
            } catch (const std::exception& ex) {
                std::println("[App] gpu_pipeline.json parse error: {}", ex.what());
            }
        }
    }

    // ── Embedded fallback ────────────────────────────────────────────────────
    reg.add("rect",
            std::string(renderer::shaders::vert_source()),
            std::string(renderer::shaders::frag_source()));
    std::println("[App] Using built-in shader sources (fallback)");
}

void App::scan_vfs_shaders() {
    auto& reg = renderer::ShaderRegistry::get();

#if defined(SOKOL_METAL)
    constexpr std::string_view backend  = "msl";
    constexpr std::string_view frag_ext = ".frag.metal";
    constexpr std::string_view vert_ext = ".vert.metal";
#elif defined(SOKOL_D3D11)
    constexpr std::string_view backend  = "hlsl";
    constexpr std::string_view frag_ext = ".frag.hlsl";
    constexpr std::string_view vert_ext = ".vert.hlsl";
#else
    constexpr std::string_view backend  = "glsl";
    constexpr std::string_view frag_ext = ".frag.glsl";
    constexpr std::string_view vert_ext = ".vert.glsl";
#endif

    const std::string dir_uri = "data://shaders/" + std::string(backend) + "/";

    // Build a fast lookup of already-registered fragment paths (canonical)
    std::unordered_set<std::string> known_fs;
    for (int i = 0; i < reg.count(); ++i) {
        const auto& e = reg.entry(i);
        if (e.fs_path.empty()) continue;
        std::error_code ec;
        auto canon = std::filesystem::canonical(e.fs_path, ec);
        if (!ec) known_fs.insert(canon.string());
        else     known_fs.insert(e.fs_path.string());  // fallback: non-canonical
    }

    const auto files = vfs.list(dir_uri);
    int new_count = 0;

    for (const auto& fname : files) {
        // Only fragment shaders; skip vertex shaders and subdirectories
        if (!fname.ends_with(std::string(frag_ext))) continue;

        auto frag_opt = vfs.find(dir_uri + fname);
        if (!frag_opt) continue;

        // Already registered by resolved path?
        std::error_code ec;
        const auto canon = std::filesystem::canonical(*frag_opt, ec);
        const std::string key = ec ? frag_opt->string() : canon.string();
        if (known_fs.contains(key)) continue;

        // Derive display name: strip the .frag.* suffix
        const std::string base(fname.substr(0, fname.size() - frag_ext.size()));

        // Prefer a same-name vertex shader, then fall back to "effect.vert.*"
        auto vert_opt = vfs.find(dir_uri + base + std::string(vert_ext));
        if (!vert_opt) vert_opt = vfs.find(dir_uri + "effect" + std::string(vert_ext));
        if (!vert_opt) {
            std::println("[App] VFS scan: no vertex shader for '{}', skipping", base);
            continue;
        }

        const int idx = reg.add_from_files(*vert_opt, *frag_opt, base);
        if (idx >= 0) {
            reg.entry(idx).pipeline_type = renderer::PipelineType::Effect;
            ++new_count;
            std::println("[App] VFS scan: registered '{}'", base);
        } else {
            std::println("[App] VFS scan: failed to register '{}': {}", base, reg.last_error());
        }
    }

    std::println("[App] VFS scan: {} new shader(s) from data://shaders/{}/",
                 new_count, std::string(backend));
}

} // namespace shadebug

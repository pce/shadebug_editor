#pragma once

#include "../renderer/shader_registry.hpp"
#include "../renderer/gpu_renderer.hpp"
#include "../renderer/effect_renderer.hpp"
#include "text_editor_panel.hpp"
#include <functional>

class ShaderListPanel {
public:
    ShaderListPanel();
    ~ShaderListPanel();

    /// Draw the flat shader list.
    /// `editor` — the text editor panel to sync on selection change.
    /// `visible` — driven by View menu toggle.
    void draw(bool& visible, TextEditorPanel& editor,
              shadebug::renderer::GpuRenderer* renderer = nullptr,
              shadebug::renderer::EffectRenderer* effect_renderer = nullptr);

    /// Called when the user clicks "Scan VFS" — should call App::scan_vfs_shaders().
    void set_rescan_callback(std::function<void()> cb) { on_rescan_ = std::move(cb); }

private:
    int listener_handle_ = -1;
    std::string status_msg_;
    std::function<void()> on_rescan_;

    void sync_editor(int idx, const shadebug::renderer::ShaderEntry& e,
                     TextEditorPanel& editor) const;
    void try_recompile(int idx, TextEditorPanel& editor,
                       shadebug::renderer::GpuRenderer* renderer,
                       shadebug::renderer::EffectRenderer* effect_renderer);
};

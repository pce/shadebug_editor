#include "shader_list_panel.hpp"
#include "../renderer/gpu_renderer.hpp"
#include "../renderer/effect_renderer.hpp"
#include "imgui.h"

#include <cstring>
#include <format>

namespace {

[[nodiscard]] static const char* lang_hint_for(const shadebug::renderer::ShaderEntry& /*e*/) {
#if defined(SOKOL_METAL)
    return "msl";
#elif defined(SOKOL_D3D11)
    return "hlsl";
#else
    return "glsl";
#endif
}

} // anon

ShaderListPanel::ShaderListPanel() {
    // Register listener: when selection changes → caller calls sync_editor
    // (we do it lazily inside draw() so we have the editor reference)
}

ShaderListPanel::~ShaderListPanel() {
    if (listener_handle_ >= 0)
        shadebug::renderer::ShaderRegistry::get().remove_listener(listener_handle_);
}

void ShaderListPanel::draw(bool& visible, TextEditorPanel& editor,
                            shadebug::renderer::GpuRenderer* gpu_renderer,
                            shadebug::renderer::EffectRenderer* effect_renderer) {
    if (!visible) return;

    // Register listener once
    if (listener_handle_ < 0) {
        listener_handle_ = shadebug::renderer::ShaderRegistry::get().add_listener(
            [this, &editor](int idx, const shadebug::renderer::ShaderEntry& e) {
                sync_editor(idx, e, editor);
            });

        // Sync editor to current selection on first open
        if (auto* sel = shadebug::renderer::ShaderRegistry::get().selected_entry())
            sync_editor(shadebug::renderer::ShaderRegistry::get().selected(), *sel, editor);
    }

    auto& reg = shadebug::renderer::ShaderRegistry::get();

    ImGui::SetNextWindowSize(ImVec2(280, 400), ImGuiCond_FirstUseEver);
    if (!ImGui::Begin("Shader Pipeline##list", &visible)) {
        ImGui::End();
        return;
    }

    ImGui::TextDisabled("%d shader(s)", reg.count());
    ImGui::SameLine();
    if (ImGui::SmallButton("Reload All")) {
        for (int i = 0; i < reg.count(); ++i) reg.reload(i);
        if (auto* sel = reg.selected_entry())
            sync_editor(reg.selected(), *sel, editor);
        status_msg_ = "Reloaded from disk.";
    }
    ImGui::SameLine();
    if (ImGui::SmallButton("Scan VFS")) {
        if (on_rescan_) {
            on_rescan_();
            status_msg_ = std::format("{} shader(s) total after scan", reg.count());
        }
    }
    if (ImGui::IsItemHovered())
        ImGui::SetTooltip("Walk data://shaders/ and register any\n"
                          ".frag.* files not yet in the pipeline list");
    ImGui::Separator();

    for (int i = 0; i < reg.count(); ++i) {
        const auto& e        = reg.entry(i);
        const bool  selected = (reg.selected() == i);

        ImGui::PushID(i);

        if (ImGui::Selectable(e.name.c_str(), selected,
                              ImGuiSelectableFlags_None, ImVec2(0, 0))) {
            reg.select(i);
        }

        // Roles badge inline — build on hover only to avoid per-frame alloc
        if (!e.roles.empty()) {
            ImGui::SameLine();
            // Join roles with ", " — stack buffer, no heap alloc for typical role counts
            char roles_buf[128] = {};
            std::size_t pos = 0;
            for (bool first = true; const auto& r : e.roles) {
                if (!first && pos + 2 < sizeof(roles_buf) - 1) {
                    roles_buf[pos++] = ',';
                    roles_buf[pos++] = ' ';
                }
                const auto n = std::min(r.size(), sizeof(roles_buf) - pos - 1);
                std::memcpy(roles_buf + pos, r.data(), n);
                pos += n;
                first = false;
            }
            ImGui::TextDisabled("[%s]", roles_buf);
        }

        // Context menu per entry
        if (ImGui::BeginPopupContextItem("##ctx")) {
            if (ImGui::MenuItem("Reload from disk")) {
                reg.reload(i);
                if (i == reg.selected())
                    sync_editor(i, reg.entry(i), editor);
            }
            if (ImGui::MenuItem("Recompile")) {
                try_recompile(i, editor, gpu_renderer, effect_renderer);
            }
            ImGui::EndPopup();
        }

        // Tooltip: paths + draw_desc (only computed when actually hovered)
        if (ImGui::IsItemHovered()) {
            if (!e.vs_path.empty() || !e.draw_desc.empty()) {
                ImGui::BeginTooltip();
                if (!e.vs_path.empty()) {
                    ImGui::TextUnformatted(e.vs_path.string().c_str());
                    ImGui::TextUnformatted(e.fs_path.string().c_str());
                }
                if (!e.draw_desc.empty()) {
                    ImGui::Separator();
                    ImGui::TextDisabled("Draw config:");
                    ImGui::TextUnformatted(e.draw_desc.c_str());
                }
                ImGui::EndTooltip();
            }
        }

        ImGui::PopID();
    }

    ImGui::Separator();

    if (reg.has_selection()) {
        if (ImGui::Button("Recompile Selected")) {
            try_recompile(reg.selected(), editor, gpu_renderer, effect_renderer);
        }
        ImGui::SameLine();
        if (!status_msg_.empty())
            ImGui::TextDisabled("%s", status_msg_.c_str());
    }

    // Last error from registry
    if (!reg.last_error().empty())
        ImGui::TextColored({1.f, 0.4f, 0.4f, 1.f}, "%s", reg.last_error().c_str());

    ImGui::End();
}


void ShaderListPanel::sync_editor(int /*idx*/, const shadebug::renderer::ShaderEntry& e,
                                   TextEditorPanel& editor) const {
    const char* hint = lang_hint_for(e);

    // Use set_title() + set_tab() instead of clear() + set_tab().
    // clear() destroys all ImGui tab-bar state so the tab bar resets to the
    // first tab (Vertex) on every shader switch — even if Fragment was active.
    // set_title() only updates the window display name; set_tab() updates the
    // buffer in-place if the tab already exists, or appends it if not.
    // ImGui's tab selection (SelectedTabId) is never touched → active tab stays.
    editor.set_title(e.name + " — Shader");
    editor.set_tab("Vertex",   e.vs_src, hint);
    editor.set_tab("Fragment", e.fs_src, hint);
}

void ShaderListPanel::try_recompile(int idx, TextEditorPanel& editor,
                                     shadebug::renderer::GpuRenderer* renderer,
                                     shadebug::renderer::EffectRenderer* effect_renderer) {
    auto& reg = shadebug::renderer::ShaderRegistry::get();

    // Pull latest content from editor if this is the selected shader
    if (idx == reg.selected()) {
        reg.update_sources(idx,
            editor.get_tab(0),   // Vertex — string_view, zero-copy
            editor.get_tab(1));  // Fragment
    }

    const auto& e = reg.entry(idx);
    if (e.pipeline_type == shadebug::renderer::PipelineType::Effect) {
        if (effect_renderer) {
            const std::string err = effect_renderer->recompile(e.vs_src, e.fs_src);
            status_msg_ = err.empty() ? "OK" : std::format("Error: {}", err);
        } else {
            status_msg_ = "(no effect renderer)";
        }
    } else {
        if (renderer) {
            const std::string err = renderer->recompile(e.vs_src, e.fs_src);
            status_msg_ = err.empty() ? "OK" : std::format("Error: {}", err);
        } else {
            status_msg_ = "(no renderer)";
        }
    }
}

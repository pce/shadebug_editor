#include "layers_panel.hpp"
#include "../app.hpp"
#include "imgui.h"
#include <ranges>

namespace shadebug::panels {

namespace {

void draw_element_row(shadebug::App& app, doc::Element& elem, int page_idx) {
    ImGui::PushID(elem.id.c_str());

    // Visibility checkbox
    ImGui::Checkbox("##vis", &elem.visible);
    ImGui::SameLine();

    // Lock indicator
    ImGui::BeginDisabled(elem.locked);
    const bool selected = app.selection.element_id == elem.id;
    if (ImGui::Selectable(elem.name.c_str(), selected,
                          ImGuiSelectableFlags_SpanAllColumns)) {
        app.selection.page_idx  = page_idx;
        app.selection.element_id = elem.id;
    }
    ImGui::EndDisabled();

    if (ImGui::BeginPopupContextItem("##ctx")) {
        if (ImGui::MenuItem("Rename…")) {}
        if (ImGui::MenuItem(elem.locked ? "Unlock" : "Lock")) elem.locked = !elem.locked;
        ImGui::Separator();
        if (ImGui::MenuItem("Delete")) {
            // Mark for removal (safe to handle outside loop)
            elem.visible = false; // placeholder until we implement deletion
        }
        ImGui::EndPopup();
    }

    ImGui::PopID();
}

} // namespace

void draw_layers_panel(shadebug::App& app) {
    if (!app.panels.show_layers) return;

    ImGui::Begin("Layers", &app.panels.show_layers);

    auto& doc = app.document;

    // ── Page tabs ─────────────────────────────────────────────────────────────
    if (ImGui::BeginTabBar("##pages")) {
        for (int pi = 0; pi < static_cast<int>(doc.pages.size()); ++pi) {
            auto& page = doc.pages[static_cast<std::size_t>(pi)];
            const bool tab_open = ImGui::BeginTabItem(page.name.c_str());
            if (ImGui::IsItemClicked())
                doc.active_page_idx = pi;
            if (!tab_open) continue;
            doc.active_page_idx = pi;


            for (auto& layer : page.layers) {
                ImGui::PushID(layer.id.c_str());
                ImGui::SetNextItemOpen(true, ImGuiCond_Once);

                bool vis = layer.visible;
                ImGui::Checkbox("##lvis", &vis);
                layer.visible = vis;
                ImGui::SameLine();

                const bool node_open = ImGui::TreeNodeEx(
                    layer.name.c_str(),
                    ImGuiTreeNodeFlags_DefaultOpen |
                    ImGuiTreeNodeFlags_SpanAvailWidth);

                if (node_open) {
                    for (auto& elem : layer.elements)
                        draw_element_row(app, elem, pi);

                    if (layer.elements.empty()) {
                        ImGui::TextDisabled("  (empty layer)");
                    }
                    ImGui::TreePop();
                }
                ImGui::PopID();
            }

            if (page.layers.empty())
                ImGui::TextDisabled("No layers");


            ImGui::Separator();
            if (ImGui::SmallButton("+ Layer")) {
                const auto n = page.layers.size() + 1;
                page.layers.push_back(doc::Layer{
                    .id   = "layer-" + std::to_string(n),
                    .name = "Layer "  + std::to_string(n),
                });
                app.unsaved_changes = true;
            }

            ImGui::EndTabItem();
        }

        //  Add page button
        if (ImGui::TabItemButton("+", ImGuiTabItemFlags_Trailing)) {
            doc.add_page();
            doc.active_page_idx = static_cast<int>(doc.pages.size()) - 1;
            app.unsaved_changes = true;
        }

        ImGui::EndTabBar();
    }

    ImGui::End();
}

} // namespace shadebug::panels

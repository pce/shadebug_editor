#include "properties_panel.hpp"
#include "../app.hpp"
#include "imgui.h"
#include <algorithm>

namespace shadebug::panels {

namespace {

void draw_style_editor(doc::Style& s, bool& changed) {
    ImGui::SeparatorText("Fill");
    changed |= ImGui::ColorEdit4("##fill",   &s.fill.r);

    ImGui::SeparatorText("Stroke");
    changed |= ImGui::ColorEdit4("##stroke", &s.stroke.r);
    ImGui::SetNextItemWidth(-FLT_MIN);
    changed |= ImGui::DragFloat("Width##sw", &s.stroke_width, 0.1f, 0.f, 50.f, "%.1f px");

    ImGui::SeparatorText("Appearance");
    ImGui::SetNextItemWidth(-FLT_MIN);
    changed |= ImGui::SliderFloat("Opacity##op", &s.opacity, 0.f, 1.f, "%.2f");
    ImGui::SetNextItemWidth(-FLT_MIN);
    changed |= ImGui::DragFloat("Corner##cr", &s.corner_radius, 0.5f, 0.f, 200.f, "%.1f mm");
}

void draw_bounds_editor(doc::Rect& bounds, float page_w, float page_h, bool& changed) {
    ImGui::SeparatorText("Position");
    ImGui::SetNextItemWidth(-FLT_MIN);
    changed |= ImGui::DragFloat2("##pos",  &bounds.pos.x,  0.5f, 0.f,
                                 std::max(page_w, page_h), "%.1f mm");

    ImGui::SeparatorText("Size");
    ImGui::SetNextItemWidth(-FLT_MIN);
    changed |= ImGui::DragFloat2("##size", &bounds.size.x, 0.5f, 1.f,
                                 std::max(page_w, page_h), "%.1f mm");
    bounds.size.x = std::max(bounds.size.x, 1.f);
    bounds.size.y = std::max(bounds.size.y, 1.f);
}

void draw_element_properties(shadebug::App& app, doc::Element& elem, const doc::Page& page) {
    // Name
    ImGui::SeparatorText("Element");
    char name_buf[256];
    std::strncpy(name_buf, elem.name.c_str(), sizeof(name_buf) - 1);
    name_buf[sizeof(name_buf) - 1] = '\0';
    ImGui::SetNextItemWidth(-FLT_MIN);
    if (ImGui::InputText("##name", name_buf, sizeof(name_buf))) {
        elem.name = name_buf;
        app.unsaved_changes = true;
    }

    // Kind label
    constexpr std::array kinds = {"Shape", "Text", "Image", "SVG", "Table"};
    const auto kidx = static_cast<int>(elem.kind);
    ImGui::TextDisabled("Kind: %s", kidx < static_cast<int>(kinds.size())
                                    ? kinds[static_cast<std::size_t>(kidx)] : "?");

    bool changed = false;

    draw_bounds_editor(elem.bounds, page.width_mm, page.height_mm, changed);
    draw_style_editor(elem.style, changed);

    if (elem.kind == doc::BlockKind::Text) {
        ImGui::SeparatorText("Text");
        char buf[2048];
        std::strncpy(buf, elem.content.c_str(), sizeof(buf) - 1);
        buf[sizeof(buf) - 1] = '\0';
        ImGui::SetNextItemWidth(-FLT_MIN);
        if (ImGui::InputTextMultiline("##content", buf, sizeof(buf),
                                      ImVec2(-FLT_MIN, 80.f))) {
            elem.content = buf;
            changed = true;
        }
    }

    // Visibility / lock
    ImGui::SeparatorText("Flags");
    changed |= ImGui::Checkbox("Visible", &elem.visible);
    ImGui::SameLine();
    changed |= ImGui::Checkbox("Locked",  &elem.locked);

    if (changed)
        app.unsaved_changes = true;
}

void draw_page_properties(shadebug::App& app, doc::Page& page) {
    ImGui::SeparatorText("Page");
    char name_buf[256];
    std::strncpy(name_buf, page.name.c_str(), sizeof(name_buf) - 1);
    name_buf[sizeof(name_buf) - 1] = '\0';
    ImGui::SetNextItemWidth(-FLT_MIN);
    if (ImGui::InputText("##pname", name_buf, sizeof(name_buf))) {
        page.name = name_buf;
        app.unsaved_changes = true;
    }

    ImGui::SeparatorText("Size");
    ImGui::SetNextItemWidth(-FLT_MIN);
    if (ImGui::DragFloat("Width mm##pw", &page.width_mm, 0.5f, 50.f, 5000.f, "%.0f mm"))
        app.unsaved_changes = true;
    ImGui::SetNextItemWidth(-FLT_MIN);
    if (ImGui::DragFloat("Height mm##ph", &page.height_mm, 0.5f, 50.f, 5000.f, "%.0f mm"))
        app.unsaved_changes = true;

    // Preset buttons
    ImGui::SeparatorText("Presets");
    if (ImGui::SmallButton("A4 Portrait"))  { page.width_mm = 210; page.height_mm = 297; app.unsaved_changes = true; }
    ImGui::SameLine();
    if (ImGui::SmallButton("A4 Landscape")) { page.width_mm = 297; page.height_mm = 210; app.unsaved_changes = true; }
    if (ImGui::SmallButton("Letter"))       { page.width_mm = 216; page.height_mm = 279; app.unsaved_changes = true; }
    ImGui::SameLine();
    if (ImGui::SmallButton("Square"))       { page.width_mm = 210; page.height_mm = 210; app.unsaved_changes = true; }
}

} // namespace

void draw_properties_panel(shadebug::App& app) {
    if (!app.panels.show_properties) return;

    ImGui::Begin("Properties", &app.panels.show_properties);

    auto* page = app.document.active_page();

    if (page && app.selection.has_element()) {
        // Find the selected element across all layers
        doc::Element* found = nullptr;
        for (auto& layer : page->layers) {
            for (auto& elem : layer.elements) {
                if (elem.id == *app.selection.element_id) {
                    found = &elem;
                    break;
                }
            }
            if (found) break;
        }
        if (found)
            draw_element_properties(app, *found, *page);
        else {
            app.selection.clear();
            ImGui::TextDisabled("Selection lost");
        }
    } else if (page) {
        draw_page_properties(app, *page);
    } else {
        ImGui::TextDisabled("No document open");
    }

    ImGui::End();
}

} // namespace shadebug::panels

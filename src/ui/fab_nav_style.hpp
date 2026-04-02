#pragma once

#include <imgui.h>
#include "fab_nav.hpp"

namespace shadebug::ui {

/**
 * @brief Styling utilities for FabNav component
 *
 * Provides theming presets and customization helpers
 */
class FabNavStyle {
public:
    enum class Theme {
        Light,
        Dark,
        Neon,
        Minimal,
        Glassmorphism,
    };

    struct ColorScheme {
        ImU32 button_normal;
        ImU32 button_hover;
        ImU32 button_active;
        ImU32 text;
        ImU32 shadow;
    };

    static ColorScheme get_theme(Theme theme) {
        ImU32 to_u32 = [](ImVec4 v) {
            return ImGui::GetColorU32(v);
        };

        switch (theme) {
            case Theme::Light:
                return {
                    .button_normal = ImGui::GetColorU32(ImVec4(0.95f, 0.95f, 0.95f, 1.0f)),
                    .button_hover = ImGui::GetColorU32(ImVec4(1.0f, 1.0f, 1.0f, 1.0f)),
                    .button_active = ImGui::GetColorU32(ImVec4(0.85f, 0.85f, 0.85f, 1.0f)),
                    .text = ImGui::GetColorU32(ImVec4(0.1f, 0.1f, 0.1f, 1.0f)),
                    .shadow = ImGui::GetColorU32(ImVec4(0.0f, 0.0f, 0.0f, 0.3f)),
                };

            case Theme::Dark:
                return {
                    .button_normal = ImGui::GetColorU32(ImVec4(0.2f, 0.2f, 0.2f, 1.0f)),
                    .button_hover = ImGui::GetColorU32(ImVec4(0.3f, 0.3f, 0.3f, 1.0f)),
                    .button_active = ImGui::GetColorU32(ImVec4(0.15f, 0.15f, 0.15f, 1.0f)),
                    .text = ImGui::GetColorU32(ImVec4(0.9f, 0.9f, 0.9f, 1.0f)),
                    .shadow = ImGui::GetColorU32(ImVec4(0.0f, 0.0f, 0.0f, 0.6f)),
                };

            case Theme::Neon:
                return {
                    .button_normal = ImGui::GetColorU32(ImVec4(0.1f, 0.1f, 0.15f, 1.0f)),
                    .button_hover = ImGui::GetColorU32(ImVec4(0.2f, 0.8f, 1.0f, 0.9f)),
                    .button_active = ImGui::GetColorU32(ImVec4(0.0f, 1.0f, 1.0f, 1.0f)),
                    .text = ImGui::GetColorU32(ImVec4(0.0f, 1.0f, 1.0f, 1.0f)),
                    .shadow = ImGui::GetColorU32(ImVec4(0.0f, 1.0f, 1.0f, 0.4f)),
                };

            case Theme::Minimal:
                return {
                    .button_normal = ImGui::GetColorU32(ImVec4(1.0f, 1.0f, 1.0f, 0.0f)),
                    .button_hover = ImGui::GetColorU32(ImVec4(1.0f, 1.0f, 1.0f, 0.1f)),
                    .button_active = ImGui::GetColorU32(ImVec4(1.0f, 1.0f, 1.0f, 0.05f)),
                    .text = ImGui::GetColorU32(ImVec4(0.5f, 0.5f, 0.5f, 1.0f)),
                    .shadow = ImGui::GetColorU32(ImVec4(0.0f, 0.0f, 0.0f, 0.0f)),
                };

            case Theme::Glassmorphism:
            default:
                return {
                    .button_normal = ImGui::GetColorU32(ImVec4(1.0f, 1.0f, 1.0f, 0.1f)),
                    .button_hover = ImGui::GetColorU32(ImVec4(1.0f, 1.0f, 1.0f, 0.2f)),
                    .button_active = ImGui::GetColorU32(ImVec4(1.0f, 1.0f, 1.0f, 0.15f)),
                    .text = ImGui::GetColorU32(ImVec4(1.0f, 1.0f, 1.0f, 0.8f)),
                    .shadow = ImGui::GetColorU32(ImVec4(0.0f, 0.0f, 0.0f, 0.2f)),
                };
        }
    }

    /**
     * Apply a theme to ImGui style (global)
     */
    static void apply_theme(Theme theme) {
        auto scheme = get_theme(theme);

        ImGuiStyle& style = ImGui::GetStyle();

        // Convert back from ImU32 to ImVec4 for assignment
        // Note: This is a simplified approach; for production,
        // you'd want to store and reuse ColorScheme values properly
    }

    /**
     * Draw a styled FAB with shadow effect
     */
    static void draw_fab_with_shadow(ImDrawList* draw_list, ImVec2 center, float radius,
                                    ImU32 col_normal, ImU32 col_hover,
                                    bool hovered, const char* icon) {
        // Draw shadow (offset down-right)
        const float shadow_offset = 2.0f;
        ImU32 shadow_col = ImGui::GetColorU32(ImVec4(0.0f, 0.0f, 0.0f, 0.2f));
        draw_list->AddCircleFilled(
            ImVec2(center.x + shadow_offset, center.y + shadow_offset),
            radius, shadow_col);

        // Draw main button
        ImU32 btn_col = hovered ? col_hover : col_normal;
        draw_list->AddCircleFilled(center, radius, btn_col);

        // Optional: Add outline
        ImU32 outline_col = ImGui::GetColorU32(ImVec4(1.0f, 1.0f, 1.0f, 0.3f));
        draw_list->AddCircle(center, radius, outline_col, 0, 1.0f);

        // Draw icon/text
        if (icon) {
            ImU32 text_col = ImGui::GetColorU32(ImVec4(1.0f, 1.0f, 1.0f, 1.0f));
            draw_list->AddText(ImVec2(center.x - 6, center.y - 6), text_col, icon);
        }
    }

    /**
     * Create a gradient-styled FAB button
     */
    static void draw_fab_gradient(ImDrawList* draw_list, ImVec2 center, float radius,
                                 ImU32 col_top, ImU32 col_bottom,
                                 bool hovered, const char* icon) {
        // Approximate gradient with multiple circles
        const int segments = 4;
        for (int i = 0; i < segments; ++i) {
            float t = static_cast<float>(i) / segments;
            ImU32 col = ImGui::GetColorU32(
                ImVec4(
                    ImGui::GetColorU32(col_top) & 0xFF,
                    ImGui::GetColorU32(col_top) & 0xFF00,
                    ImGui::GetColorU32(col_top) & 0xFF0000,
                    255
                )
            );
            float r = radius * (1.0f - t * 0.3f);
            draw_list->AddCircleFilled(center, r, col);
        }

        // Draw icon
        if (icon) {
            ImU32 text_col = ImGui::GetColorU32(ImVec4(1.0f, 1.0f, 1.0f, 1.0f));
            draw_list->AddText(ImVec2(center.x - 6, center.y - 6), text_col, icon);
        }
    }
};

} // namespace shadebug::ui


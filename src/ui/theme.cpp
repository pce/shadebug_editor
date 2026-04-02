#include "theme.hpp"

namespace shadebug::ui {

void Theme::ApplyImGuiStyle(float dpi_scale, const StyleParams& p) noexcept
{
    const float s = (dpi_scale > 0.f) ? dpi_scale : 1.f;

    ImGuiStyle& st = ImGui::GetStyle();

    const float r = p.rounding * s;
    st.WindowRounding    = r;
    st.FrameRounding     = r;
    st.ChildRounding     = r;
    st.PopupRounding     = r;
    st.TabRounding       = r;
    st.ScrollbarRounding = r;
    st.GrabRounding      = r;
    st.ScrollbarSize     = 14.f * s;
    st.GrabMinSize       = 10.f * s;

    st.WindowPadding    = ImVec2(10.f * s,          10.f * s);
    st.FramePadding     = ImVec2(p.frame_padding * s, 4.f * s);
    st.ItemSpacing      = ImVec2(p.item_spacing  * s, 4.f * s);
    st.ItemInnerSpacing = ImVec2( 4.f * s,            4.f * s);
    st.CellPadding      = ImVec2( 4.f * s,            2.f * s);
    st.IndentSpacing    = 21.f * s;

    ImGui::GetIO().FontGlobalScale = p.font_scale;
}

void Theme::ApplyColorTheme(ThemeType theme) noexcept
{
    ImVec4* c = ImGui::GetStyle().Colors;

    switch (theme) {
    case ThemeType::SolarizedDark: {
        c[ImGuiCol_Text]                 = ImVec4(0.93f, 0.91f, 0.84f, 1.00f);
        c[ImGuiCol_TextDisabled]         = ImVec4(0.40f, 0.55f, 0.56f, 1.00f);
        c[ImGuiCol_WindowBg]             = ImVec4(0.00f, 0.17f, 0.21f, 1.00f);
        c[ImGuiCol_ChildBg]              = ImVec4(0.02f, 0.22f, 0.25f, 0.95f);
        c[ImGuiCol_PopupBg]              = ImVec4(0.00f, 0.15f, 0.19f, 0.98f);
        c[ImGuiCol_Border]               = ImVec4(0.08f, 0.30f, 0.35f, 1.00f);
        c[ImGuiCol_BorderShadow]         = ImVec4(0.00f, 0.00f, 0.00f, 0.00f);
        c[ImGuiCol_FrameBg]              = ImVec4(0.02f, 0.25f, 0.30f, 1.00f);
        c[ImGuiCol_FrameBgHovered]       = ImVec4(0.18f, 0.38f, 0.38f, 1.00f);
        c[ImGuiCol_FrameBgActive]        = ImVec4(0.28f, 0.54f, 0.55f, 1.00f);
        c[ImGuiCol_TitleBg]              = ImVec4(0.00f, 0.15f, 0.18f, 1.00f);
        c[ImGuiCol_TitleBgActive]        = ImVec4(0.00f, 0.20f, 0.23f, 1.00f);
        c[ImGuiCol_TitleBgCollapsed]     = ImVec4(0.00f, 0.12f, 0.15f, 1.00f);
        c[ImGuiCol_MenuBarBg]            = ImVec4(0.00f, 0.14f, 0.17f, 1.00f);
        c[ImGuiCol_ScrollbarBg]          = ImVec4(0.00f, 0.14f, 0.17f, 1.00f);
        c[ImGuiCol_ScrollbarGrab]        = ImVec4(0.10f, 0.30f, 0.35f, 1.00f);
        c[ImGuiCol_ScrollbarGrabHovered] = ImVec4(0.18f, 0.40f, 0.45f, 1.00f);
        c[ImGuiCol_ScrollbarGrabActive]  = ImVec4(0.25f, 0.52f, 0.57f, 1.00f);
        c[ImGuiCol_CheckMark]            = ImVec4(0.42f, 0.80f, 0.82f, 1.00f);
        c[ImGuiCol_SliderGrab]           = ImVec4(0.25f, 0.55f, 0.65f, 1.00f);
        c[ImGuiCol_SliderGrabActive]     = ImVec4(0.35f, 0.68f, 0.78f, 1.00f);
        c[ImGuiCol_Button]               = ImVec4(0.11f, 0.28f, 0.32f, 1.00f);
        c[ImGuiCol_ButtonHovered]        = ImVec4(0.15f, 0.38f, 0.40f, 1.00f);
        c[ImGuiCol_ButtonActive]         = ImVec4(0.18f, 0.45f, 0.48f, 1.00f);
        c[ImGuiCol_Header]               = c[ImGuiCol_Button];
        c[ImGuiCol_HeaderHovered]        = c[ImGuiCol_ButtonHovered];
        c[ImGuiCol_HeaderActive]         = c[ImGuiCol_ButtonActive];
        c[ImGuiCol_Separator]            = ImVec4(0.08f, 0.30f, 0.35f, 1.00f);
        c[ImGuiCol_SeparatorHovered]     = ImVec4(0.18f, 0.42f, 0.48f, 1.00f);
        c[ImGuiCol_SeparatorActive]      = ImVec4(0.28f, 0.55f, 0.60f, 1.00f);
        c[ImGuiCol_ResizeGrip]           = ImVec4(0.10f, 0.28f, 0.32f, 0.50f);
        c[ImGuiCol_ResizeGripHovered]    = ImVec4(0.20f, 0.42f, 0.46f, 0.80f);
        c[ImGuiCol_ResizeGripActive]     = ImVec4(0.30f, 0.55f, 0.60f, 1.00f);
        c[ImGuiCol_Tab]                  = c[ImGuiCol_Button];
        c[ImGuiCol_TabHovered]           = c[ImGuiCol_ButtonHovered];
        c[ImGuiCol_TabSelected]          = c[ImGuiCol_ButtonActive];
        c[ImGuiCol_TableHeaderBg]        = ImVec4(0.00f, 0.18f, 0.22f, 1.00f);
        c[ImGuiCol_TableBorderStrong]    = ImVec4(0.08f, 0.30f, 0.35f, 1.00f);
        c[ImGuiCol_TableBorderLight]     = ImVec4(0.05f, 0.22f, 0.26f, 1.00f);
        c[ImGuiCol_TableRowBg]           = ImVec4(0.00f, 0.00f, 0.00f, 0.00f);
        c[ImGuiCol_TableRowBgAlt]        = ImVec4(1.00f, 1.00f, 1.00f, 0.03f);
        break;
    }
    case ThemeType::SolarizedLight: {
        c[ImGuiCol_Text]                 = ImVec4(0.00f, 0.17f, 0.21f, 1.00f);
        c[ImGuiCol_TextDisabled]         = ImVec4(0.45f, 0.55f, 0.57f, 1.00f);
        c[ImGuiCol_WindowBg]             = ImVec4(0.99f, 0.96f, 0.89f, 1.00f);
        c[ImGuiCol_ChildBg]              = ImVec4(0.94f, 0.91f, 0.85f, 1.00f);
        c[ImGuiCol_PopupBg]              = ImVec4(0.99f, 0.97f, 0.92f, 0.98f);
        c[ImGuiCol_Border]               = ImVec4(0.72f, 0.72f, 0.68f, 1.00f);
        c[ImGuiCol_BorderShadow]         = ImVec4(0.00f, 0.00f, 0.00f, 0.00f);
        c[ImGuiCol_FrameBg]              = ImVec4(0.87f, 0.84f, 0.78f, 1.00f);
        c[ImGuiCol_FrameBgHovered]       = ImVec4(0.72f, 0.76f, 0.76f, 1.00f);
        c[ImGuiCol_FrameBgActive]        = ImVec4(0.60f, 0.60f, 0.58f, 1.00f);
        c[ImGuiCol_TitleBg]              = ImVec4(0.89f, 0.87f, 0.83f, 1.00f);
        c[ImGuiCol_TitleBgActive]        = ImVec4(0.78f, 0.76f, 0.70f, 1.00f);
        c[ImGuiCol_TitleBgCollapsed]     = ImVec4(0.92f, 0.90f, 0.86f, 1.00f);
        c[ImGuiCol_MenuBarBg]            = ImVec4(0.91f, 0.89f, 0.84f, 1.00f);
        c[ImGuiCol_ScrollbarBg]          = ImVec4(0.91f, 0.89f, 0.84f, 1.00f);
        c[ImGuiCol_ScrollbarGrab]        = ImVec4(0.70f, 0.68f, 0.63f, 1.00f);
        c[ImGuiCol_ScrollbarGrabHovered] = ImVec4(0.60f, 0.58f, 0.54f, 1.00f);
        c[ImGuiCol_ScrollbarGrabActive]  = ImVec4(0.50f, 0.48f, 0.44f, 1.00f);
        c[ImGuiCol_CheckMark]            = ImVec4(0.20f, 0.52f, 0.53f, 1.00f);
        c[ImGuiCol_SliderGrab]           = ImVec4(0.35f, 0.60f, 0.62f, 1.00f);
        c[ImGuiCol_SliderGrabActive]     = ImVec4(0.20f, 0.48f, 0.50f, 1.00f);
        c[ImGuiCol_Button]               = ImVec4(0.75f, 0.65f, 0.53f, 1.00f);
        c[ImGuiCol_ButtonHovered]        = ImVec4(0.83f, 0.73f, 0.62f, 1.00f);
        c[ImGuiCol_ButtonActive]         = ImVec4(0.93f, 0.80f, 0.60f, 1.00f);
        c[ImGuiCol_Header]               = c[ImGuiCol_Button];
        c[ImGuiCol_HeaderHovered]        = c[ImGuiCol_ButtonHovered];
        c[ImGuiCol_HeaderActive]         = c[ImGuiCol_ButtonActive];
        c[ImGuiCol_Separator]            = ImVec4(0.72f, 0.72f, 0.68f, 1.00f);
        c[ImGuiCol_SeparatorHovered]     = ImVec4(0.55f, 0.55f, 0.50f, 1.00f);
        c[ImGuiCol_SeparatorActive]      = ImVec4(0.40f, 0.40f, 0.36f, 1.00f);
        c[ImGuiCol_ResizeGrip]           = ImVec4(0.70f, 0.65f, 0.53f, 0.50f);
        c[ImGuiCol_ResizeGripHovered]    = ImVec4(0.60f, 0.55f, 0.45f, 0.80f);
        c[ImGuiCol_ResizeGripActive]     = ImVec4(0.50f, 0.45f, 0.36f, 1.00f);
        c[ImGuiCol_Tab]                  = c[ImGuiCol_Button];
        c[ImGuiCol_TabHovered]           = c[ImGuiCol_ButtonHovered];
        c[ImGuiCol_TabSelected]          = c[ImGuiCol_ButtonActive];
        c[ImGuiCol_TableHeaderBg]        = ImVec4(0.84f, 0.82f, 0.77f, 1.00f);
        c[ImGuiCol_TableBorderStrong]    = ImVec4(0.68f, 0.66f, 0.60f, 1.00f);
        c[ImGuiCol_TableBorderLight]     = ImVec4(0.78f, 0.76f, 0.70f, 1.00f);
        c[ImGuiCol_TableRowBg]           = ImVec4(0.00f, 0.00f, 0.00f, 0.00f);
        c[ImGuiCol_TableRowBgAlt]        = ImVec4(0.00f, 0.00f, 0.00f, 0.04f);
        break;
    }
    case ThemeType::Monokai: {
        c[ImGuiCol_Text]                 = ImVec4(0.90f, 0.90f, 0.90f, 1.00f);
        c[ImGuiCol_TextDisabled]         = ImVec4(0.50f, 0.50f, 0.50f, 1.00f);
        c[ImGuiCol_WindowBg]             = ImVec4(0.10f, 0.10f, 0.10f, 1.00f);
        c[ImGuiCol_ChildBg]              = ImVec4(0.13f, 0.13f, 0.13f, 1.00f);
        c[ImGuiCol_PopupBg]              = ImVec4(0.08f, 0.08f, 0.08f, 0.98f);
        c[ImGuiCol_Border]               = ImVec4(0.22f, 0.22f, 0.22f, 1.00f);
        c[ImGuiCol_BorderShadow]         = ImVec4(0.00f, 0.00f, 0.00f, 0.00f);
        c[ImGuiCol_FrameBg]              = ImVec4(0.16f, 0.16f, 0.16f, 1.00f);
        c[ImGuiCol_FrameBgHovered]       = ImVec4(0.22f, 0.22f, 0.22f, 1.00f);
        c[ImGuiCol_FrameBgActive]        = ImVec4(0.28f, 0.28f, 0.28f, 1.00f);
        c[ImGuiCol_TitleBg]              = ImVec4(0.08f, 0.08f, 0.08f, 1.00f);
        c[ImGuiCol_TitleBgActive]        = ImVec4(0.12f, 0.12f, 0.12f, 1.00f);
        c[ImGuiCol_TitleBgCollapsed]     = ImVec4(0.06f, 0.06f, 0.06f, 1.00f);
        c[ImGuiCol_MenuBarBg]            = ImVec4(0.09f, 0.09f, 0.09f, 1.00f);
        c[ImGuiCol_ScrollbarBg]          = ImVec4(0.08f, 0.08f, 0.08f, 1.00f);
        c[ImGuiCol_ScrollbarGrab]        = ImVec4(0.24f, 0.24f, 0.24f, 1.00f);
        c[ImGuiCol_ScrollbarGrabHovered] = ImVec4(0.32f, 0.32f, 0.32f, 1.00f);
        c[ImGuiCol_ScrollbarGrabActive]  = ImVec4(0.40f, 0.40f, 0.40f, 1.00f);
        c[ImGuiCol_CheckMark]            = ImVec4(0.64f, 0.87f, 0.29f, 1.00f);
        c[ImGuiCol_SliderGrab]           = ImVec4(0.44f, 0.33f, 0.53f, 1.00f);
        c[ImGuiCol_SliderGrabActive]     = ImVec4(0.56f, 0.44f, 0.66f, 1.00f);
        c[ImGuiCol_Button]               = ImVec4(0.44f, 0.33f, 0.53f, 1.00f);
        c[ImGuiCol_ButtonHovered]        = ImVec4(0.54f, 0.43f, 0.63f, 1.00f);
        c[ImGuiCol_ButtonActive]         = ImVec4(0.64f, 0.53f, 0.73f, 1.00f);
        c[ImGuiCol_Header]               = ImVec4(0.27f, 0.36f, 0.43f, 1.00f);
        c[ImGuiCol_HeaderHovered]        = ImVec4(0.34f, 0.44f, 0.52f, 1.00f);
        c[ImGuiCol_HeaderActive]         = ImVec4(0.42f, 0.53f, 0.62f, 1.00f);
        c[ImGuiCol_Separator]            = ImVec4(0.22f, 0.22f, 0.22f, 1.00f);
        c[ImGuiCol_SeparatorHovered]     = ImVec4(0.34f, 0.44f, 0.52f, 1.00f);
        c[ImGuiCol_SeparatorActive]      = ImVec4(0.42f, 0.53f, 0.62f, 1.00f);
        c[ImGuiCol_ResizeGrip]           = ImVec4(0.44f, 0.33f, 0.53f, 0.40f);
        c[ImGuiCol_ResizeGripHovered]    = ImVec4(0.54f, 0.43f, 0.63f, 0.70f);
        c[ImGuiCol_ResizeGripActive]     = ImVec4(0.64f, 0.53f, 0.73f, 1.00f);
        c[ImGuiCol_Tab]                  = c[ImGuiCol_Button];
        c[ImGuiCol_TabHovered]           = c[ImGuiCol_ButtonHovered];
        c[ImGuiCol_TabSelected]          = c[ImGuiCol_ButtonActive];
        c[ImGuiCol_TableHeaderBg]        = ImVec4(0.14f, 0.14f, 0.14f, 1.00f);
        c[ImGuiCol_TableBorderStrong]    = ImVec4(0.22f, 0.22f, 0.22f, 1.00f);
        c[ImGuiCol_TableBorderLight]     = ImVec4(0.18f, 0.18f, 0.18f, 1.00f);
        c[ImGuiCol_TableRowBg]           = ImVec4(0.00f, 0.00f, 0.00f, 0.00f);
        c[ImGuiCol_TableRowBgAlt]        = ImVec4(1.00f, 1.00f, 1.00f, 0.03f);
        break;
    }
    case ThemeType::ArcDark: {
        // Arc Dark — cool blue-grey palette with clear accent #5294E2
        c[ImGuiCol_Text]                 = ImVec4(0.88f, 0.89f, 0.91f, 1.00f);
        c[ImGuiCol_TextDisabled]         = ImVec4(0.50f, 0.52f, 0.56f, 1.00f);
        c[ImGuiCol_WindowBg]             = ImVec4(0.17f, 0.18f, 0.22f, 1.00f);
        c[ImGuiCol_ChildBg]              = ImVec4(0.20f, 0.21f, 0.25f, 1.00f);
        c[ImGuiCol_PopupBg]              = ImVec4(0.15f, 0.16f, 0.19f, 0.98f);
        c[ImGuiCol_Border]               = ImVec4(0.25f, 0.26f, 0.31f, 1.00f);
        c[ImGuiCol_BorderShadow]         = ImVec4(0.00f, 0.00f, 0.00f, 0.00f);
        c[ImGuiCol_FrameBg]              = ImVec4(0.22f, 0.23f, 0.28f, 1.00f);
        c[ImGuiCol_FrameBgHovered]       = ImVec4(0.27f, 0.28f, 0.34f, 1.00f);
        c[ImGuiCol_FrameBgActive]        = ImVec4(0.32f, 0.58f, 0.89f, 0.30f);
        c[ImGuiCol_TitleBg]              = ImVec4(0.14f, 0.15f, 0.18f, 1.00f);
        c[ImGuiCol_TitleBgActive]        = ImVec4(0.17f, 0.18f, 0.22f, 1.00f);
        c[ImGuiCol_TitleBgCollapsed]     = ImVec4(0.12f, 0.13f, 0.16f, 1.00f);
        c[ImGuiCol_MenuBarBg]            = ImVec4(0.14f, 0.15f, 0.18f, 1.00f);
        c[ImGuiCol_ScrollbarBg]          = ImVec4(0.14f, 0.15f, 0.18f, 1.00f);
        c[ImGuiCol_ScrollbarGrab]        = ImVec4(0.28f, 0.30f, 0.36f, 1.00f);
        c[ImGuiCol_ScrollbarGrabHovered] = ImVec4(0.35f, 0.37f, 0.44f, 1.00f);
        c[ImGuiCol_ScrollbarGrabActive]  = ImVec4(0.32f, 0.58f, 0.89f, 1.00f);
        c[ImGuiCol_CheckMark]            = ImVec4(0.32f, 0.58f, 0.89f, 1.00f);
        c[ImGuiCol_SliderGrab]           = ImVec4(0.32f, 0.58f, 0.89f, 0.80f);
        c[ImGuiCol_SliderGrabActive]     = ImVec4(0.32f, 0.58f, 0.89f, 1.00f);
        c[ImGuiCol_Button]               = ImVec4(0.25f, 0.26f, 0.32f, 1.00f);
        c[ImGuiCol_ButtonHovered]        = ImVec4(0.32f, 0.58f, 0.89f, 0.80f);
        c[ImGuiCol_ButtonActive]         = ImVec4(0.32f, 0.58f, 0.89f, 1.00f);
        c[ImGuiCol_Header]               = ImVec4(0.32f, 0.58f, 0.89f, 0.25f);
        c[ImGuiCol_HeaderHovered]        = ImVec4(0.32f, 0.58f, 0.89f, 0.50f);
        c[ImGuiCol_HeaderActive]         = ImVec4(0.32f, 0.58f, 0.89f, 0.80f);
        c[ImGuiCol_Separator]            = ImVec4(0.25f, 0.26f, 0.31f, 1.00f);
        c[ImGuiCol_SeparatorHovered]     = ImVec4(0.32f, 0.58f, 0.89f, 0.60f);
        c[ImGuiCol_SeparatorActive]      = ImVec4(0.32f, 0.58f, 0.89f, 1.00f);
        c[ImGuiCol_ResizeGrip]           = ImVec4(0.32f, 0.58f, 0.89f, 0.20f);
        c[ImGuiCol_ResizeGripHovered]    = ImVec4(0.32f, 0.58f, 0.89f, 0.60f);
        c[ImGuiCol_ResizeGripActive]     = ImVec4(0.32f, 0.58f, 0.89f, 1.00f);
        c[ImGuiCol_Tab]                  = c[ImGuiCol_Button];
        c[ImGuiCol_TabHovered]           = c[ImGuiCol_ButtonHovered];
        c[ImGuiCol_TabSelected]          = c[ImGuiCol_ButtonActive];
        c[ImGuiCol_TableHeaderBg]        = ImVec4(0.19f, 0.20f, 0.24f, 1.00f);
        c[ImGuiCol_TableBorderStrong]    = ImVec4(0.25f, 0.26f, 0.31f, 1.00f);
        c[ImGuiCol_TableBorderLight]     = ImVec4(0.22f, 0.23f, 0.28f, 1.00f);
        c[ImGuiCol_TableRowBg]           = ImVec4(0.00f, 0.00f, 0.00f, 0.00f);
        c[ImGuiCol_TableRowBgAlt]        = ImVec4(1.00f, 1.00f, 1.00f, 0.03f);
        break;
    }
    case ThemeType::OneDark: {
        // One Dark (Atom / VS Code) — warm dark with vivid accents
        c[ImGuiCol_Text]                 = ImVec4(0.83f, 0.86f, 0.90f, 1.00f);
        c[ImGuiCol_TextDisabled]         = ImVec4(0.37f, 0.41f, 0.48f, 1.00f);
        c[ImGuiCol_WindowBg]             = ImVec4(0.15f, 0.16f, 0.20f, 1.00f);
        c[ImGuiCol_ChildBg]              = ImVec4(0.18f, 0.19f, 0.23f, 1.00f);
        c[ImGuiCol_PopupBg]              = ImVec4(0.13f, 0.14f, 0.18f, 0.98f);
        c[ImGuiCol_Border]               = ImVec4(0.22f, 0.24f, 0.29f, 1.00f);
        c[ImGuiCol_BorderShadow]         = ImVec4(0.00f, 0.00f, 0.00f, 0.00f);
        c[ImGuiCol_FrameBg]              = ImVec4(0.20f, 0.21f, 0.26f, 1.00f);
        c[ImGuiCol_FrameBgHovered]       = ImVec4(0.26f, 0.27f, 0.33f, 1.00f);
        c[ImGuiCol_FrameBgActive]        = ImVec4(0.36f, 0.55f, 0.82f, 0.30f);
        c[ImGuiCol_TitleBg]              = ImVec4(0.12f, 0.13f, 0.16f, 1.00f);
        c[ImGuiCol_TitleBgActive]        = ImVec4(0.15f, 0.16f, 0.20f, 1.00f);
        c[ImGuiCol_TitleBgCollapsed]     = ImVec4(0.10f, 0.11f, 0.14f, 1.00f);
        c[ImGuiCol_MenuBarBg]            = ImVec4(0.12f, 0.13f, 0.16f, 1.00f);
        c[ImGuiCol_ScrollbarBg]          = ImVec4(0.12f, 0.13f, 0.16f, 1.00f);
        c[ImGuiCol_ScrollbarGrab]        = ImVec4(0.26f, 0.28f, 0.34f, 1.00f);
        c[ImGuiCol_ScrollbarGrabHovered] = ImVec4(0.33f, 0.35f, 0.43f, 1.00f);
        c[ImGuiCol_ScrollbarGrabActive]  = ImVec4(0.36f, 0.55f, 0.82f, 1.00f);
        c[ImGuiCol_CheckMark]            = ImVec4(0.36f, 0.55f, 0.82f, 1.00f);
        c[ImGuiCol_SliderGrab]           = ImVec4(0.36f, 0.55f, 0.82f, 0.80f);
        c[ImGuiCol_SliderGrabActive]     = ImVec4(0.36f, 0.55f, 0.82f, 1.00f);
        // Green accent for buttons (Atom-style)
        c[ImGuiCol_Button]               = ImVec4(0.25f, 0.41f, 0.33f, 1.00f);
        c[ImGuiCol_ButtonHovered]        = ImVec4(0.29f, 0.53f, 0.40f, 1.00f);
        c[ImGuiCol_ButtonActive]         = ImVec4(0.36f, 0.65f, 0.49f, 1.00f);
        c[ImGuiCol_Header]               = ImVec4(0.36f, 0.55f, 0.82f, 0.22f);
        c[ImGuiCol_HeaderHovered]        = ImVec4(0.36f, 0.55f, 0.82f, 0.45f);
        c[ImGuiCol_HeaderActive]         = ImVec4(0.36f, 0.55f, 0.82f, 0.75f);
        c[ImGuiCol_Separator]            = ImVec4(0.22f, 0.24f, 0.29f, 1.00f);
        c[ImGuiCol_SeparatorHovered]     = ImVec4(0.36f, 0.55f, 0.82f, 0.55f);
        c[ImGuiCol_SeparatorActive]      = ImVec4(0.36f, 0.55f, 0.82f, 1.00f);
        c[ImGuiCol_ResizeGrip]           = ImVec4(0.36f, 0.55f, 0.82f, 0.20f);
        c[ImGuiCol_ResizeGripHovered]    = ImVec4(0.36f, 0.55f, 0.82f, 0.55f);
        c[ImGuiCol_ResizeGripActive]     = ImVec4(0.36f, 0.55f, 0.82f, 1.00f);
        c[ImGuiCol_Tab]                  = c[ImGuiCol_Button];
        c[ImGuiCol_TabHovered]           = c[ImGuiCol_ButtonHovered];
        c[ImGuiCol_TabSelected]          = c[ImGuiCol_ButtonActive];
        c[ImGuiCol_TableHeaderBg]        = ImVec4(0.18f, 0.19f, 0.23f, 1.00f);
        c[ImGuiCol_TableBorderStrong]    = ImVec4(0.22f, 0.24f, 0.29f, 1.00f);
        c[ImGuiCol_TableBorderLight]     = ImVec4(0.20f, 0.21f, 0.26f, 1.00f);
        c[ImGuiCol_TableRowBg]           = ImVec4(0.00f, 0.00f, 0.00f, 0.00f);
        c[ImGuiCol_TableRowBgAlt]        = ImVec4(1.00f, 1.00f, 1.00f, 0.03f);
        break;
    }
    case ThemeType::PixelDark: {
        // Pixel Dark — retro purple-black terminal palette, pairs with ProggyClean + FontAwesome.
        // Accent: vivid pink/magenta  #CF6BD4 / hover #E080E8
        c[ImGuiCol_Text]                 = ImVec4(0.92f, 0.88f, 0.98f, 1.00f);
        c[ImGuiCol_TextDisabled]         = ImVec4(0.45f, 0.40f, 0.55f, 1.00f);
        c[ImGuiCol_WindowBg]             = ImVec4(0.07f, 0.04f, 0.11f, 1.00f);
        c[ImGuiCol_ChildBg]              = ImVec4(0.09f, 0.05f, 0.14f, 1.00f);
        c[ImGuiCol_PopupBg]              = ImVec4(0.07f, 0.04f, 0.11f, 0.98f);
        c[ImGuiCol_Border]               = ImVec4(0.35f, 0.20f, 0.50f, 1.00f);
        c[ImGuiCol_BorderShadow]         = ImVec4(0.00f, 0.00f, 0.00f, 0.00f);
        c[ImGuiCol_FrameBg]              = ImVec4(0.13f, 0.08f, 0.20f, 1.00f);
        c[ImGuiCol_FrameBgHovered]       = ImVec4(0.22f, 0.13f, 0.32f, 1.00f);
        c[ImGuiCol_FrameBgActive]        = ImVec4(0.40f, 0.20f, 0.58f, 0.50f);
        c[ImGuiCol_TitleBg]              = ImVec4(0.06f, 0.03f, 0.09f, 1.00f);
        c[ImGuiCol_TitleBgActive]        = ImVec4(0.22f, 0.10f, 0.35f, 1.00f);
        c[ImGuiCol_TitleBgCollapsed]     = ImVec4(0.05f, 0.03f, 0.08f, 1.00f);
        c[ImGuiCol_MenuBarBg]            = ImVec4(0.06f, 0.03f, 0.09f, 1.00f);
        c[ImGuiCol_ScrollbarBg]          = ImVec4(0.06f, 0.03f, 0.09f, 1.00f);
        c[ImGuiCol_ScrollbarGrab]        = ImVec4(0.30f, 0.15f, 0.45f, 1.00f);
        c[ImGuiCol_ScrollbarGrabHovered] = ImVec4(0.42f, 0.22f, 0.60f, 1.00f);
        c[ImGuiCol_ScrollbarGrabActive]  = ImVec4(0.81f, 0.42f, 0.83f, 1.00f);
        c[ImGuiCol_CheckMark]            = ImVec4(0.81f, 0.42f, 0.83f, 1.00f);
        c[ImGuiCol_SliderGrab]           = ImVec4(0.60f, 0.28f, 0.72f, 1.00f);
        c[ImGuiCol_SliderGrabActive]     = ImVec4(0.81f, 0.42f, 0.83f, 1.00f);
        c[ImGuiCol_Button]               = ImVec4(0.20f, 0.10f, 0.30f, 1.00f);
        c[ImGuiCol_ButtonHovered]        = ImVec4(0.50f, 0.22f, 0.65f, 1.00f);
        c[ImGuiCol_ButtonActive]         = ImVec4(0.81f, 0.42f, 0.83f, 1.00f);
        c[ImGuiCol_Header]               = ImVec4(0.40f, 0.18f, 0.55f, 0.35f);
        c[ImGuiCol_HeaderHovered]        = ImVec4(0.55f, 0.25f, 0.70f, 0.65f);
        c[ImGuiCol_HeaderActive]         = ImVec4(0.81f, 0.42f, 0.83f, 1.00f);
        c[ImGuiCol_Separator]            = ImVec4(0.35f, 0.20f, 0.50f, 1.00f);
        c[ImGuiCol_SeparatorHovered]     = ImVec4(0.60f, 0.28f, 0.72f, 0.80f);
        c[ImGuiCol_SeparatorActive]      = ImVec4(0.81f, 0.42f, 0.83f, 1.00f);
        c[ImGuiCol_ResizeGrip]           = ImVec4(0.40f, 0.18f, 0.55f, 0.30f);
        c[ImGuiCol_ResizeGripHovered]    = ImVec4(0.60f, 0.28f, 0.72f, 0.70f);
        c[ImGuiCol_ResizeGripActive]     = ImVec4(0.81f, 0.42f, 0.83f, 1.00f);
        c[ImGuiCol_Tab]                  = c[ImGuiCol_Button];
        c[ImGuiCol_TabHovered]           = c[ImGuiCol_ButtonHovered];
        c[ImGuiCol_TabSelected]          = c[ImGuiCol_ButtonActive];
        c[ImGuiCol_TableHeaderBg]        = ImVec4(0.10f, 0.06f, 0.16f, 1.00f);
        c[ImGuiCol_TableBorderStrong]    = ImVec4(0.35f, 0.20f, 0.50f, 1.00f);
        c[ImGuiCol_TableBorderLight]     = ImVec4(0.20f, 0.12f, 0.30f, 1.00f);
        c[ImGuiCol_TableRowBg]           = ImVec4(0.00f, 0.00f, 0.00f, 0.00f);
        c[ImGuiCol_TableRowBgAlt]        = ImVec4(1.00f, 1.00f, 1.00f, 0.03f);
        break;
    }
    default: break;
    }
}


} // namespace shadebug::ui

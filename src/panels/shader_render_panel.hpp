#pragma once

#include "../renderer/draw_ctx.hpp"
#include "../renderer/scene_renderer_3d.hpp"
#include "sokol_gfx.h"
#include "imgui.h"

namespace shadebug::renderer { class GpuRenderer; }
namespace shadebug::renderer { class EffectRenderer; }

// RenderMode

enum class RenderMode : int {
    Solid    = 0,
    Wireframe,
    UV,
    Solid3D,   ///< Dedicated 3-D scene with orbit camera  ← NEW
    Count,
};
[[nodiscard]] inline const char* render_mode_name(RenderMode m) {
    switch (m) {
    case RenderMode::Solid:     return "Solid";
    case RenderMode::Wireframe: return "Wireframe";
    case RenderMode::UV:        return "UV";
    case RenderMode::Solid3D:   return "3D Scene";
    default:                    return "?";
    }
}

//  ShaderRenderPanel
//
//  Offscreen render target: runs the instanced rect pipeline into a texture,
//  then displays it as an ImGui::Image inside a panel.
//
//  Uses its own private DrawCtx — never touches the shared app draw_ctx so
//  canvas GPU rects are not clobbered.
//
//  In Solid3D mode the panel delegates to SceneRenderer3D, which owns a
//  separate colour + depth render target and a full 3-D mesh pipeline.
//

class ShaderRenderPanel {
public:
    ShaderRenderPanel()  = default;
    ~ShaderRenderPanel();

    /// Explicitly release all sokol GPU resources.
    /// Must be called before sg_shutdown() — e.g. from App::cleanup_cb().
    void destroy_render_target();

    /// draw() no longer takes an external DrawCtx — uses its own private ctx_.
    void draw(bool& visible,
              shadebug::renderer::GpuRenderer& gpu_renderer,
              shadebug::renderer::EffectRenderer& effect_renderer);

    [[nodiscard]] RenderMode mode() const noexcept { return mode_; }
    void set_mode(RenderMode m) noexcept { mode_ = m; }

private:
    shadebug::renderer::DrawCtx ctx_;              // private scene buffer — never shared

    sg_image  color_img_    = { SG_INVALID_ID };
    sg_view   color_view_   = { SG_INVALID_ID };   // color attachment view
    sg_view   sample_view_  = { SG_INVALID_ID };   // texture sampling view for ImGui
    int       rt_w_ = 0, rt_h_ = 0;

    RenderMode mode_ = RenderMode::Solid;
    float anim_time_ = 0.f;

    // 3D scene
    shadebug::renderer::SceneRenderer3D scene3d_;

    /// Orbit-camera state driven by mouse drag inside the viewport.
    float cam_azimuth_   =  0.5f;   ///< radians, horizontal
    float cam_elevation_ =  0.3f;   ///< radians, vertical (clamped ±1.4)
    float cam_zoom_      =  5.0f;   ///< eye-to-origin distance

    /// Auto-rotate the model around its Y axis.
    bool auto_rotate_ = true;

    //  Hybrid: SDF overlay on top of 3D
    //
    //  Option A implementation:
    //    1. scene3d_.render()         → 3D pass  (color+depth, CLEAR)
    //    2. render_sdf_overlay()      → SDF pass  (color-only, LOAD)
    //    Both write into the same colour image; the SDF pipeline uses
    //    GpuRenderer's offscreen_pip_ which declares no depth attachment.
    //
    bool                          show_sdf_overlay_ = false;
    shadebug::renderer::DrawCtx   overlay_ctx_;         ///< SDF HUD geometry

    void ensure_render_target(int w, int h);
    void render_scene(shadebug::renderer::GpuRenderer& gpu_renderer, int w, int h);
    void render_effect(shadebug::renderer::EffectRenderer& er, int w, int h);
    void populate_demo_scene(int w, int h);

    /// Populate overlay_ctx_ with HUD elements (status bar, indicator dots, panels).
    void populate_sdf_overlay(int w, int h);
    /// Start a color-only pass (SG_LOADACTION_LOAD) on the 3D colour attachment
    /// and flush overlay_ctx_ through the GpuRenderer offscreen pipeline.
    void render_sdf_overlay(shadebug::renderer::GpuRenderer& gpu_renderer, int w, int h);

    static void draw_fallback_pattern(ImVec2 origin, ImVec2 size);

    /// Mini XYZ-axis indicator drawn in the bottom-right corner of the 3D viewport.
    /// Replaces the need for imGuIZMO.quat for basic orientation feedback.
    static void draw_orientation_gizmo(ImDrawList* dl, ImVec2 viewport_br,
                                        float azimuth, float elevation);
};

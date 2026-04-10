#include "shader_render_panel.hpp"
#include "../renderer/gpu_renderer.hpp"
#include "../renderer/effect_renderer.hpp"
#include "../renderer/shader_registry.hpp"
#include "sokol_app.h"
#include "sokol_gfx.h"
#include "imgui.h"
#include "util/sokol_imgui.h"

#include <algorithm>
#include <cmath>
#include <format>

using shadebug::renderer::DrawCtx;
using shadebug::renderer::UiRect;


ShaderRenderPanel::~ShaderRenderPanel() {
    // Guard: destructor may run after sg_shutdown() (static App lifetime).
    if (sg_isvalid()) destroy_render_target();
}


void ShaderRenderPanel::draw(bool& visible,
                              shadebug::renderer::GpuRenderer& gpu_renderer,
                              shadebug::renderer::EffectRenderer& effect_renderer) {
    if (!visible) return;
    anim_time_ += static_cast<float>(sapp_frame_duration());

    ImGui::SetNextWindowSize(ImVec2(820, 640), ImGuiCond_FirstUseEver);
    if (!ImGui::Begin("Shader Render##render_panel", &visible,
                      ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse)) {
        ImGui::End();
        return;
    }

    // ── Toolbar ──────────────────────────────────────────────────────────────
    ImGui::AlignTextToFramePadding();
    ImGui::TextUnformatted("Mode:");
    ImGui::SameLine();

    for (int m = 0; m < static_cast<int>(RenderMode::Count); ++m) {
        const auto rm = static_cast<RenderMode>(m);
        if (ImGui::RadioButton(render_mode_name(rm), mode_ == rm))
            mode_ = rm;
        ImGui::SameLine();
    }

    // Extra controls for Solid3D mode
    if (mode_ == RenderMode::Solid3D) {
        ImGui::TextDisabled("|");
        ImGui::SameLine();
        ImGui::Checkbox("Auto-rotate", &auto_rotate_);
        ImGui::SameLine();
        ImGui::Checkbox("SDF Overlay", &show_sdf_overlay_);
        ImGui::SameLine();
    }

    // Params toggle (shown when selected shader has params)
    const auto* sel_entry = shadebug::renderer::ShaderRegistry::get().selected_entry();
    const bool  is_effect = sel_entry &&
        sel_entry->pipeline_type == shadebug::renderer::PipelineType::Effect;
    const bool  has_params = sel_entry && !sel_entry->params.empty();

    if (has_params) {
        ImGui::TextDisabled("|");
        ImGui::SameLine();
        ImGui::Checkbox("Params##prm_toggle", &show_params_);
        ImGui::SameLine();
    }

    ImGui::NewLine();
    ImGui::Separator();

    // ── Content area ─────────────────────────────────────────────────────────
    const ImVec2 avail_full = ImGui::GetContentRegionAvail();
    constexpr float kParamW = 240.f;
    const float viewport_w = (has_params && show_params_)
        ? std::max(16.f, avail_full.x - kParamW - 6.f)
        : avail_full.x;
    const ImVec2 avail = { viewport_w, avail_full.y };
    const int    vp_w  = std::max(16, static_cast<int>(avail.x));
    const int    vp_h  = std::max(16, static_cast<int>(avail.y));

    const ImVec2 cursor = ImGui::GetCursorScreenPos();

    // ── Solid3D path ─────────────────────────────────────────────────────────
    if (mode_ == RenderMode::Solid3D) {
        scene3d_.resize(vp_w, vp_h);

        if (!scene3d_.valid()) {
            draw_fallback_pattern(cursor, avail);
            ImGui::Dummy(avail);
        } else {
            const float model_angle = auto_rotate_ ? anim_time_ * 0.5f : 0.f;
            scene3d_.render(anim_time_, cam_azimuth_, cam_elevation_, cam_zoom_,
                            model_angle);

            if (show_sdf_overlay_ && gpu_renderer.valid()) {
                populate_sdf_overlay(vp_w, vp_h);
                render_sdf_overlay(gpu_renderer, vp_w, vp_h);
            }

            ImGui::Image(simgui_imtextureid(scene3d_.sample_view()), avail);

            {
                ImDrawList* dl = ImGui::GetWindowDrawList();
                const ImVec2 br { cursor.x + avail.x, cursor.y + avail.y };
                draw_orientation_gizmo(dl, br, cam_azimuth_, cam_elevation_);
            }

            if (ImGui::IsItemHovered()) {
                const auto& io = ImGui::GetIO();
                if (io.MouseDown[0]) {
                    cam_azimuth_   += io.MouseDelta.x * 0.007f;
                    cam_elevation_ += io.MouseDelta.y * 0.007f;
                    cam_elevation_  = std::clamp(cam_elevation_, -1.4f, 1.4f);
                }
                cam_zoom_ -= io.MouseWheel * 0.3f;
                cam_zoom_  = std::clamp(cam_zoom_, 1.5f, 30.f);
            }
        }

        ImGui::End();
        return;
    }

    // ── 2D / Effect path ─────────────────────────────────────────────────────

    const bool active_valid = is_effect ? effect_renderer.valid() : gpu_renderer.valid();
    if (!active_valid) {
        draw_fallback_pattern(cursor, avail);
        ImGui::Dummy(avail);
        ImGui::End();
        return;
    }

    ensure_render_target(vp_w, vp_h);

    if (color_view_.id == SG_INVALID_ID || sample_view_.id == SG_INVALID_ID) {
        draw_fallback_pattern(cursor, avail);
        ImGui::Dummy(avail);
        ImGui::End();
        return;
    }

    // Pack + upload custom params before rendering
    if (is_effect && has_params) {
        auto pu = shadebug::renderer::pack_params(
            const_cast<std::vector<shadebug::renderer::ShaderParam>&>(sel_entry->params),
            anim_time_);
        effect_renderer.set_custom_params(pu);
    }

    if (is_effect) {
        render_effect(effect_renderer, vp_w, vp_h);
    } else {
        populate_demo_scene(vp_w, vp_h);
        render_scene(gpu_renderer, vp_w, vp_h);
    }

    // ── Viewport image ───────────────────────────────────────────────────────
    ImGui::Image(simgui_imtextureid(sample_view_), avail);

    // ── Params sidebar ───────────────────────────────────────────────────────
    if (has_params && show_params_) {
        ImGui::SameLine();
        ImGui::BeginChild("##param_sidebar", ImVec2(kParamW, avail_full.y),
                          ImGuiChildFlags_Borders);
        draw_params_panel(
            const_cast<std::vector<shadebug::renderer::ShaderParam>&>(sel_entry->params),
            anim_time_);
        ImGui::EndChild();
    }

    ImGui::End();
}


void ShaderRenderPanel::populate_demo_scene(int w, int h) {
    ctx_.clear();
    const float fw = static_cast<float>(w);
    const float fh = static_cast<float>(h);

    switch (mode_) {
    case RenderMode::Solid: {
        const struct { float x,y,w,h,r,g,b,cr; } rects[] = {
            { fw*0.05f, fh*0.05f, fw*0.40f, fh*0.40f, 0.9f, 0.3f, 0.3f, 12.f },
            { fw*0.55f, fh*0.05f, fw*0.40f, fh*0.40f, 0.3f, 0.8f, 0.4f, 20.f },
            { fw*0.05f, fh*0.55f, fw*0.40f, fh*0.40f, 0.3f, 0.5f, 0.9f,  6.f },
            { fw*0.55f, fh*0.55f, fw*0.40f, fh*0.40f, 0.9f, 0.8f, 0.2f, 32.f },
        };
        for (auto& r : rects)
            ctx_.push_rect(r.x, r.y, r.w, r.h, r.r, r.g, r.b, 1.f, r.cr, 2.f,
                          0.f, 0.f, 0.f, 1.f);
        break;
    }
    case RenderMode::Wireframe: {
        const struct { float x,y,w,h,bw; } rects[] = {
            { fw*0.10f, fh*0.10f, fw*0.35f, fh*0.35f, 3.f },
            { fw*0.55f, fh*0.10f, fw*0.35f, fh*0.35f, 3.f },
            { fw*0.10f, fh*0.55f, fw*0.80f, fh*0.35f, 3.f },
        };
        for (auto& r : rects)
            ctx_.push_rect(r.x, r.y, r.w, r.h,
                          1.f, 1.f, 1.f, 0.1f,
                          8.f, r.bw,
                          0.2f, 0.9f, 0.4f, 1.f);
        break;
    }
    case RenderMode::UV: {
        UiRect r{};
        r.x = fw*0.05f;  r.y = fh*0.05f;
        r.w = fw*0.90f;  r.h = fh*0.90f;
        r.fill[0] = 1.f; r.fill[1] = 1.f; r.fill[2] = 1.f; r.fill[3] = 1.f;
        r.corner_radius = 16.f;
        r.uv[0] = 0.f; r.uv[1] = 0.f; r.uv[2] = 1.f; r.uv[3] = 1.f;
        ctx_.push(r);
        break;
    }
    default: break;
    }
}


void ShaderRenderPanel::render_effect(shadebug::renderer::EffectRenderer& er, int w, int h) {
    if (!er.valid() || color_view_.id == SG_INVALID_ID) return;

    sg_pass_action pa = {};
    pa.colors[0].load_action = SG_LOADACTION_CLEAR;
    pa.colors[0].clear_value = { 0.f, 0.f, 0.f, 1.f };

    sg_attachments atts = {};
    atts.colors[0] = color_view_;

    sg_begin_pass(sg_pass{ .action = pa, .attachments = atts });
    er.flush(anim_time_, static_cast<float>(w), static_cast<float>(h), /*offscreen=*/true);
    sg_end_pass();
}


void ShaderRenderPanel::render_scene(shadebug::renderer::GpuRenderer& gpu_renderer,
                                      int w, int h) {
    // Safety: don't start a pass if either the renderer or the render target
    // is not ready — sg_begin_pass with an invalid attachment will segfault.
    if (!gpu_renderer.valid() || color_view_.id == SG_INVALID_ID)
        return;

    sg_pass_action pa = {};
    pa.colors[0].load_action  = SG_LOADACTION_CLEAR;
    pa.colors[0].clear_value  = { 0.08f, 0.08f, 0.10f, 1.f };

    sg_attachments atts = {};
    atts.colors[0] = color_view_;

    sg_begin_pass(sg_pass{
        .action      = pa,
        .attachments = atts,
    });

    gpu_renderer.flush(ctx_,
                       static_cast<float>(w),
                       static_cast<float>(h),
                       /*offscreen=*/true);

    sg_end_pass();
}


void ShaderRenderPanel::ensure_render_target(int w, int h) {
    if (rt_w_ == w && rt_h_ == h && color_img_.id != SG_INVALID_ID) return;

    destroy_render_target();

    color_img_ = sg_make_image(sg_image_desc{
        .usage  = { .color_attachment = true },
        .width  = w,
        .height = h,
        .label  = "render-panel-color",
    });

    color_view_ = sg_make_view(sg_view_desc{
        .color_attachment = { .image = color_img_ },
    });

    // Separate texture view for sampling in ImGui::Image
    sample_view_ = sg_make_view(sg_view_desc{
        .texture = { .image = color_img_ },
    });

    rt_w_ = w;
    rt_h_ = h;
}

void ShaderRenderPanel::destroy_render_target() {
    scene3d_.shutdown();
    if (sample_view_.id != SG_INVALID_ID) { sg_destroy_view(sample_view_); sample_view_ = {SG_INVALID_ID}; }
    if (color_view_.id  != SG_INVALID_ID) { sg_destroy_view(color_view_);  color_view_  = {SG_INVALID_ID}; }
    if (color_img_.id   != SG_INVALID_ID) { sg_destroy_image(color_img_);  color_img_   = {SG_INVALID_ID}; }
    rt_w_ = rt_h_ = 0;
}

//  SDF overlay (Option A hybrid)
//
//  populate_sdf_overlay fills a DrawCtx with HUD elements:
//    • A semi-transparent status bar at the bottom
//    • Green / amber indicator dots
//    • A top-left info panel and a top-right "3D" badge
//
//  render_sdf_overlay starts a colour-only offscreen pass on the SAME
//  colour image as the 3D scene (SG_LOADACTION_LOAD) and flushes the
//  overlay geometry through GpuRenderer's offscreen pipeline.
//  Because there is no depth attachment the pipeline's
//  depth.pixel_format = SG_PIXELFORMAT_NONE requirement is satisfied.

void ShaderRenderPanel::populate_sdf_overlay(int w, int h) {
    overlay_ctx_.clear();
    const float fw = static_cast<float>(w);
    const float fh = static_cast<float>(h);

    // Semi-transparent dark bar at the bottom of the viewport
    overlay_ctx_.push_rect(0.f, fh - 30.f, fw, 30.f,
                           0.04f, 0.04f, 0.08f, 0.80f,
                           0.f, 0.f);

    // Green "live" dot
    overlay_ctx_.push_rect(8.f, fh - 22.f, 14.f, 14.f,
                           0.10f, 0.90f, 0.30f, 0.95f, 7.f, 0.f);
    // Amber dot
    overlay_ctx_.push_rect(28.f, fh - 22.f, 14.f, 14.f,
                           0.90f, 0.75f, 0.10f, 0.95f, 7.f, 0.f);

    // Top-left: info panel (rounded rect with coloured border)
    overlay_ctx_.push_rect(10.f, 10.f, 130.f, 34.f,
                           0.08f, 0.08f, 0.14f, 0.82f,
                           6.f, 1.5f,
                           0.40f, 0.85f, 1.00f, 0.80f);

    // Top-right: small "3D" mode badge
    overlay_ctx_.push_rect(fw - 52.f, 10.f, 42.f, 24.f,
                           0.15f, 0.35f, 0.90f, 0.85f,
                           4.f, 1.f,
                           0.50f, 0.70f, 1.00f, 0.70f);
}

void ShaderRenderPanel::render_sdf_overlay(
        shadebug::renderer::GpuRenderer& gpu_renderer, int w, int h) {
    if (!gpu_renderer.valid() || !scene3d_.valid()) return;
    if (overlay_ctx_.count() == 0) return;

    // LOAD: keep the 3D pixels already written by scene3d_.render().
    // No depth attachment — matches GpuRenderer's offscreen pipeline.
    sg_pass_action pa = {};
    pa.colors[0].load_action = SG_LOADACTION_LOAD;

    sg_attachments atts = {};
    atts.colors[0] = scene3d_.color_att();   // same image, colour-only

    sg_begin_pass(sg_pass{ .action = pa, .attachments = atts });
    gpu_renderer.flush(overlay_ctx_,
                       static_cast<float>(w), static_cast<float>(h),
                       /*offscreen=*/true);
    sg_end_pass();
}


//  Orientation gizmo
//
//  Drawn in the bottom-right corner of the 3D viewport using the window
//  draw list — no external library required.
//
//  A small circle background + three coloured axis lines with dot-tips and
//  single-character labels shows the current camera orientation at a glance.
//  Replaces imGuIZMO.quat for basic orientation feedback; swap in the real
//  library by replacing this function later.

void ShaderRenderPanel::draw_orientation_gizmo(ImDrawList* dl,
                                                ImVec2 viewport_br,
                                                float azimuth, float elevation)
{
    constexpr float kR    = 30.f;   // gizmo circle radius
    constexpr float kPad  =  8.f;
    const ImVec2 center = { viewport_br.x - kR - kPad,
                            viewport_br.y - kR - kPad };

    // Background circle
    dl->AddCircleFilled(center, kR,     IM_COL32( 18, 18, 28, 170));
    dl->AddCircle      (center, kR,     IM_COL32(100,100,130, 160), 0, 1.2f);

    // Camera right (screen X) and up (screen Y) from azimuth/elevation
    const float ca = std::cos(azimuth),  sa = std::sin(azimuth);
    const float ce = std::cos(elevation),se = std::sin(elevation);
    // right = (cos az, 0, -sin az)   in world space projected to screen
    // up    = (sin az * sin el, cos el, cos az * sin el)
    auto proj2d = [&](float wx, float wy, float wz) -> ImVec2 {
        const float sx =  wx*ca  - wz*sa;              // camera right component
        const float sy = -wx*sa*se - wy*ce - wz*ca*se;  // camera up (negative = screen down)
        return { center.x + sx * (kR - 5.f),
                 center.y + sy * (kR - 5.f) };
    };

    const ImVec2 px = proj2d(1,0,0);
    const ImVec2 py = proj2d(0,1,0);
    const ImVec2 pz = proj2d(0,0,1);

    // Draw negative half-axes (dimmed) first so positives render on top
    auto neg = [](ImVec2 c, ImVec2 p) -> ImVec2 {
        return { 2*c.x - p.x, 2*c.y - p.y };
    };
    dl->AddLine(center, neg(center, px), IM_COL32(100, 30, 30, 120), 1.f);
    dl->AddLine(center, neg(center, py), IM_COL32( 30,100, 30, 120), 1.f);
    dl->AddLine(center, neg(center, pz), IM_COL32( 30, 50,120, 120), 1.f);

    // Positive axes
    dl->AddLine(center, px, IM_COL32(230, 60, 60, 230), 2.f);
    dl->AddLine(center, py, IM_COL32( 60,220, 60, 230), 2.f);
    dl->AddLine(center, pz, IM_COL32( 60,110,230, 230), 2.f);

    // Dots
    dl->AddCircleFilled(px, 4.f, IM_COL32(230, 60,  60, 255));
    dl->AddCircleFilled(py, 4.f, IM_COL32( 60,220,  60, 255));
    dl->AddCircleFilled(pz, 4.f, IM_COL32( 60,110, 230, 255));

    // Labels
    dl->AddText({px.x + 3.f, px.y - 7.f}, IM_COL32(255,110,110,230), "X");
    dl->AddText({py.x + 3.f, py.y - 7.f}, IM_COL32(110,255,110,230), "Y");
    dl->AddText({pz.x + 3.f, pz.y - 7.f}, IM_COL32(110,160,255,230), "Z");
}


// ─────────────────────────────────────────────────────────────────────────────
//  Params panel
//
//  Renders interactive ImGui widgets for each ShaderParam.
//  Returns true if any value changed.
// ─────────────────────────────────────────────────────────────────────────────

bool ShaderRenderPanel::draw_params_panel(
        std::vector<shadebug::renderer::ShaderParam>& params, float time) {
    using namespace shadebug::renderer;

    bool changed = false;

    ImGui::SeparatorText("Shader Params");

    for (auto& p : params) {
        ImGui::PushID(p.name.c_str());

        // Motion indicator badge
        const bool animated = p.motion_enabled && p.motion.mode != MotionMode::Static;
        if (animated) {
            ImGui::PushStyleColor(ImGuiCol_Text, IM_COL32(80, 200, 120, 255));
            ImGui::TextUnformatted("~");
            ImGui::PopStyleColor();
            ImGui::SameLine();
        }

        // Widget per type
        switch (p.type) {

        case ParamType::Float:
            if (ImGui::SliderFloat(p.label.c_str(), &p.val[0], p.range_min, p.range_max))
                changed = true;
            if (animated && ImGui::IsItemHovered())
                ImGui::SetTooltip("Driven by motion (oscillate/keyframes)\nDrag to override");
            break;

        case ParamType::Float2:
            if (ImGui::SliderFloat2(p.label.c_str(), p.val, p.range_min, p.range_max))
                changed = true;
            break;

        case ParamType::Float3:
            if (ImGui::SliderFloat3(p.label.c_str(), p.val, p.range_min, p.range_max))
                changed = true;
            break;

        case ParamType::Color4:
            if (ImGui::ColorEdit4(p.label.c_str(), p.val,
                    ImGuiColorEditFlags_Float | ImGuiColorEditFlags_HDR))
                changed = true;
            break;

        case ParamType::Bool: {
            bool b = p.val[0] > 0.5f;
            if (ImGui::Checkbox(p.label.c_str(), &b)) {
                p.val[0] = b ? 1.f : 0.f;
                changed  = true;
            }
            break;
        }

        case ParamType::Int: {
            int iv = static_cast<int>(p.val[0]);
            if (ImGui::SliderInt(p.label.c_str(), &iv,
                    static_cast<int>(p.range_min), static_cast<int>(p.range_max))) {
                p.val[0] = static_cast<float>(iv);
                changed  = true;
            }
            break;
        }

        default: break;
        }

        // Context menu: reset / toggle motion
        if (ImGui::BeginPopupContextItem("##ctx")) {
            if (ImGui::MenuItem("Reset to default"))  { p.reset_to_default(); changed = true; }
            if (p.motion_enabled) {
                const char* lbl = (p.motion.mode == MotionMode::Static)
                    ? "Enable motion" : "Pause motion";
                if (ImGui::MenuItem(lbl)) {
                    p.motion.mode = (p.motion.mode == MotionMode::Static)
                        ? MotionMode::Oscillate : MotionMode::Static;
                }
            }
            if (ImGui::MenuItem("Copy slot info")) {
                const auto s = std::format("slot={} // {}", p.slot,
                    param_type_name(p.type));
                ImGui::SetClipboardText(s.c_str());
            }
            ImGui::EndPopup();
        }
        if (ImGui::IsItemClicked(ImGuiMouseButton_Right))
            ImGui::OpenPopup("##ctx");

        // Slot badge (small, right-aligned)
        {
            const auto badge = std::format("[{}]", p.slot);
            const float bw   = ImGui::CalcTextSize(badge.c_str()).x;
            const float cx   = ImGui::GetContentRegionMax().x - bw;
            const float cy   = ImGui::GetCursorPosY() - ImGui::GetTextLineHeightWithSpacing();
            if (cx > 0.f) {
                ImGui::SetCursorPosX(cx);
                ImGui::SetCursorPosY(cy);
                ImGui::TextDisabled("%s", badge.c_str());
            }
        }

        ImGui::PopID();
    }

    ImGui::Spacing();
    if (ImGui::SmallButton("Reset all")) {
        for (auto& p : params) p.reset_to_default();
        changed = true;
    }

    // Live readout footer
    ImGui::Spacing();
    ImGui::SeparatorText("Uniform slots");
    ImGui::PushStyleColor(ImGuiCol_Text, IM_COL32(120, 180, 255, 200));
    const auto pu = pack_params(params, time);
    const float* raw = reinterpret_cast<const float*>(&pu);
    for (int i = 0; i < 16; i += 4) {
        ImGui::TextDisabled("%2d: %.3f %.3f %.3f %.3f",
            i, raw[i], raw[i+1], raw[i+2], raw[i+3]);
    }
    ImGui::PopStyleColor();

    return changed;
}


// ─────────────────────────────────────────────────────────────────────────────
//  Fallback pattern
//
//  Drawn entirely with the ImGui draw list when the GPU shader pipeline is
//  not yet valid (shader failed to load or compile).
//  Palette: near-black-purple background (#0F051E) + hot-pink (#FF50B4) dots.

void ShaderRenderPanel::draw_fallback_pattern(ImVec2 origin, ImVec2 size) {
    ImDrawList* dl = ImGui::GetWindowDrawList();

    const ImVec2 br { origin.x + size.x, origin.y + size.y };

    // Dark black-purple background
    dl->AddRectFilled(origin, br, IM_COL32(15, 5, 30, 255));

    // Tiled pink dot grid
    constexpr float kSpacing  = 22.f;
    constexpr float kRadius   = 2.2f;
    constexpr ImU32 kDotColor = IM_COL32(255, 80, 180, 210);

    const float x0 = origin.x + std::fmod(kSpacing * 0.5f, kSpacing);
    const float y0 = origin.y + std::fmod(kSpacing * 0.5f, kSpacing);

    for (float y = y0; y < br.y; y += kSpacing)
        for (float x = x0; x < br.x; x += kSpacing)
            dl->AddCircleFilled(ImVec2(x, y), kRadius, kDotColor);

    // Status label
    constexpr ImU32 kLabelColor = IM_COL32(255, 120, 200, 230);
    dl->AddText(ImVec2(origin.x + 10.f, origin.y + 10.f),
                kLabelColor, "Shader unavailable");
}

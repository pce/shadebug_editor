// sdf_panel.cpp
#include "sdf_panel.hpp"
#include "../renderer/effect_renderer.hpp"   // EffectUniforms
#include "sokol_app.h"
#include "sokol_gfx.h"
#include "imgui.h"
#include "util/sokol_imgui.h"

#include <algorithm>
#include <cmath>
#include <format>
#include <fstream>
#include <sstream>
#include <print>

namespace shadebug::panels {

using namespace std::string_literals;

// ── Destructor ─────────────────────────────────────────────────────────────

SdfPanel::~SdfPanel() {
    if (sg_isvalid()) shutdown();
}

// ── Init / shutdown ─────────────────────────────────────────────────────────

void SdfPanel::init(const std::filesystem::path& exe_dir) {
    namespace fs = std::filesystem;

    auto read = [](const fs::path& p) -> std::string {
        std::ifstream f(p);
        if (!f) return {};
        std::ostringstream ss;
        ss << f.rdbuf();
        return ss.str();
    };

    const fs::path vs_path = exe_dir / "data/shaders/msl/effect.vert.metal";
    const fs::path fs_path = exe_dir / "data/shaders/msl/sdf_scene.frag.metal";

    const std::string vs      = read(vs_path);
    const std::string frag_src = read(fs_path);

    if (vs.empty() || frag_src.empty()) {
        std::println("[SdfPanel] shader files not found: {} / {}",
                     vs_path.string(), fs_path.string());
        return;
    }

    if (!build_pipeline(vs, frag_src)) return;

    // Create fullscreen quad VB
    constexpr float kQuad[12] = {
        0,0, 1,0, 1,1,
        0,0, 1,1, 0,1,
    };
    quad_vb_ = sg_make_buffer(sg_buffer_desc{
        .size  = sizeof(kQuad),
        .data  = SG_RANGE(kQuad),
        .label = "sdf-quad-vb",
    });

    // Default scene: 2 circles + 1 round rect
    nodes_.push_back({ "Circle A",  SdfShape::Circle,   SdfBlend::Union,
                        0.38f, 0.50f, 0.18f, 0.18f, 0.00f, 0.05f,
                        {0.20f, 0.55f, 1.00f, 1.f} });
    nodes_.push_back({ "Circle B",  SdfShape::Circle,   SdfBlend::SmoothUnion,
                        0.62f, 0.50f, 0.14f, 0.14f, 0.00f, 0.08f,
                        {1.00f, 0.40f, 0.30f, 1.f} });
    nodes_.push_back({ "Round Rect",SdfShape::RoundBox, SdfBlend::SmoothUnion,
                        0.50f, 0.70f, 0.20f, 0.08f, 0.04f, 0.06f,
                        {0.30f, 0.90f, 0.50f, 1.f} });

    gpu_ready_ = true;
    std::println("[SdfPanel] initialised ({} default nodes)", nodes_.size());
}

void SdfPanel::shutdown() {
    destroy_render_target();
    destroy_pipeline();
    if (quad_vb_.id != SG_INVALID_ID) { sg_destroy_buffer(quad_vb_); quad_vb_ = {SG_INVALID_ID}; }
    gpu_ready_ = false;
}

// ── Pipeline build ────────────────────────────────────────────────────────────

bool SdfPanel::build_pipeline(std::string_view vs, std::string_view fs) {
    sg_shader_desc sd = {};
    sd.label                 = "sdf-scene-shader";
    sd.vertex_func.source    = vs.data();
    sd.vertex_func.entry     = "vs_main";
    sd.fragment_func.source  = fs.data();
    sd.fragment_func.entry   = "fs_main";
    sd.attrs[0] = { .glsl_name = "a_pos", .hlsl_sem_name = "POSITION" };

    // Block 0: time + resolution (same as all effect shaders)
    sd.uniform_blocks[0] = {
        .stage             = SG_SHADERSTAGE_FRAGMENT,
        .size              = sizeof(shadebug::renderer::EffectUniforms),
        .msl_buffer_n      = 0,
        .hlsl_register_b_n = 0,
        .glsl_uniforms = {
            [0] = { .type = SG_UNIFORMTYPE_FLOAT2, .glsl_name = "iResolution" },
            [1] = { .type = SG_UNIFORMTYPE_FLOAT,  .glsl_name = "iTime"       },
        },
    };
    // Block 1: SDF node buffer (Metal-primary; 496 bytes)
    sd.uniform_blocks[1] = {
        .stage             = SG_SHADERSTAGE_FRAGMENT,
        .size              = sizeof(GpuSdfBlock),
        .msl_buffer_n      = 1,
        .hlsl_register_b_n = 1,
    };

    shd_ = sg_make_shader(&sd);
    if (shd_.id == SG_INVALID_ID) {
        std::println("[SdfPanel] shader compilation failed");
        return false;
    }

    auto make_pip = [&](bool offscreen) -> sg_pipeline {
        sg_pipeline_desc pd = {};
        pd.shader = shd_;
        pd.label  = offscreen ? "sdf-pip-offscr" : "sdf-pip-screen";
        pd.layout.buffers[0].stride = 8;
        pd.layout.attrs[0] = { .format = SG_VERTEXFORMAT_FLOAT2 };
        pd.colors[0].blend.enabled = false;
        pd.depth.write_enabled = false;
        pd.depth.compare       = SG_COMPAREFUNC_ALWAYS;
        if (offscreen) pd.depth.pixel_format = SG_PIXELFORMAT_NONE;
        return sg_make_pipeline(&pd);
    };

    pip_screen_ = make_pip(false);
    pip_offscr_ = make_pip(true);

    if (pip_screen_.id == SG_INVALID_ID || pip_offscr_.id == SG_INVALID_ID) {
        std::println("[SdfPanel] pipeline creation failed");
        destroy_pipeline();
        return false;
    }
    return true;
}

void SdfPanel::destroy_pipeline() {
    if (pip_offscr_.id != SG_INVALID_ID) { sg_destroy_pipeline(pip_offscr_); pip_offscr_ = {SG_INVALID_ID}; }
    if (pip_screen_.id != SG_INVALID_ID) { sg_destroy_pipeline(pip_screen_); pip_screen_ = {SG_INVALID_ID}; }
    if (shd_.id        != SG_INVALID_ID) { sg_destroy_shader(shd_);          shd_        = {SG_INVALID_ID}; }
}

// ── Render target ─────────────────────────────────────────────────────────────

void SdfPanel::ensure_render_target(int w, int h) {
    if (rt_w_ == w && rt_h_ == h && color_img_.id != SG_INVALID_ID) return;
    destroy_render_target();

    color_img_ = sg_make_image(sg_image_desc{
        .usage  = { .color_attachment = true },
        .width  = w, .height = h,
        .label  = "sdf-color",
    });
    color_view_ = sg_make_view(sg_view_desc{
        .color_attachment = { .image = color_img_ },
    });
    sample_view_ = sg_make_view(sg_view_desc{
        .texture = { .image = color_img_ },
    });
    rt_w_ = w; rt_h_ = h;
}

void SdfPanel::destroy_render_target() {
    if (sample_view_.id != SG_INVALID_ID) { sg_destroy_view(sample_view_);  sample_view_ = {SG_INVALID_ID}; }
    if (color_view_.id  != SG_INVALID_ID) { sg_destroy_view(color_view_);   color_view_  = {SG_INVALID_ID}; }
    if (color_img_.id   != SG_INVALID_ID) { sg_destroy_image(color_img_);   color_img_   = {SG_INVALID_ID}; }
    rt_w_ = rt_h_ = 0;
}

// ── GPU pack ──────────────────────────────────────────────────────────────────

GpuSdfBlock SdfPanel::pack_nodes() const noexcept {
    GpuSdfBlock b{};
    b.node_count = static_cast<int>(std::min(nodes_.size(), std::size_t(kSdfMaxNodes)));
    b.debug_mode = debug_mode_;
    for (int i = 0; i < b.node_count; ++i) {
        const auto& n = nodes_[static_cast<std::size_t>(i)];
        b.nodes[i].p0[0] = n.x;
        b.nodes[i].p0[1] = n.y;
        b.nodes[i].p0[2] = n.rx;
        b.nodes[i].p0[3] = static_cast<float>(n.shape);
        b.nodes[i].p1[0] = n.ry;
        b.nodes[i].p1[1] = n.corner_r;
        b.nodes[i].p1[2] = n.smooth_k;
        b.nodes[i].p1[3] = 0.f;
        b.nodes[i].p2[0] = n.color[0];
        b.nodes[i].p2[1] = n.color[1];
        b.nodes[i].p2[2] = n.color[2];
        b.nodes[i].p2[3] = static_cast<float>(n.blend);
    }
    return b;
}

// ── Render ────────────────────────────────────────────────────────────────────

void SdfPanel::render_to_target(int w, int h) {
    if (!gpu_ready_ || color_view_.id == SG_INVALID_ID) return;

    sg_pass_action pa = {};
    pa.colors[0].load_action = SG_LOADACTION_CLEAR;
    pa.colors[0].clear_value = { 0.04f, 0.04f, 0.06f, 1.f };

    sg_attachments atts = {};
    atts.colors[0] = color_view_;

    sg_begin_pass(sg_pass{ .action = pa, .attachments = atts });
    sg_apply_pipeline(pip_offscr_);

    sg_bindings bindings = {};
    bindings.vertex_buffers[0] = quad_vb_;
    sg_apply_bindings(bindings);

    const shadebug::renderer::EffectUniforms eu{
        static_cast<float>(w), static_cast<float>(h), anim_time_, 0.f };
    sg_apply_uniforms(0, SG_RANGE(eu));

    const GpuSdfBlock block = pack_nodes();
    sg_apply_uniforms(1, SG_RANGE(block));

    sg_draw(0, 6, 1);
    sg_end_pass();
}

// ── Hit-testing ───────────────────────────────────────────────────────────────

// Returns node index whose center is within threshold_uv of mouse_uv, or -1.
int SdfPanel::hit_test_center(ImVec2 mouse_uv, float asp, float threshold_uv) const {
    for (int i = static_cast<int>(nodes_.size()) - 1; i >= 0; --i) {
        const auto& n  = nodes_[static_cast<std::size_t>(i)];
        const float dx = (mouse_uv.x - n.x) * asp;
        const float dy =  mouse_uv.y - n.y;
        if (std::sqrt(dx*dx + dy*dy) < threshold_uv) return i;
    }
    return -1;
}

// Returns node index whose edge is within threshold_uv of mouse_uv, or -1.
int SdfPanel::hit_test_edge(ImVec2 mouse_uv, float asp, float threshold_uv) const {
    for (int i = static_cast<int>(nodes_.size()) - 1; i >= 0; --i) {
        const auto& n  = nodes_[static_cast<std::size_t>(i)];
        const float dx = (mouse_uv.x - n.x) * asp;
        const float dy =  mouse_uv.y - n.y;
        const float d  = std::sqrt(dx*dx + dy*dy);
        if (std::abs(d - n.rx) < threshold_uv) return i;
    }
    return -1;
}

// ── Handle overlay ────────────────────────────────────────────────────────────

void SdfPanel::draw_handles_overlay(
        ImDrawList* dl,
        const std::vector<SdfNode>& nodes,
        ImVec2 canvas_pos, ImVec2 canvas_size,
        int selected, int hover, float asp)
{
    const float  inv_asp = 1.f / std::max(asp, 1e-3f);

    for (int i = 0; i < static_cast<int>(nodes.size()); ++i) {
        const auto& n    = nodes[static_cast<std::size_t>(i)];
        const bool  sel  = (i == selected);
        const bool  hov  = (i == hover);

        // Node center in screen coordinates
        const ImVec2 c {
            canvas_pos.x + n.x * canvas_size.x,
            canvas_pos.y + n.y * canvas_size.y
        };

        // Circle: draw radius ring
        if (n.shape == SdfShape::Circle) {
            const float screen_r = n.rx * canvas_size.y;   // y-fraction → px
            const ImU32 ring_col = sel ? IM_COL32(255,220, 60,200)
                                       : IM_COL32(200,200,200, 80);
            dl->AddCircle(c, screen_r, ring_col, 0, sel ? 1.5f : 1.0f);
        }
        // Box / RoundBox: draw bounding rect
        if (n.shape == SdfShape::Box || n.shape == SdfShape::RoundBox) {
            const float sw = n.rx * canvas_size.y;   // half-width in px (y-fraction units)
            const float sh = n.ry * canvas_size.y;
            const ImVec2 tl { c.x - sw, c.y - sh };
            const ImVec2 br { c.x + sw, c.y + sh };
            const ImU32  col = sel ? IM_COL32(255,220,60,200) : IM_COL32(200,200,200,80);
            if (n.shape == SdfShape::RoundBox) {
                const float cr = n.corner_r * canvas_size.y;
                dl->AddRect(tl, br, col, cr, 0, sel ? 1.5f : 1.f);
            } else {
                dl->AddRect(tl, br, col, 0.f, 0, sel ? 1.5f : 1.f);
            }
        }

        // Center dot
        const ImU32 dot_col = sel  ? IM_COL32(255,220, 60,240) :
                              hov  ? IM_COL32(255,255,255,180) :
                                     IM_COL32(200,200,200,120);
        dl->AddCircleFilled(c, sel ? 5.f : 4.f, dot_col);
        dl->AddCircle      (c, sel ? 5.f : 4.f, IM_COL32(0,0,0,100));

        // Label
        if (sel || hov) {
            const std::string tag = std::format("{}: {}", i, n.label);
            dl->AddText({ c.x + 8.f, c.y - 8.f }, IM_COL32(255,255,200,230), tag.c_str());
        }
    }
}

// ── draw() ───────────────────────────────────────────────────────────────────

void SdfPanel::draw(bool& visible) {
    if (!visible) return;
    anim_time_ += static_cast<float>(sapp_frame_duration());

    ImGui::SetNextWindowSize(ImVec2(900, 620), ImGuiCond_FirstUseEver);
    if (!ImGui::Begin("SDF Scene##sdf_panel", &visible)) {
        ImGui::End();
        return;
    }

    if (!gpu_ready_) {
        ImGui::TextColored({1,0.4f,0.4f,1}, "SDF Panel not initialised – shader files missing?");
        ImGui::End();
        return;
    }

    // ── Toolbar ───────────────────────────────────────────────────────────────
    const char* kDebugLabels[] = { "Normal", "Heatmap", "Fill + Iso", "Iso-lines" };
    ImGui::AlignTextToFramePadding();
    ImGui::TextUnformatted("View:");  ImGui::SameLine();
    for (int d = 0; d < 4; ++d) {
        if (ImGui::RadioButton(kDebugLabels[d], debug_mode_ == d)) debug_mode_ = d;
        ImGui::SameLine();
    }

    ImGui::TextDisabled("|");  ImGui::SameLine();
    if (ImGui::SmallButton("+ Circle"))
        nodes_.push_back({ std::format("Circle_{}", nodes_.size()),
                           SdfShape::Circle, SdfBlend::Union,
                           0.50f, 0.50f, 0.12f, 0.12f, 0.f, 0.05f,
                           {0.4f,0.7f,1.0f,1.f} });
    ImGui::SameLine();
    if (ImGui::SmallButton("+ Box"))
        nodes_.push_back({ std::format("Box_{}", nodes_.size()),
                           SdfShape::Box, SdfBlend::Union,
                           0.50f, 0.50f, 0.15f, 0.10f, 0.f, 0.05f,
                           {1.0f,0.7f,0.3f,1.f} });
    ImGui::SameLine();
    if (ImGui::SmallButton("+ Round"))
        nodes_.push_back({ std::format("Round_{}", nodes_.size()),
                           SdfShape::RoundBox, SdfBlend::SmoothUnion,
                           0.50f, 0.50f, 0.15f, 0.10f, 0.03f, 0.06f,
                           {0.5f,1.0f,0.6f,1.f} });

    ImGui::NewLine();
    ImGui::Separator();

    // ── Split layout: canvas (left) + node list (right) ──────────────────────
    const ImVec2 avail       = ImGui::GetContentRegionAvail();
    constexpr float kSideW   = 260.f;
    const float canvas_w     = std::max(16.f, avail.x - kSideW - 6.f);
    const float canvas_h     = std::max(16.f, avail.y);

    // ── Canvas child ──────────────────────────────────────────────────────────
    ImGui::BeginChild("##sdf_canvas", ImVec2(canvas_w, canvas_h), ImGuiChildFlags_None,
                      ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);

    const ImVec2 canvas_pos  = ImGui::GetCursorScreenPos();
    const ImVec2 canvas_size = { canvas_w, canvas_h };
    const int    vp_w = static_cast<int>(canvas_w);
    const int    vp_h = static_cast<int>(canvas_h);
    const float  asp  = canvas_w / std::max(canvas_h, 1.f);

    ensure_render_target(vp_w, vp_h);
    render_to_target(vp_w, vp_h);

    const bool focused = ImGui::IsWindowHovered(ImGuiHoveredFlags_ChildWindows);

    if (sample_view_.id != SG_INVALID_ID)
        ImGui::Image(simgui_imtextureid(sample_view_), canvas_size);
    else
        ImGui::Dummy(canvas_size);

    // Handle mouse interaction on the image
    draw_canvas(canvas_pos, canvas_size, focused);

    // Draw interactive handles on top
    {
        ImDrawList* dl = ImGui::GetWindowDrawList();
        // Determine hover from last frame
        const ImVec2 mouse_scr = ImGui::GetIO().MousePos;
        ImVec2 mouse_uv{
            (mouse_scr.x - canvas_pos.x) / canvas_size.x,
            (mouse_scr.y - canvas_pos.y) / canvas_size.y
        };
        bool in_canvas = mouse_uv.x >= 0 && mouse_uv.x <= 1 &&
                         mouse_uv.y >= 0 && mouse_uv.y <= 1;
        int hover = in_canvas ? hit_test_center(mouse_uv, asp, 0.04f) : -1;
        if (hover < 0 && in_canvas)
            hover = hit_test_edge(mouse_uv, asp, 0.015f);

        draw_handles_overlay(dl, nodes_, canvas_pos, canvas_size,
                             selected_, hover, asp);
    }

    ImGui::EndChild();

    // ── Node sidebar ──────────────────────────────────────────────────────────
    ImGui::SameLine();
    ImGui::BeginChild("##sdf_side", ImVec2(kSideW, canvas_h), ImGuiChildFlags_Borders);
    draw_node_sidebar();
    ImGui::EndChild();

    ImGui::End();
}

// ── Canvas mouse interaction ──────────────────────────────────────────────────

void SdfPanel::draw_canvas(ImVec2 canvas_pos, ImVec2 canvas_size, bool focused) {
    const auto& io  = ImGui::GetIO();
    const float asp = canvas_size.x / std::max(canvas_size.y, 1.f);

    // Convert current mouse position to UV
    ImVec2 mouse_uv {
        (io.MousePos.x - canvas_pos.x) / canvas_size.x,
        (io.MousePos.y - canvas_pos.y) / canvas_size.y
    };
    const bool in_canvas = mouse_uv.x >= 0 && mouse_uv.x <= 1 &&
                           mouse_uv.y >= 0 && mouse_uv.y <= 1;

    if (!focused || !in_canvas) {
        drag_mode_ = DragMode::None;
        return;
    }

    // ── Begin drag ─────────────────────────────────────────────────────────
    if (ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
        // Priority: check edge first (resize), then center (move)
        int edge_idx   = hit_test_edge  (mouse_uv, asp, 0.015f);
        int center_idx = hit_test_center(mouse_uv, asp, 0.040f);

        if (edge_idx >= 0) {
            drag_mode_     = DragMode::ResizeNode;
            drag_idx_      = edge_idx;
            drag_start_uv_ = mouse_uv;
            drag_start_rx_ = nodes_[static_cast<std::size_t>(edge_idx)].rx;
            drag_start_ry_ = nodes_[static_cast<std::size_t>(edge_idx)].ry;
            selected_      = edge_idx;
        } else if (center_idx >= 0) {
            drag_mode_     = DragMode::MoveNode;
            drag_idx_      = center_idx;
            drag_start_uv_ = mouse_uv;
            drag_start_x_  = nodes_[static_cast<std::size_t>(center_idx)].x;
            drag_start_y_  = nodes_[static_cast<std::size_t>(center_idx)].y;
            selected_      = center_idx;
        } else {
            drag_mode_ = DragMode::None;
            selected_  = -1;
        }
    }

    // ── Apply drag ─────────────────────────────────────────────────────────
    if (ImGui::IsMouseDown(ImGuiMouseButton_Left) && drag_mode_ != DragMode::None) {
        const float du = mouse_uv.x - drag_start_uv_.x;
        const float dv = mouse_uv.y - drag_start_uv_.y;
        auto& n = nodes_[static_cast<std::size_t>(drag_idx_)];

        if (drag_mode_ == DragMode::MoveNode) {
            n.x = std::clamp(drag_start_x_ + du, 0.01f, 0.99f);
            n.y = std::clamp(drag_start_y_ + dv, 0.01f, 0.99f);
        } else {
            // Resize: change rx (and ry for box) by the distance delta
            // Work in aspect-corrected space for natural feel
            const float dist = std::sqrt(du*du*asp*asp + dv*dv);
            if (n.shape == SdfShape::Circle) {
                n.rx = std::clamp(drag_start_rx_ + dist * (dv > 0 ? 1.f : -1.f) * 0.5f,
                                  0.01f, 0.49f);
            } else {
                // Box: drag in x changes rx, drag in y changes ry
                n.rx = std::clamp(drag_start_rx_ + du * asp, 0.01f, 0.49f);
                n.ry = std::clamp(drag_start_ry_ + dv,       0.01f, 0.49f);
            }
        }
        // Tooltip
        const auto tip = std::format("({:.3f}, {:.3f})  r: {:.3f}",
                                     nodes_[static_cast<std::size_t>(drag_idx_)].x,
                                     nodes_[static_cast<std::size_t>(drag_idx_)].y,
                                     nodes_[static_cast<std::size_t>(drag_idx_)].rx);
        ImGui::SetTooltip("%s", tip.c_str());
    }

    if (ImGui::IsMouseReleased(ImGuiMouseButton_Left))
        drag_mode_ = DragMode::None;

    // Cursor feedback
    {
        int edge_idx   = hit_test_edge  (mouse_uv, asp, 0.015f);
        int center_idx = hit_test_center(mouse_uv, asp, 0.040f);
        if (edge_idx   >= 0) ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeAll);
        else if (center_idx >= 0) ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);
    }
}

// ── Node sidebar ──────────────────────────────────────────────────────────────

void SdfPanel::draw_node_sidebar() {
    ImGui::SeparatorText("Nodes");

    const char* kShapeIcons[] = { "○", "□", "◙" };
    const char* kBlendNames[] = { "Union", "Smooth∪", "Subtract", "Intersect" };

    for (int i = 0; i < static_cast<int>(nodes_.size()); ++i) {
        auto& n = nodes_[static_cast<std::size_t>(i)];
        const bool sel = (i == selected_);

        ImGui::PushID(i);

        if (sel) ImGui::PushStyleColor(ImGuiCol_Header, ImVec4(0.25f,0.50f,0.85f,0.60f));
        const bool open = ImGui::Selectable(
            std::format("{} [{}] {}##node{}", kShapeIcons[static_cast<int>(n.shape)],
                        kBlendNames[static_cast<int>(n.blend)], n.label, i).c_str(),
            sel, ImGuiSelectableFlags_AllowOverlap, {0,0});
        if (open) selected_ = i;
        if (sel) ImGui::PopStyleColor();

        // Small color swatch + delete button on the right
        ImGui::SameLine(ImGui::GetContentRegionMax().x - 40.f);
        ImGui::ColorButton("##col", ImVec4(n.color[0],n.color[1],n.color[2],1),
                           ImGuiColorEditFlags_NoPicker | ImGuiColorEditFlags_NoTooltip,
                           {14,14});
        ImGui::SameLine();
        if (ImGui::SmallButton("×")) {
            nodes_.erase(nodes_.begin() + i);
            if (selected_ >= static_cast<int>(nodes_.size()))
                selected_ = static_cast<int>(nodes_.size()) - 1;
            ImGui::PopID();
            break;
        }

        // Inline properties when selected
        if (sel && !nodes_.empty()) {
            ImGui::Indent(8.f);
            draw_node_properties(n);
            ImGui::Unindent(8.f);
        }
        ImGui::PopID();
    }

    // Node count cap warning
    if (static_cast<int>(nodes_.size()) >= kSdfMaxNodes) {
        ImGui::Spacing();
        ImGui::TextColored({1,0.7f,0,1}, "Max %d nodes reached", kSdfMaxNodes);
    }

    // Live uniform slot readout (collapsible)
    ImGui::Spacing();
    if (ImGui::CollapsingHeader("GPU Buffer")) {
        const auto blk = pack_nodes();
        ImGui::TextDisabled("count=%d  debug=%d", blk.node_count, blk.debug_mode);
        for (int i = 0; i < blk.node_count; ++i) {
            ImGui::PushStyleColor(ImGuiCol_Text, IM_COL32(120,200,255,200));
            ImGui::TextDisabled("[%d] p=(%.2f,%.2f) rx=%.2f t=%d op=%d",
                i,
                blk.nodes[i].p0[0], blk.nodes[i].p0[1],
                blk.nodes[i].p0[2],
                (int)blk.nodes[i].p0[3],
                (int)blk.nodes[i].p2[3]);
            ImGui::PopStyleColor();
        }
    }
}

// ── Per-node properties ────────────────────────────────────────────────────────

void SdfPanel::draw_node_properties(SdfNode& n) {
    ImGui::SetNextItemWidth(120);
    ImGui::InputText("Name##nprop", &n.label[0], n.label.capacity() > 32 ? n.label.capacity() : 32);

    // Shape combo
    const char* kShapeNames[] = { "Circle", "Box", "RoundBox" };
    int s = static_cast<int>(n.shape);
    ImGui::SetNextItemWidth(100);
    if (ImGui::Combo("Shape##nprop", &s, kShapeNames, 3))
        n.shape = static_cast<SdfShape>(s);

    // Blend op combo
    const char* kBlendNames[] = { "Union", "SmoothUnion", "Subtract", "Intersect" };
    int b = static_cast<int>(n.blend);
    ImGui::SetNextItemWidth(100);
    if (ImGui::Combo("Blend##nprop", &b, kBlendNames, 4))
        n.blend = static_cast<SdfBlend>(b);

    ImGui::SetNextItemWidth(120);
    ImGui::SliderFloat2("Pos##nprop",  &n.x, 0.f, 1.f, "%.3f");
    ImGui::SetNextItemWidth(120);
    if (n.shape == SdfShape::Circle) {
        ImGui::SliderFloat("Radius##nprop", &n.rx, 0.01f, 0.48f, "%.3f");
    } else {
        ImGui::SliderFloat2("Half-ext##nprop", &n.rx, 0.01f, 0.48f, "%.3f");
    }
    if (n.shape == SdfShape::RoundBox) {
        ImGui::SetNextItemWidth(120);
        ImGui::SliderFloat("Corner##nprop", &n.corner_r, 0.f, 0.1f, "%.3f");
    }
    if (n.blend == SdfBlend::SmoothUnion) {
        ImGui::SetNextItemWidth(120);
        ImGui::SliderFloat("Smooth-k##nprop", &n.smooth_k, 0.001f, 0.2f, "%.3f");
    }
    ImGui::SetNextItemWidth(120);
    ImGui::ColorEdit3("Color##nprop", n.color,
                      ImGuiColorEditFlags_Float | ImGuiColorEditFlags_HDR);
}

} // namespace shadebug::panels


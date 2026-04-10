#pragma once
// sdf_panel.hpp
//
//  Self-contained SDF scene editor panel.
//
//  Features:
//  - Renders a fullscreen SDF shader evaluating a flat node buffer (≤10 nodes)
//  - Node types: Circle, Box, RoundBox
//  - Blend ops:  Union, SmoothUnion, Subtract, Intersect
//  - Debug modes: Normal | Distance heatmap | Fill + iso-lines | Iso-lines only
//  - Mouse interaction: drag node (move), drag edge (resize radius / half-extent)
//  - Node list + property sidebar with ImGui widgets
//
//  GPU layout (Metal buffer 1, 496 bytes):
//    struct SdfBlock { int4 meta; SdfNode nodes[10]; };   // meta.x=count, meta.y=debug
//    struct SdfNode  { float4 p0; float4 p1; float4 p2; };
//      p0: (x, y, rx, type)     type: 0=circle 1=box 2=round_box
//      p1: (ry, corner_r, smooth_k, _pad)
//      p2: (r,  g,  b,  op)     op:   0=union 1=smooth_union 2=subtract 3=intersect

#include "sokol_gfx.h"
#include "imgui.h"
#include <string>
#include <vector>
#include <filesystem>

namespace shadebug { class App; }

namespace shadebug::panels {

// Node data

enum class SdfShape { Circle = 0, Box = 1, RoundBox = 2 };
enum class SdfBlend { Union = 0, SmoothUnion = 1, Subtract = 2, Intersect = 3 };

struct SdfNode {
    std::string label;
    SdfShape    shape    = SdfShape::Circle;
    SdfBlend    blend    = SdfBlend::Union;
    float       x        = 0.50f;   ///< position, UV [0,1]
    float       y        = 0.50f;
    float       rx       = 0.15f;   ///< radius (circle) or half-width  (box)
    float       ry       = 0.10f;   ///< half-height (box only)
    float       corner_r = 0.02f;   ///< corner radius (round_box only)
    float       smooth_k = 0.05f;   ///< smooth-union blend factor
    float       color[4] = {1.f, 0.5f, 0.2f, 1.f};
};

// GPU layout (must match sdf_scene.frag.metal exactly)

struct alignas(16) GpuSdfNode {
    float p0[4];   // x, y, rx, type_f
    float p1[4];   // ry, corner_r, smooth_k, _pad
    float p2[4];   // r, g, b, blend_f
};
static_assert(sizeof(GpuSdfNode) == 48);

constexpr int kSdfMaxNodes = 10;

struct alignas(16) GpuSdfBlock {
    int node_count = 0;
    int debug_mode = 0;
    int _pad[2]    = {};
    GpuSdfNode nodes[kSdfMaxNodes];
};
static_assert(sizeof(GpuSdfBlock) == 16 + kSdfMaxNodes * 48);

// Panel class

class SdfPanel {
public:
    SdfPanel()  = default;
    ~SdfPanel();

    SdfPanel(const SdfPanel&)            = delete;
    SdfPanel& operator=(const SdfPanel&) = delete;

    /// Call once after sg_setup(), before first draw().
    /// exe_dir is used to locate the shader files under data/shaders/msl/.
    void init(const std::filesystem::path& exe_dir);

    /// Release all GPU resources – call before sg_shutdown().
    void shutdown();

    /// Render the panel window.  visible is toggled by the close button.
    void draw(bool& visible);

    bool ready() const noexcept { return gpu_ready_; }

private:
    // Sokol GPU objects
    sg_shader   shd_          = {SG_INVALID_ID};
    sg_pipeline pip_screen_   = {SG_INVALID_ID};   ///< on-screen pass (for preview)
    sg_pipeline pip_offscr_   = {SG_INVALID_ID};   ///< offscreen pass (into texture)
    sg_buffer   quad_vb_      = {SG_INVALID_ID};
    sg_image    color_img_    = {SG_INVALID_ID};
    sg_view     color_view_   = {SG_INVALID_ID};
    sg_view     sample_view_  = {SG_INVALID_ID};
    int         rt_w_  = 0, rt_h_ = 0;
    bool        gpu_ready_ = false;

    // Scene
    std::vector<SdfNode> nodes_;
    int   selected_  = -1;
    int   debug_mode_= 0;
    float anim_time_ = 0.f;

    // ── Interaction state ─────────────────────────────────────────────────────
    enum class DragMode { None, MoveNode, ResizeNode };
    DragMode  drag_mode_     = DragMode::None;
    int       drag_idx_      = -1;
    float     drag_start_x_  = 0.f, drag_start_y_  = 0.f;   ///< node position at drag start
    float     drag_start_rx_ = 0.f, drag_start_ry_ = 0.f;   ///< node size at drag start
    ImVec2    drag_start_uv_ = {};                            ///< mouse UV at drag start

    // ── Internal ──────────────────────────────────────────────────────────────
    void ensure_pipeline(std::string_view vs, std::string_view fs);
    void ensure_render_target(int w, int h);
    void destroy_pipeline();
    void destroy_render_target();
    void render_to_target(int w, int h);

    GpuSdfBlock pack_nodes() const noexcept;

    void draw_canvas(ImVec2 canvas_pos, ImVec2 canvas_size, bool focused);
    void draw_node_sidebar();
    void draw_node_properties(SdfNode& n);

    static void draw_handles_overlay(ImDrawList* dl,
                                     const std::vector<SdfNode>& nodes,
                                     ImVec2 canvas_pos, ImVec2 canvas_size,
                                     int selected, int hover,
                                     float asp);
    [[nodiscard]] int  hit_test_center(ImVec2 mouse_uv, float asp, float threshold_uv) const;
    [[nodiscard]] int  hit_test_edge  (ImVec2 mouse_uv, float asp, float threshold_uv) const;
    [[nodiscard]] bool build_pipeline (std::string_view vs, std::string_view fs);
};

} // namespace shadebug::panels


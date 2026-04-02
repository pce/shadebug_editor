#pragma once

#include <array>
#include <cstdint>
#include <span>

namespace shadebug::renderer {

// ── Per-instance rect (exactly matches the GPU vertex layout) ─────────────────
//
//  Attribute layout (buffer_index = 1, SG_VERTEXSTEP_PER_INSTANCE):
//
//   attr  0  a_pos       — float2  (from static quad VB, buffer 0)
//   attr  1  a_uv        — float2  (from static quad VB, buffer 0)
//   attr  2  i_rect      — float4  x, y, w, h  (screen pixels)
//   attr  3  i_fill      — float4  r, g, b, a
//   attr  4  i_border    — float4  r, g, b, a
//   attr  5  i_params    — float2  corner_radius, border_width
//   attr  6  i_uv        — float4  uv.x, uv.y, uv.w, uv.h
//
//  Total stride: 72 bytes — no padding needed.
//

struct UiRect {
    float x{}, y{}, w{}, h{};              // screen-space position + size
    float fill[4]       = {1,1,1,1};       // RGBA fill / tint
    float border_col[4] = {0,0,0,0};       // RGBA border colour
    float corner_radius = 0.f;
    float border_width  = 0.f;
    float uv[4]         = {0,0,1,1};       // texture UV rect; (0,0,1,1) = solid
};

static_assert(sizeof(UiRect) == 72, "UiRect must be 72 bytes to match GPU layout");

// ── CPU-side command buffer ────────────────────────────────────────────────────

class DrawCtx {
public:
    static constexpr int kMaxRects = 4096;

    void clear() noexcept { count_ = 0; }

    [[nodiscard]] int  count() const noexcept { return count_; }
    [[nodiscard]] bool full()  const noexcept { return count_ >= kMaxRects; }

    [[nodiscard]] std::span<const UiRect> span() const noexcept {
        return { rects_.data(), static_cast<std::size_t>(count_) };
    }

    // ── Push helpers ──────────────────────────────────────────────────────────

    /// Solid filled rect, optional rounded corners + border.
    void push_rect(float x, float y, float w, float h,
                   float r, float g, float b, float a = 1.f,
                   float corner_radius = 0.f,
                   float border_width  = 0.f,
                   float br = 0.f, float bg = 0.f,
                   float bb = 0.f, float ba = 1.f) noexcept
    {
        if (full()) return;
        rects_[count_++] = UiRect{
            .x = x, .y = y, .w = w, .h = h,
            .fill        = { r, g, b, a },
            .border_col  = { br, bg, bb, ba },
            .corner_radius = corner_radius,
            .border_width  = border_width,
            .uv          = { 0, 0, 1, 1 },
        };
    }

    /// Textured rect with UV sub-region.
    void push_image(float x, float y, float w, float h,
                    float uv_x = 0.f, float uv_y = 0.f,
                    float uv_w = 1.f, float uv_h = 1.f,
                    float tint_r = 1.f, float tint_g = 1.f,
                    float tint_b = 1.f, float tint_a = 1.f) noexcept
    {
        if (full()) return;
        rects_[count_++] = UiRect{
            .x = x, .y = y, .w = w, .h = h,
            .fill = { tint_r, tint_g, tint_b, tint_a },
            .uv   = { uv_x, uv_y, uv_w, uv_h },
        };
    }

    /// Raw push — caller fills the entire UiRect.
    void push(const UiRect& r) noexcept {
        if (!full()) rects_[count_++] = r;
    }

private:
    std::array<UiRect, kMaxRects> rects_{};
    int count_ = 0;
};

} // namespace shadebug::renderer

#pragma once

#include "draw_ctx.hpp"
#include "sokol_gfx.h"
#include <string>
#include <string_view>

namespace shadebug::renderer {

// ── RectUniforms — uploaded to VS each frame ──────────────────────────────────

struct alignas(16) RectUniforms {
    float screen_w;
    float screen_h;
    float _pad[2];
};
static_assert(sizeof(RectUniforms) == 16);

// ── GpuRenderer ───────────────────────────────────────────────────────────────
//
//  Renders a DrawCtx in one instanced draw call.
//
//  Instance buffer strategy:
//    The buffer uses sg_append_buffer (not sg_update_buffer), which allows
//    multiple flush() calls per frame (e.g. swapchain + offscreen passes).
//    The buffer is sized for kMaxFlushesPerFrame × kMaxRects instances.
//    sokol resets the append position automatically at sg_commit() time.
//
//  Usage:
//    r.init();
//    // each frame:
//    r.flush(ctx, screen_w, screen_h);           // swapchain pass
//    r.flush(ctx2, w, h, /*offscreen=*/true);    // offscreen pass
//    r.cleanup();
//

class GpuRenderer {
public:
    /// How many flush() calls can happen per frame before the buffer overflows.
    static constexpr int kMaxFlushesPerFrame = 8;

    GpuRenderer()  = default;
    ~GpuRenderer() = default;

    GpuRenderer(const GpuRenderer&)            = delete;
    GpuRenderer& operator=(const GpuRenderer&) = delete;

    void init()    noexcept;
    void cleanup() noexcept;

    /// Upload instance data and issue one instanced draw call.
    /// Can be called multiple times per frame (uses sg_append_buffer).
    /// Set offscreen=true inside a color-only offscreen pass (no depth attachment).
    void flush(const DrawCtx& ctx, float screen_w, float screen_h,
               bool offscreen = false) noexcept;

    // ── Verbose diagnostics ────────────────────────────────────────────────────
    void set_verbose(bool v) noexcept { verbose_ = v; }
    [[nodiscard]] bool verbose() const noexcept { return verbose_; }

    // ── Hot-reload ────────────────────────────────────────────────────────────

    /// Replace the shader sources and rebuild pipeline.
    /// Returns empty string on success, error message on failure.
    [[nodiscard]] std::string recompile(std::string_view vs_src,
                                        std::string_view fs_src);

    /// Reset shader sources to the built-in defaults.
    void reset_shaders();

    // ── Shader source access (for editor panel) ───────────────────────────────
    [[nodiscard]] const std::string& vs_source()   const noexcept { return vs_src_; }
    [[nodiscard]] const std::string& fs_source()   const noexcept { return fs_src_; }
    [[nodiscard]] const std::string& last_error()  const noexcept { return last_error_; }

    [[nodiscard]] bool valid() const noexcept {
        return pip_.id != SG_INVALID_ID && offscreen_pip_.id != SG_INVALID_ID;
    }

private:
    sg_pipeline pip_          = { SG_INVALID_ID };  // swapchain pipeline (default depth)
    sg_pipeline offscreen_pip_= { SG_INVALID_ID };  // offscreen pipeline (no depth)
    sg_shader   shd_          = { SG_INVALID_ID };
    sg_buffer   quad_vb_      = { SG_INVALID_ID };
    sg_buffer   inst_vb_      = { SG_INVALID_ID };  // stream buffer, append per flush
    sg_image    white_        = { SG_INVALID_ID };  // 1×1 white fallback
    sg_view     white_view_   = { SG_INVALID_ID };  // texture view for white_ (created once)
    sg_sampler  sampler_      = { SG_INVALID_ID };

    std::string vs_src_;
    std::string fs_src_;
    std::string last_error_;
    bool        verbose_ = false;

    [[nodiscard]] static sg_shader   make_shader(std::string_view vs, std::string_view fs);
    /// offscreen=true → depth.pixel_format=SG_PIXELFORMAT_NONE (color-only passes)
    [[nodiscard]] static sg_pipeline make_pipeline(sg_shader shd, bool offscreen = false);
    void destroy_pipeline_and_shader() noexcept;
};

} // namespace shadebug::renderer

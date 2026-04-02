#include "gpu_renderer.hpp"
#include "shaders.hpp"
#include "sokol_log.h"

#include <cstdint>
#include <print>

namespace shadebug::renderer {

// ── Static quad geometry (6 verts, CCW, two triangles) ───────────────────────

struct QuadVertex { float x, y, u, v; };

static constexpr QuadVertex kQuad[6] = {
    {0.f, 0.f,  0.f, 0.f},   // TL
    {1.f, 0.f,  1.f, 0.f},   // TR
    {1.f, 1.f,  1.f, 1.f},   // BR
    {0.f, 0.f,  0.f, 0.f},   // TL
    {1.f, 1.f,  1.f, 1.f},   // BR
    {0.f, 1.f,  0.f, 1.f},   // BL
};

// ── Lifecycle ─────────────────────────────────────────────────────────────────

void GpuRenderer::init() noexcept {
    // Static quad vertex buffer
    quad_vb_ = sg_make_buffer(sg_buffer_desc{
        .size    = sizeof(kQuad),
        .data    = SG_RANGE(kQuad),
        .label   = "rect-quad-vb",
    });

    // Stream instance buffer — sg_append_buffer allows multiple flush() calls
    // per frame. Sized for kMaxFlushesPerFrame × kMaxRects instances; sokol
    // resets the append position automatically after sg_commit().
    inst_vb_ = sg_make_buffer(sg_buffer_desc{
        .size  = static_cast<size_t>(GpuRenderer::kMaxFlushesPerFrame)
               * static_cast<size_t>(DrawCtx::kMaxRects) * sizeof(UiRect),
        .usage = { .dynamic_update = true },
        .label = "rect-instance-vb",
    });

    // 1×1 white fallback texture (used for solid fills, no texture binding needed)
    const uint32_t white = 0xFFFFFFFF;
    white_ = sg_make_image(sg_image_desc{
        .width  = 1,
        .height = 1,
        .data   = { .mip_levels = { [0] = { &white, sizeof(white) } } },
        .label  = "rect-white-1x1",
    });

    // Permanent texture view for the white fallback (not recreated each frame)
    white_view_ = sg_make_view(sg_view_desc{
        .texture = { .image = white_ },
    });

    // Linear-clamp sampler
    sampler_ = sg_make_sampler(sg_sampler_desc{
        .min_filter    = SG_FILTER_LINEAR,
        .mag_filter    = SG_FILTER_LINEAR,
        .mipmap_filter = SG_FILTER_NEAREST,
        .wrap_u        = SG_WRAP_CLAMP_TO_EDGE,
        .wrap_v        = SG_WRAP_CLAMP_TO_EDGE,
        .label         = "rect-sampler",
    });

    // Build initial pipeline from embedded defaults
    reset_shaders();
    std::println("[GpuRenderer] initialised (pipeline: {})",
                 valid() ? "OK" : "FAILED");
}

void GpuRenderer::cleanup() noexcept {
    if (!sg_isvalid()) return;
    destroy_pipeline_and_shader();
    if (white_view_.id != SG_INVALID_ID) { sg_destroy_view(white_view_);   white_view_ = {SG_INVALID_ID}; }
    if (quad_vb_.id  != SG_INVALID_ID) { sg_destroy_buffer(quad_vb_);   quad_vb_  = {SG_INVALID_ID}; }
    if (inst_vb_.id  != SG_INVALID_ID) { sg_destroy_buffer(inst_vb_);   inst_vb_  = {SG_INVALID_ID}; }
    if (white_.id    != SG_INVALID_ID) { sg_destroy_image(white_);       white_    = {SG_INVALID_ID}; }
    if (sampler_.id  != SG_INVALID_ID) { sg_destroy_sampler(sampler_);   sampler_  = {SG_INVALID_ID}; }
}

// ── Per-frame flush ───────────────────────────────────────────────────────────

void GpuRenderer::flush(const DrawCtx& ctx, float screen_w, float screen_h,
                        bool offscreen) noexcept {
    if (!valid()) {
        if (verbose_)
            std::println("[GpuRenderer] flush skipped: pipeline invalid (pass={})",
                         offscreen ? "offscreen" : "swapchain");
        return;
    }
    if (ctx.count() == 0) {
        if (verbose_)
            std::println("[GpuRenderer] flush skipped: 0 rects (pass={})",
                         offscreen ? "offscreen" : "swapchain");
        return;
    }

    const auto sp = ctx.span();

    // sg_append_buffer can be called multiple times per frame on the same
    // buffer (unlike sg_update_buffer which allows only one call per frame).
    // It returns the byte offset to use in vertex_buffer_offsets[].
    const int inst_offset = sg_append_buffer(inst_vb_, sg_range{
        sp.data(),
        sp.size_bytes(),
    });

    if (sg_query_buffer_overflow(inst_vb_)) {
        std::println("[GpuRenderer][ERROR] instance buffer overflow! "
                     "{} rects requested, buffer capacity {}×{} rects.",
                     ctx.count(), kMaxFlushesPerFrame, DrawCtx::kMaxRects);
        return;
    }

    if (verbose_)
        std::println("[GpuRenderer] flush: {} rect(s), pass={}, inst_offset={}",
                     ctx.count(), offscreen ? "offscreen" : "swapchain", inst_offset);

    sg_apply_pipeline(offscreen ? offscreen_pip_ : pip_);

    sg_bindings bindings = {};
    bindings.vertex_buffers[0]        = quad_vb_;
    bindings.vertex_buffers[1]        = inst_vb_;
    bindings.vertex_buffer_offsets[1] = inst_offset;  // offset from sg_append_buffer

    bindings.views[0]    = white_view_;
    bindings.samplers[0] = sampler_;

    sg_apply_bindings(bindings);

    const RectUniforms uniforms{ screen_w, screen_h, 0.f, 0.f };
    sg_apply_uniforms(0, SG_RANGE(uniforms));

    sg_draw(0, 6, ctx.count());
}

// ── Pipeline / shader construction ───────────────────────────────────────────

sg_shader GpuRenderer::make_shader(std::string_view vs, std::string_view fs) {
    sg_shader_desc d = {};
    d.label = "rect-shader";

    // ── Vertex function ───────────────────────────────────────────────────────
    d.vertex_func.source = vs.data();
    d.vertex_func.entry  = "vs_main";

    // ── Fragment function ─────────────────────────────────────────────────────
    d.fragment_func.source = fs.data();
    d.fragment_func.entry  = "fs_main";

    // ── Vertex attribute names (GLSL) / HLSL semantics ────────────────────────
    d.attrs[0] = { .glsl_name = "a_pos",     .hlsl_sem_name = "POSITION", .hlsl_sem_index = 0 };
    d.attrs[1] = { .glsl_name = "a_uv",      .hlsl_sem_name = "TEXCOORD", .hlsl_sem_index = 0 };
    d.attrs[2] = { .glsl_name = "i_rect",    .hlsl_sem_name = "TEXCOORD", .hlsl_sem_index = 1 };
    d.attrs[3] = { .glsl_name = "i_fill",    .hlsl_sem_name = "TEXCOORD", .hlsl_sem_index = 2 };
    d.attrs[4] = { .glsl_name = "i_border",  .hlsl_sem_name = "TEXCOORD", .hlsl_sem_index = 3 };
    d.attrs[5] = { .glsl_name = "i_params",  .hlsl_sem_name = "TEXCOORD", .hlsl_sem_index = 4 };
    d.attrs[6] = { .glsl_name = "i_uv",      .hlsl_sem_name = "TEXCOORD", .hlsl_sem_index = 5 };

    // ── VS uniform block (screen size) ────────────────────────────────────────
    d.uniform_blocks[0] = {
        .stage  = SG_SHADERSTAGE_VERTEX,
        .size   = sizeof(RectUniforms),    // 16 bytes
        .msl_buffer_n         = 2,         // buffers 0,1 used by vertex data
        .hlsl_register_b_n    = 0,
        .glsl_uniforms = {
            [0] = { .type = SG_UNIFORMTYPE_FLOAT2, .glsl_name = "screen" },
        },
    };

    // ── FS texture + sampler ──────────────────────────────────────────────────
    d.views[0].texture = {
        .stage       = SG_SHADERSTAGE_FRAGMENT,
        .image_type  = SG_IMAGETYPE_2D,
        .sample_type = SG_IMAGESAMPLETYPE_FLOAT,
        .msl_texture_n     = 0,
        .hlsl_register_t_n = 0,
    };
    d.samplers[0] = {
        .stage         = SG_SHADERSTAGE_FRAGMENT,
        .sampler_type  = SG_SAMPLERTYPE_FILTERING,
        .msl_sampler_n    = 0,
        .hlsl_register_s_n = 0,
    };
    d.texture_sampler_pairs[0] = {
        .stage        = SG_SHADERSTAGE_FRAGMENT,
        .view_slot    = 0,
        .sampler_slot = 0,
        .glsl_name    = "u_tex",
    };

    return sg_make_shader(&d);
}

sg_pipeline GpuRenderer::make_pipeline(sg_shader shd, bool offscreen) {
    sg_pipeline_desc d = {};
    d.shader = shd;
    d.label  = offscreen ? "rect-pipeline-offscreen" : "rect-pipeline";

    // ── Vertex layout ─────────────────────────────────────────────────────────
    auto& lay = d.layout;

    // Buffer 0 — static quad, per-vertex
    // stride = 2×float2 = 16 bytes; offsets explicit (auto-compute disabled once
    // any attr offset is non-zero, so we must set all of them).
    lay.buffers[0].step_func = SG_VERTEXSTEP_PER_VERTEX;
    lay.buffers[0].stride    = 16;
    lay.attrs[0] = { .buffer_index = 0, .offset =  0, .format = SG_VERTEXFORMAT_FLOAT2 }; // a_pos
    lay.attrs[1] = { .buffer_index = 0, .offset =  8, .format = SG_VERTEXFORMAT_FLOAT2 }; // a_uv

    // Buffer 1 — instance data, per-instance (stride = sizeof(UiRect) = 72)
    // UiRect memory layout (must match):
    //   offset  0: float4  x,y,w,h      → i_rect
    //   offset 16: float4  fill[4]      → i_fill
    //   offset 32: float4  border_col[] → i_border
    //   offset 48: float   corner_radius → i_params.x
    //   offset 52: float   border_width  → i_params.y
    //   offset 56: float4  uv[4]        → i_uv
    lay.buffers[1].step_func = SG_VERTEXSTEP_PER_INSTANCE;
    lay.buffers[1].stride    = sizeof(UiRect);  // 72
    lay.attrs[2] = { .buffer_index = 1, .offset =  0, .format = SG_VERTEXFORMAT_FLOAT4 }; // i_rect
    lay.attrs[3] = { .buffer_index = 1, .offset = 16, .format = SG_VERTEXFORMAT_FLOAT4 }; // i_fill
    lay.attrs[4] = { .buffer_index = 1, .offset = 32, .format = SG_VERTEXFORMAT_FLOAT4 }; // i_border
    lay.attrs[5] = { .buffer_index = 1, .offset = 48, .format = SG_VERTEXFORMAT_FLOAT2 }; // i_params
    lay.attrs[6] = { .buffer_index = 1, .offset = 56, .format = SG_VERTEXFORMAT_FLOAT4 }; // i_uv

    // ── Blending (standard alpha, premultiplied) ──────────────────────────────
    d.colors[0].blend = {
        .enabled          = true,
        .src_factor_rgb   = SG_BLENDFACTOR_SRC_ALPHA,
        .dst_factor_rgb   = SG_BLENDFACTOR_ONE_MINUS_SRC_ALPHA,
        .src_factor_alpha = SG_BLENDFACTOR_ONE,
        .dst_factor_alpha = SG_BLENDFACTOR_ONE_MINUS_SRC_ALPHA,
    };

    // No depth test — painter's order
    d.depth.write_enabled = false;
    d.depth.compare       = SG_COMPAREFUNC_ALWAYS;
    // Offscreen color-only pass has no depth attachment; we must declare
    // SG_PIXELFORMAT_NONE explicitly so sokol validation matches.
    if (offscreen)
        d.depth.pixel_format = SG_PIXELFORMAT_NONE;

    return sg_make_pipeline(&d);
}

void GpuRenderer::destroy_pipeline_and_shader() noexcept {
    if (offscreen_pip_.id != SG_INVALID_ID) { sg_destroy_pipeline(offscreen_pip_); offscreen_pip_ = {SG_INVALID_ID}; }
    if (pip_.id != SG_INVALID_ID) { sg_destroy_pipeline(pip_); pip_ = {SG_INVALID_ID}; }
    if (shd_.id != SG_INVALID_ID) { sg_destroy_shader(shd_);   shd_ = {SG_INVALID_ID}; }
}

// ── Hot-reload ────────────────────────────────────────────────────────────────

std::string GpuRenderer::recompile(std::string_view vs_src, std::string_view fs_src) {
    sg_shader new_shd = make_shader(vs_src, fs_src);
    if (new_shd.id == SG_INVALID_ID) {
        last_error_ = "Shader compilation failed — check sokol log for details.";
        return last_error_;
    }

    sg_pipeline new_pip = make_pipeline(new_shd, false);
    if (new_pip.id == SG_INVALID_ID) {
        sg_destroy_shader(new_shd);
        last_error_ = "Pipeline creation failed — check sokol log for details.";
        return last_error_;
    }

    // Offscreen pipeline reuses the same shader (different depth format only)
    sg_pipeline new_offscreen_pip = make_pipeline(new_shd, true);
    if (new_offscreen_pip.id == SG_INVALID_ID) {
        sg_destroy_pipeline(new_pip);
        sg_destroy_shader(new_shd);
        last_error_ = "Offscreen pipeline creation failed.";
        return last_error_;
    }

    destroy_pipeline_and_shader();
    pip_           = new_pip;
    offscreen_pip_ = new_offscreen_pip;
    shd_           = new_shd;
    vs_src_     = vs_src;
    fs_src_     = fs_src;
    last_error_ = {};
    std::println("[GpuRenderer] recompiled OK");
    return {};
}

void GpuRenderer::reset_shaders() {
    const std::string err = recompile(shaders::vert_source(), shaders::frag_source());
    if (!err.empty())
        std::println("[GpuRenderer] default shader failed: {}", err);
}

} // namespace shadebug::renderer

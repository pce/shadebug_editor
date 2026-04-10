#include "effect_renderer.hpp"
#include "sokol_log.h"

#include <cstdint>
#include <print>

namespace shadebug::renderer {

static constexpr float kEffectQuad[12] = {
    0.f, 0.f,   1.f, 0.f,   1.f, 1.f,
    0.f, 0.f,   1.f, 1.f,   0.f, 1.f,
};

void EffectRenderer::init() noexcept {
    quad_vb_ = sg_make_buffer(sg_buffer_desc{
        .size  = sizeof(kEffectQuad),
        .data  = SG_RANGE(kEffectQuad),
        .label = "effect-quad-vb",
    });
    std::println("[EffectRenderer] initialised (quad buffer ready)");
}

void EffectRenderer::cleanup() noexcept {
    if (!sg_isvalid()) return;
    destroy_pipeline_and_shader();
    if (quad_vb_.id != SG_INVALID_ID) { sg_destroy_buffer(quad_vb_); quad_vb_ = {SG_INVALID_ID}; }
}

void EffectRenderer::flush(float time, float w, float h, bool offscreen) noexcept {
    if (!valid()) return;

    sg_apply_pipeline(offscreen ? offscreen_pip_ : pip_);

    sg_bindings bindings = {};
    bindings.vertex_buffers[0] = quad_vb_;
    sg_apply_bindings(bindings);

    const EffectUniforms u{ w, h, time, 0.f };
    sg_apply_uniforms(0, SG_RANGE(u));
    sg_apply_uniforms(1, SG_RANGE(custom_params_));

    sg_draw(0, 6, 1);
}

sg_shader EffectRenderer::make_shader(std::string_view vs, std::string_view fs) {
    sg_shader_desc d = {};
    d.label = "effect-shader";

    d.vertex_func.source = vs.data();
    d.vertex_func.entry  = "vs_main";
    d.fragment_func.source = fs.data();
    d.fragment_func.entry  = "fs_main";

    d.attrs[0] = { .glsl_name = "a_pos",
                   .hlsl_sem_name = "POSITION", .hlsl_sem_index = 0 };

    d.uniform_blocks[0] = {
        .stage  = SG_SHADERSTAGE_FRAGMENT,
        .size   = sizeof(EffectUniforms),
        .msl_buffer_n      = 0,
        .hlsl_register_b_n = 0,
        .glsl_uniforms = {
            [0] = { .type = SG_UNIFORMTYPE_FLOAT2, .glsl_name = "iResolution" },
            [1] = { .type = SG_UNIFORMTYPE_FLOAT,  .glsl_name = "iTime"       },
        },
    };

    // Block 1: custom shader params (64 bytes = 4×float4).
    // Shaders opt-in by declaring: constant ParamUniforms& p [[buffer(1)]]
    // Shaders that don't declare it silently ignore the bound buffer.
    d.uniform_blocks[1] = {
        .stage  = SG_SHADERSTAGE_FRAGMENT,
        .size   = sizeof(ParamUniforms),
        .msl_buffer_n      = 1,
        .hlsl_register_b_n = 1,
        .glsl_uniforms = {
            [0] = { .type = SG_UNIFORMTYPE_FLOAT4, .glsl_name = "iParams0" },
            [1] = { .type = SG_UNIFORMTYPE_FLOAT4, .glsl_name = "iParams1" },
            [2] = { .type = SG_UNIFORMTYPE_FLOAT4, .glsl_name = "iParams2" },
            [3] = { .type = SG_UNIFORMTYPE_FLOAT4, .glsl_name = "iParams3" },
        },
    };

    return sg_make_shader(&d);
}

sg_pipeline EffectRenderer::make_pipeline(sg_shader shd, bool offscreen) {
    sg_pipeline_desc d = {};
    d.shader = shd;
    d.label  = offscreen ? "effect-pipeline-offscreen" : "effect-pipeline";

    auto& lay = d.layout;
    lay.buffers[0].step_func = SG_VERTEXSTEP_PER_VERTEX;
    lay.buffers[0].stride    = 8;
    lay.attrs[0] = { .buffer_index = 0, .offset = 0,
                     .format = SG_VERTEXFORMAT_FLOAT2 };

    d.colors[0].blend.enabled = false;

    d.depth.write_enabled = false;
    d.depth.compare       = SG_COMPAREFUNC_ALWAYS;
    if (offscreen)
        d.depth.pixel_format = SG_PIXELFORMAT_NONE;

    return sg_make_pipeline(&d);
}

void EffectRenderer::destroy_pipeline_and_shader() noexcept {
    if (offscreen_pip_.id != SG_INVALID_ID) { sg_destroy_pipeline(offscreen_pip_); offscreen_pip_ = {SG_INVALID_ID}; }
    if (pip_.id           != SG_INVALID_ID) { sg_destroy_pipeline(pip_);           pip_           = {SG_INVALID_ID}; }
    if (shd_.id           != SG_INVALID_ID) { sg_destroy_shader(shd_);             shd_           = {SG_INVALID_ID}; }
}

std::string EffectRenderer::recompile(std::string_view vs_src, std::string_view fs_src) {
    sg_shader new_shd = make_shader(vs_src, fs_src);
    if (new_shd.id == SG_INVALID_ID) {
        last_error_ = "Effect shader compilation failed.";
        return last_error_;
    }

    sg_pipeline new_pip = make_pipeline(new_shd, false);
    if (new_pip.id == SG_INVALID_ID) {
        sg_destroy_shader(new_shd);
        last_error_ = "Effect pipeline creation failed.";
        return last_error_;
    }

    sg_pipeline new_offscreen = make_pipeline(new_shd, true);
    if (new_offscreen.id == SG_INVALID_ID) {
        sg_destroy_pipeline(new_pip);
        sg_destroy_shader(new_shd);
        last_error_ = "Effect offscreen pipeline failed.";
        return last_error_;
    }

    destroy_pipeline_and_shader();
    pip_           = new_pip;
    offscreen_pip_ = new_offscreen;
    shd_           = new_shd;
    vs_src_        = vs_src;
    fs_src_        = fs_src;
    last_error_    = {};
    std::println("[EffectRenderer] recompiled OK");
    return {};
}

} // namespace shadebug::renderer

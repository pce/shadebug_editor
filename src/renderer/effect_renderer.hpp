#pragma once

#include "shader_params.hpp"
#include "sokol_gfx.h"
#include <string>
#include <string_view>

namespace shadebug::renderer {

struct alignas(16) EffectUniforms {
    float res_x;
    float res_y;
    float iTime;
    float _pad = 0.f;
};
static_assert(sizeof(EffectUniforms) == 16);

class EffectRenderer {
public:
    EffectRenderer()  = default;
    ~EffectRenderer() = default;

    EffectRenderer(const EffectRenderer&)            = delete;
    EffectRenderer& operator=(const EffectRenderer&) = delete;

    void init()    noexcept;
    void cleanup() noexcept;

    void flush(float time, float w, float h, bool offscreen = false) noexcept;

    [[nodiscard]] std::string recompile(std::string_view vs_src,
                                        std::string_view fs_src);

    /// Upload custom shader params before the next flush().
    void set_custom_params(const ParamUniforms& p) noexcept { custom_params_ = p; }
    [[nodiscard]] const ParamUniforms& custom_params() const noexcept { return custom_params_; }

    [[nodiscard]] bool valid() const noexcept {
        return pip_.id != SG_INVALID_ID && offscreen_pip_.id != SG_INVALID_ID;
    }

    [[nodiscard]] const std::string& last_error() const noexcept { return last_error_; }
    [[nodiscard]] const std::string& vs_source()  const noexcept { return vs_src_; }
    [[nodiscard]] const std::string& fs_source()  const noexcept { return fs_src_; }

private:
    sg_pipeline pip_          = { SG_INVALID_ID };
    sg_pipeline offscreen_pip_= { SG_INVALID_ID };
    sg_shader   shd_          = { SG_INVALID_ID };
    sg_buffer   quad_vb_      = { SG_INVALID_ID };

    std::string vs_src_;
    std::string fs_src_;
    std::string last_error_;

    ParamUniforms custom_params_{};   ///< custom user params – written to block 1

    [[nodiscard]] static sg_shader   make_shader(std::string_view vs, std::string_view fs);
    [[nodiscard]] static sg_pipeline make_pipeline(sg_shader shd, bool offscreen = false);
    void destroy_pipeline_and_shader() noexcept;
};

} // namespace shadebug::renderer

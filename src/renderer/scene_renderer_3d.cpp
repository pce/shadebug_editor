#include "scene_renderer_3d.hpp"
#include "scene3d_shaders.hpp"

#include <cmath>
#include <print>

namespace shadebug::renderer {

// ─ Cube geometry (36 vertices, no index buffer) ──────────────────────────────
//
//  6 faces × 2 triangles × 3 vertices.
//  All faces wound CCW when viewed from outside, for back-face culling.
//  Face colours chosen to be visually distinct.
//

static constexpr Vertex3D kCube[36] = {
    // +X  orange
    {{+1,-1,-1},{1.00f,0.50f,0.00f}},
    {{+1,+1,-1},{1.00f,0.50f,0.00f}},
    {{+1,+1,+1},{1.00f,0.50f,0.00f}},
    {{+1,-1,-1},{1.00f,0.50f,0.00f}},
    {{+1,+1,+1},{1.00f,0.50f,0.00f}},
    {{+1,-1,+1},{1.00f,0.50f,0.00f}},

    // -X  cyan
    {{-1,-1,+1},{0.00f,0.80f,1.00f}},
    {{-1,+1,+1},{0.00f,0.80f,1.00f}},
    {{-1,+1,-1},{0.00f,0.80f,1.00f}},
    {{-1,-1,+1},{0.00f,0.80f,1.00f}},
    {{-1,+1,-1},{0.00f,0.80f,1.00f}},
    {{-1,-1,-1},{0.00f,0.80f,1.00f}},

    // +Y  lime green
    {{-1,+1,-1},{0.20f,0.90f,0.20f}},
    {{-1,+1,+1},{0.20f,0.90f,0.20f}},
    {{+1,+1,+1},{0.20f,0.90f,0.20f}},
    {{-1,+1,-1},{0.20f,0.90f,0.20f}},
    {{+1,+1,+1},{0.20f,0.90f,0.20f}},
    {{+1,+1,-1},{0.20f,0.90f,0.20f}},

    // -Y  magenta
    {{-1,-1,+1},{0.90f,0.20f,0.80f}},
    {{-1,-1,-1},{0.90f,0.20f,0.80f}},
    {{+1,-1,-1},{0.90f,0.20f,0.80f}},
    {{-1,-1,+1},{0.90f,0.20f,0.80f}},
    {{+1,-1,-1},{0.90f,0.20f,0.80f}},
    {{+1,-1,+1},{0.90f,0.20f,0.80f}},

    // +Z  red
    {{-1,-1,+1},{0.90f,0.20f,0.20f}},
    {{+1,-1,+1},{0.90f,0.20f,0.20f}},
    {{+1,+1,+1},{0.90f,0.20f,0.20f}},
    {{-1,-1,+1},{0.90f,0.20f,0.20f}},
    {{+1,+1,+1},{0.90f,0.20f,0.20f}},
    {{-1,+1,+1},{0.90f,0.20f,0.20f}},

    // -Z  blue
    {{+1,-1,-1},{0.20f,0.40f,1.00f}},
    {{-1,-1,-1},{0.20f,0.40f,1.00f}},
    {{-1,+1,-1},{0.20f,0.40f,1.00f}},
    {{+1,-1,-1},{0.20f,0.40f,1.00f}},
    {{-1,+1,-1},{0.20f,0.40f,1.00f}},
    {{+1,+1,-1},{0.20f,0.40f,1.00f}},
};

// ── Lifecycle ──────────────────────────────────────────────────────────────────

SceneRenderer3D::~SceneRenderer3D() {
    if (sg_isvalid()) shutdown();
}

void SceneRenderer3D::shutdown() noexcept {
    if (!sg_isvalid()) return;
    destroy_render_target();
    destroy_pipeline();
}


void SceneRenderer3D::ensure_pipeline() noexcept {
    if (pipeline_ready_) return;

    //  Static cube vertex buffer
    vbuf_ = sg_make_buffer(sg_buffer_desc{
        .size  = sizeof(kCube),
        .data  = SG_RANGE(kCube),
        .label = "scene3d-cube-vb",
    });
    if (vbuf_.id == SG_INVALID_ID) {
        std::println("[SceneRenderer3D] vertex buffer creation failed");
        return;
    }

    // 1×1 white fallback for when no albedo texture is bound
    const uint32_t white_px = 0xFFFFFFFF;
    white_img_ = sg_make_image(sg_image_desc{
        .width  = 1,
        .height = 1,
        .data   = { .mip_levels = { [0] = { &white_px, sizeof(white_px) } } },
        .label  = "scene3d-white-1x1",
    });
    white_view_ = sg_make_view(sg_view_desc{
        .texture = { .image = white_img_ },
    });

    // ── Linear clamp sampler ─────────────────────────────────────────────────
    sampler_ = sg_make_sampler(sg_sampler_desc{
        .min_filter = SG_FILTER_LINEAR,
        .mag_filter = SG_FILTER_LINEAR,
        .wrap_u     = SG_WRAP_CLAMP_TO_EDGE,
        .wrap_v     = SG_WRAP_CLAMP_TO_EDGE,
        .label      = "scene3d-sampler",
    });

    // ── Shader ───────────────────────────────────────────────────────────────
    sg_shader_desc sd = {};
    sd.label                = "scene3d-shader";
    sd.vertex_func.source   = scene3d_shaders::vert_source();
    sd.vertex_func.entry    = "vs_main";
    sd.fragment_func.source = scene3d_shaders::frag_source();
    sd.fragment_func.entry  = "fs_main";

    sd.attrs[0] = { .glsl_name = "a_pos", .hlsl_sem_name = "POSITION", .hlsl_sem_index = 0 };
    sd.attrs[1] = { .glsl_name = "a_col", .hlsl_sem_name = "TEXCOORD", .hlsl_sem_index = 0 };

    sd.uniform_blocks[0] = {
        .stage             = SG_SHADERSTAGE_VERTEX,
        .size              = sizeof(MeshUniforms),
        .msl_buffer_n      = 1,
        .hlsl_register_b_n = 0,
        .glsl_uniforms = {
            [0] = { .type = SG_UNIFORMTYPE_MAT4, .glsl_name = "mvp" },
        },
    };

    // Albedo texture + sampler (Option B)
    sd.views[0].texture = {
        .stage             = SG_SHADERSTAGE_FRAGMENT,
        .image_type        = SG_IMAGETYPE_2D,
        .sample_type       = SG_IMAGESAMPLETYPE_FLOAT,
        .msl_texture_n     = 0,
        .hlsl_register_t_n = 0,
    };
    sd.samplers[0] = {
        .stage              = SG_SHADERSTAGE_FRAGMENT,
        .sampler_type       = SG_SAMPLERTYPE_FILTERING,
        .msl_sampler_n      = 0,
        .hlsl_register_s_n  = 0,
    };
    sd.texture_sampler_pairs[0] = {
        .stage        = SG_SHADERSTAGE_FRAGMENT,
        .view_slot    = 0,
        .sampler_slot = 0,
        .glsl_name    = "u_albedo",
    };

    shd_ = sg_make_shader(&sd);
    if (shd_.id == SG_INVALID_ID) {
        std::println("[SceneRenderer3D] shader compilation failed");
        destroy_pipeline();
        return;
    }

    // ── Pipeline ─────────────────────────────────────────────────────────────
    sg_pipeline_desc pd = {};
    pd.shader = shd_;
    pd.label  = "scene3d-pipeline";

    auto& lay             = pd.layout;
    lay.buffers[0].stride = sizeof(Vertex3D);
    lay.attrs[0] = { .buffer_index = 0, .offset =  0, .format = SG_VERTEXFORMAT_FLOAT3 };
    lay.attrs[1] = { .buffer_index = 0, .offset = 12, .format = SG_VERTEXFORMAT_FLOAT3 };

    pd.depth.pixel_format  = SG_PIXELFORMAT_DEPTH;
    pd.depth.write_enabled = true;
    pd.depth.compare       = SG_COMPAREFUNC_LESS_EQUAL;
    pd.cull_mode           = SG_CULLMODE_BACK;

    pip_ = sg_make_pipeline(&pd);
    if (pip_.id == SG_INVALID_ID) {
        std::println("[SceneRenderer3D] pipeline creation failed");
        destroy_pipeline();
        return;
    }

    pipeline_ready_ = true;
    std::println("[SceneRenderer3D] pipeline ready");
}

void SceneRenderer3D::destroy_pipeline() noexcept {
    if (dyn_vbuf_.id  != SG_INVALID_ID) { sg_destroy_buffer(dyn_vbuf_);  dyn_vbuf_  = {SG_INVALID_ID}; dyn_vbuf_capacity_ = 0; dyn_count_ = 0; }
    if (pip_.id       != SG_INVALID_ID) { sg_destroy_pipeline(pip_);     pip_       = {SG_INVALID_ID}; }
    if (shd_.id       != SG_INVALID_ID) { sg_destroy_shader(shd_);       shd_       = {SG_INVALID_ID}; }
    if (vbuf_.id      != SG_INVALID_ID) { sg_destroy_buffer(vbuf_);      vbuf_      = {SG_INVALID_ID}; }
    if (sampler_.id   != SG_INVALID_ID) { sg_destroy_sampler(sampler_);  sampler_   = {SG_INVALID_ID}; }
    if (white_view_.id!= SG_INVALID_ID) { sg_destroy_view(white_view_);  white_view_= {SG_INVALID_ID}; }
    if (white_img_.id != SG_INVALID_ID) { sg_destroy_image(white_img_);  white_img_ = {SG_INVALID_ID}; }
    pipeline_ready_ = false;
}

// ── Dynamic mesh ───────────────────────────────────────────────────────────────

void SceneRenderer3D::submit_mesh(const Vertex3D* verts, int count) noexcept {
    if (!verts || count <= 0) { dyn_count_ = 0; return; }

    const size_t need = static_cast<size_t>(count) * sizeof(Vertex3D);

    // Grow the dynamic buffer if the current one is too small
    if (dyn_vbuf_.id == SG_INVALID_ID || need > dyn_vbuf_capacity_) {
        if (dyn_vbuf_.id != SG_INVALID_ID) sg_destroy_buffer(dyn_vbuf_);
        dyn_vbuf_ = sg_make_buffer(sg_buffer_desc{
            .size  = need,
            .usage = { .dynamic_update = true },
            .label = "scene3d-dynamic-vb",
        });
        dyn_vbuf_capacity_ = need;
    }

    sg_update_buffer(dyn_vbuf_, sg_range{ verts, need });
    dyn_count_ = count;
}


void SceneRenderer3D::resize(int w, int h) noexcept {
    ensure_pipeline();
    if (!pipeline_ready_) return;
    if (rt_w_ == w && rt_h_ == h && color_img_.id != SG_INVALID_ID) return;
    ensure_render_target(w, h);
}

void SceneRenderer3D::ensure_render_target(int w, int h) noexcept {
    destroy_render_target();

    color_img_ = sg_make_image(sg_image_desc{
        .usage  = { .color_attachment = true },
        .width  = w,
        .height = h,
        .label  = "scene3d-color",
    });
    color_att_ = sg_make_view(sg_view_desc{
        .color_attachment = { .image = color_img_ },
    });
    // Separate texture-sample view for ImGui::Image
    sample_view_ = sg_make_view(sg_view_desc{
        .texture = { .image = color_img_ },
    });

    depth_img_ = sg_make_image(sg_image_desc{
        .usage        = { .depth_stencil_attachment = true },
        .pixel_format = SG_PIXELFORMAT_DEPTH,
        .width        = w,
        .height       = h,
        .label        = "scene3d-depth",
    });
    depth_att_ = sg_make_view(sg_view_desc{
        .depth_stencil_attachment = { .image = depth_img_ },
    });

    rt_w_ = w;
    rt_h_ = h;
    std::println("[SceneRenderer3D] render target {}×{}", w, h);
}

void SceneRenderer3D::destroy_render_target() noexcept {
    if (sample_view_.id != SG_INVALID_ID) { sg_destroy_view(sample_view_);  sample_view_ = {SG_INVALID_ID}; }
    if (color_att_.id   != SG_INVALID_ID) { sg_destroy_view(color_att_);    color_att_   = {SG_INVALID_ID}; }
    if (color_img_.id   != SG_INVALID_ID) { sg_destroy_image(color_img_);   color_img_   = {SG_INVALID_ID}; }
    if (depth_att_.id   != SG_INVALID_ID) { sg_destroy_view(depth_att_);    depth_att_   = {SG_INVALID_ID}; }
    if (depth_img_.id   != SG_INVALID_ID) { sg_destroy_image(depth_img_);   depth_img_   = {SG_INVALID_ID}; }
    rt_w_ = rt_h_ = 0;
}


void SceneRenderer3D::render(float /*anim_time*/, float azimuth, float elevation,
                              float zoom, float model_angle) noexcept {
    if (!valid()) return;

    // ── Camera (spherical orbit) ──────────────────────────────────────────────
    const float cos_el = std::cos(elevation);
    const float ex = zoom * std::sin(azimuth)  * cos_el;
    const float ey = zoom * std::sin(elevation);
    const float ez = zoom * std::cos(azimuth)  * cos_el;

    const float aspect = rt_h_ > 0
                       ? static_cast<float>(rt_w_) / static_cast<float>(rt_h_)
                       : 1.f;

    const Mat4 proj  = Mat4::perspective(0.7854f /* 45° */, aspect, 0.1f, 100.f);
    const Mat4 view  = Mat4::look_at(ex, ey, ez, 0.f, 0.f, 0.f);
    const Mat4 model = Mat4::rotate_y(model_angle);
    const Mat4 mvp   = proj * view * model;

    MeshUniforms u{};
    for (int i = 0; i < 16; ++i) u.mvp[i] = mvp.m[i];

    // ── Offscreen pass ────────────────────────────────────────────────────────
    sg_pass_action pa = {};
    pa.colors[0].load_action = SG_LOADACTION_CLEAR;
    pa.colors[0].clear_value = { 0.07f, 0.07f, 0.12f, 1.f };
    pa.depth.load_action     = SG_LOADACTION_CLEAR;
    pa.depth.clear_value     = 1.f;

    sg_attachments atts = {};
    atts.colors[0]     = color_att_;
    atts.depth_stencil = depth_att_;

    sg_begin_pass(sg_pass{ .action = pa, .attachments = atts });
    sg_apply_pipeline(pip_);

    // Vertex buffer: dynamic mesh if submitted this frame, otherwise static cube
    const bool use_dyn = (dyn_count_ > 0 && dyn_vbuf_.id != SG_INVALID_ID);
    sg_bindings bindings = {};
    bindings.vertex_buffers[0] = use_dyn ? dyn_vbuf_ : vbuf_;

    // Albedo: user-supplied texture or white fallback (= pure vertex colour)
    const sg_view effective_albedo =
        (albedo_view_.id != SG_INVALID_ID) ? albedo_view_ : white_view_;
    bindings.views[0]    = effective_albedo;
    bindings.samplers[0] = sampler_;

    sg_apply_bindings(bindings);
    sg_apply_uniforms(0, SG_RANGE(u));
    sg_draw(0, use_dyn ? dyn_count_ : 36, 1);

    sg_end_pass();

    // Reset dynamic mesh for next frame
    dyn_count_ = 0;
}

} // namespace shadebug::renderer


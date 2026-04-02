#pragma once

#include "sokol_gfx.h"
#include <cmath>

namespace shadebug::renderer {

//  Minimal column-major 4×4 matrix
//
//  Storage: m[col * 4 + row]   (column-major, matches Metal float4x4 / GLSL mat4)
//
//  Multiply: (A * B) transforms B first, then A — standard MVP convention.
//

struct Mat4 {
    float m[16] = {};

    [[nodiscard]] float  at(int row, int col) const noexcept { return m[col * 4 + row]; }
    float& at(int row, int col)               noexcept       { return m[col * 4 + row]; }

    [[nodiscard]] static Mat4 identity() noexcept {
        Mat4 r;
        r.at(0,0) = r.at(1,1) = r.at(2,2) = r.at(3,3) = 1.f;
        return r;
    }

    [[nodiscard]] Mat4 operator*(const Mat4& b) const noexcept {
        Mat4 r;
        for (int row = 0; row < 4; ++row)
            for (int col = 0; col < 4; ++col) {
                float s = 0.f;
                for (int k = 0; k < 4; ++k) s += at(row, k) * b.at(k, col);
                r.at(row, col) = s;
            }
        return r;
    }

    /// Perspective projection — right-handed.
    /// Depth range [0,1] for Metal / D3D / WebGPU, [-1,1] for OpenGL.
    [[nodiscard]] static Mat4 perspective(float fov_y, float aspect,
                                          float near_z, float far_z) noexcept {
        const float f = 1.f / std::tan(fov_y * 0.5f);
        Mat4 r;
        r.at(0,0) = f / aspect;
        r.at(1,1) = f;
#if defined(SOKOL_METAL) || defined(SOKOL_D3D11) || defined(SOKOL_WGPU)
        // Depth [0, 1]
        r.at(2,2) = far_z  / (near_z - far_z);
        r.at(2,3) = (far_z * near_z) / (near_z - far_z);
#else
        // Depth [-1, 1]  (OpenGL)
        r.at(2,2) = -(far_z + near_z) / (far_z - near_z);
        r.at(2,3) = -(2.f * far_z * near_z) / (far_z - near_z);
#endif
        r.at(3,2) = -1.f;
        return r;
    }

    /// Standard right-handed look-at view matrix (matches GLM lookAt).
    [[nodiscard]] static Mat4 look_at(float ex, float ey, float ez,
                                       float tx, float ty, float tz,
                                       float wx = 0.f, float wy = 1.f,
                                       float wz = 0.f) noexcept {
        // f = normalize(target - eye)
        float fx = tx - ex, fy = ty - ey, fz = tz - ez;
        float fl = std::sqrt(fx*fx + fy*fy + fz*fz);
        if (fl < 1e-6f) fl = 1e-6f;
        fx /= fl; fy /= fl; fz /= fl;

        // s = normalize(f × worldUp)
        float sx = fy*wz - fz*wy, sy = fz*wx - fx*wz, sz = fx*wy - fy*wx;
        float sl = std::sqrt(sx*sx + sy*sy + sz*sz);
        if (sl < 1e-6f) sl = 1e-6f;
        sx /= sl; sy /= sl; sz /= sl;

        // u = s × f  (recomputed up, already unit length)
        const float ux = sy*fz - sz*fy;
        const float uy = sz*fx - sx*fz;
        const float uz = sx*fy - sy*fx;

        Mat4 r;
        r.at(0,0)= sx; r.at(0,1)= sy; r.at(0,2)= sz;
        r.at(1,0)= ux; r.at(1,1)= uy; r.at(1,2)= uz;
        r.at(2,0)=-fx; r.at(2,1)=-fy; r.at(2,2)=-fz;
        r.at(0,3)= -(sx*ex + sy*ey + sz*ez);
        r.at(1,3)= -(ux*ex + uy*ey + uz*ez);
        r.at(2,3)=  (fx*ex + fy*ey + fz*ez);
        r.at(3,3)= 1.f;
        return r;
    }

    /// Rotation around the Y axis (yaw).
    [[nodiscard]] static Mat4 rotate_y(float angle) noexcept {
        const float c = std::cos(angle), s = std::sin(angle);
        Mat4 r = identity();
        r.at(0,0) =  c;  r.at(0,2) = s;
        r.at(2,0) = -s;  r.at(2,2) = c;
        return r;
    }

    /// Rotation around the X axis (pitch).
    [[nodiscard]] static Mat4 rotate_x(float angle) noexcept {
        const float c = std::cos(angle), s = std::sin(angle);
        Mat4 r = identity();
        r.at(1,1) =  c;  r.at(1,2) = -s;
        r.at(2,1) =  s;  r.at(2,2) =  c;
        return r;
    }
};

//  Per-vertex data

struct alignas(4) Vertex3D {
    float pos[3];   ///< world-space position (x, y, z)
    float col[3];   ///< linear RGB colour
};
static_assert(sizeof(Vertex3D) == 24, "Vertex3D stride must be 24");

//  VS uniform block (64 bytes)

struct alignas(16) MeshUniforms {
    float mvp[16];  ///< column-major MVP matrix
};
static_assert(sizeof(MeshUniforms) == 64);

//  SceneRenderer3D
//
//  Self-contained offscreen 3D renderer.
//
//  Usage per frame (inside ShaderRenderPanel::draw):
//
//      scene3d_.resize(vp_w, vp_h);                           // lazy init
//      scene3d_.render(anim_time, azimuth, elevation, zoom);  // draw cube
//      ImGui::Image(simgui_imtextureid(scene3d_.sample_view()), avail);
//
//  The renderer owns its own colour + depth render target so it never
//  interferes with the existing 2D / effect pipelines.
//

class SceneRenderer3D {
public:
    SceneRenderer3D()  = default;
    ~SceneRenderer3D();

    SceneRenderer3D(const SceneRenderer3D&)            = delete;
    SceneRenderer3D& operator=(const SceneRenderer3D&) = delete;


    /// Recreate the offscreen render target when vp dimensions change.
    /// Lazily initialises the GPU pipeline on first call.
    void resize(int w, int h) noexcept;

    /// Render one orbit-camera frame.
    ///   MVP = proj × view × model   where model = rotate_y(model_angle).
    void render(float anim_time, float azimuth, float elevation,
                float zoom, float model_angle = 0.f) noexcept;

    //  Call submit_mesh() BEFORE render() each frame to replace the built-in
    //  cube with arbitrary geometry.  Reset per-frame (cube used if not called).
    //
    //  Physics integration pattern:
    //    physics.step(dt);
    //    std::vector<Vertex3D> verts;  physics.write_verts(verts);
    //    scene3d_.submit_mesh(verts.data(), (int)verts.size());
    //    scene3d_.render(...);

    void submit_mesh(const Vertex3D* verts, int count) noexcept;

    //  Option B: SDF / custom albedo texture
    //
    //  Bind any sg_view as the mesh's albedo texture (screen-space projection).
    //  Pass sg_view{SG_INVALID_ID} to revert to the 1×1 white fallback
    //  (pure vertex colour).
    //
    //  Example — paint the SDF 2D panel onto the 3D mesh:
    //    scene3d_.set_albedo(sdf_render_panel.sample_view());

    void set_albedo(sg_view v) noexcept { albedo_view_ = v; }


    void shutdown() noexcept;


    [[nodiscard]] sg_view sample_view() const noexcept { return sample_view_; }
    [[nodiscard]] sg_view color_att()   const noexcept { return color_att_;   }

    [[nodiscard]] bool valid() const noexcept {
        return pip_.id != SG_INVALID_ID && color_img_.id != SG_INVALID_ID;
    }

private:
    sg_image  color_img_   = {SG_INVALID_ID};
    sg_image  depth_img_   = {SG_INVALID_ID};
    sg_view   color_att_   = {SG_INVALID_ID};
    sg_view   depth_att_   = {SG_INVALID_ID};
    sg_view   sample_view_ = {SG_INVALID_ID};
    int       rt_w_  = 0, rt_h_ = 0;

    sg_pipeline pip_  = {SG_INVALID_ID};
    sg_shader   shd_  = {SG_INVALID_ID};
    sg_buffer   vbuf_ = {SG_INVALID_ID};   ///< static cube VB
    bool        pipeline_ready_ = false;

    sg_buffer   dyn_vbuf_          = {SG_INVALID_ID};
    size_t      dyn_vbuf_capacity_ = 0;
    int         dyn_count_         = 0;    ///< >0 means use dynamic mesh this frame

    sg_image    white_img_   = {SG_INVALID_ID};   ///< 1×1 white fallback
    sg_view     white_view_  = {SG_INVALID_ID};
    sg_sampler  sampler_     = {SG_INVALID_ID};
    sg_view     albedo_view_ = {SG_INVALID_ID};   ///< optional user albedo

    void ensure_pipeline()              noexcept;
    void ensure_render_target(int w, int h) noexcept;
    void destroy_render_target()        noexcept;
    void destroy_pipeline()             noexcept;
};

} // namespace shadebug::renderer


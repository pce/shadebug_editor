#pragma once

#include <cstdint>

#include "sokol_gfx.h"

namespace shadebug {

struct KernelData {
    static constexpr uint32_t MaxSize = 7;
    static constexpr uint32_t MaxValues = MaxSize * MaxSize;

    uint32_t size = 3;
    float values[MaxValues] = {};
    float normalization = 1.0f;
};

class ConvolutionPass {
public:
    bool init();
    void shutdown();

    // Recreate the output storage image when the viewport changes.
    bool resize(int width, int height);

    // Apply the kernel:
    //
    //   input -> output
    //
    // The input and output must not refer to the same image.
    void apply(
        sg_view input,
        sg_view output,
        int width,
        int height,
        const KernelData& kernel
    );

    sg_pipeline pipeline() const {
        return _pipeline;
    }

private:
    sg_shader   _shader = {};
    sg_pipeline _pipeline = {};

    // GPU-side kernel parameters.
    sg_buffer _kernel_buffer = {};

    bool _initialized = false;
    int _width = 0;
    int _height = 0;
};

} // namespace shadebug
// draft

kernel void convolution(
    texture2d<float, access::read> input  [[texture(0)]],
    texture2d<float, access::write> output [[texture(1)]],
    constant KernelParams& params          [[buffer(0)]],
    uint2 gid                              [[thread_position_in_grid]]
)


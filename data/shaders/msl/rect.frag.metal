#include <metal_stdlib>
using namespace metal;

struct Varyings {
    float4 pos    [[position]];
    float2 local;
    float2 size;
    float4 fill;
    float4 border;
    float2 params;
    float2 uv;
};

float sdf_rrect(float2 p, float2 half_size, float r) {
    float2 q = abs(p) - half_size + r;
    return min(max(q.x, q.y), 0.0) + length(max(q, float2(0.0))) - r;
}

fragment float4 fs_main(Varyings in [[stage_in]],
                         texture2d<float> tex [[texture(0)]],
                         sampler smp [[sampler(0)]])
{
    float cr     = in.params.x;
    float bw     = in.params.y;
    float2 hs  = in.size * 0.5;
    float2 p     = (in.local - 0.5) * in.size;
    float d      = sdf_rrect(p, hs, max(cr, 0.001));
    float aa     = length(float2(dfdx(d), dfdy(d)));
    float outer  = 1.0 - smoothstep(-aa, aa, d);

    float4 tex_col  = tex.sample(smp, in.uv);
    float4 fill_col = in.fill * tex_col;

    float4 color;
    if (bw > 0.0) {
        float inner = 1.0 - smoothstep(-aa, aa, d + bw);
        color = mix(in.border, fill_col, inner);
    } else {
        color = fill_col;
    }
    color.a *= outer;
    return color;
}

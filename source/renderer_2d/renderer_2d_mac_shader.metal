
#include <metal_stdlib>

using namespace metal;

#include "renderer_2d_mac_shared_types.h"

struct MAC_Renderer2DRasteriserData
{
    float4 position [[position]];
    float2 uv;
    float2 dst_pos;
    float2 dst_half_size;
    float2 dst_centre;
    float4 fill_colour;
    float4 stroke_colour;
    float stroke_width;
    float corner_radius;
    float edge_softness;
};

vertex MAC_Renderer2DRasteriserData
vs(uint vid [[vertex_id]],
             uint iid [[instance_id]],
             constant MAC_Renderer2DInstance *instances [[buffer(MAC_Renderer2DVertexInputIndex_Instances)]],
             constant MAC_Renderer2DVertex *vertices [[buffer(MAC_Renderer2DVertexInputIndex_Vertices)]],
             constant vector_uint2 *viewport_size_pointer [[buffer(MAC_Renderer2DVertexInputIndex_ViewportSize)]])
{
    MAC_Renderer2DRasteriserData out;

    float2 dst_half_size = (instances[iid].dst_max.xy - instances[iid].dst_min.xy) / 2.0;
    float2 dst_centre = (instances[iid].dst_max.xy + instances[iid].dst_min.xy) / 2.0;
    float2 dst_pos = dst_centre + vertices[vid].position*dst_half_size;

    float2 src_half_size = (instances[iid].src_max.xy - instances[iid].src_min.xy) / 2.0;
    float2 src_centre = (instances[iid].src_max.xy + instances[iid].src_min.xy) / 2.0;
    float2 src_pos = src_centre + vertices[vid].position*src_half_size;

    float2 viewport_size = float2(*viewport_size_pointer);
    
    float4 vert = float4(2.0*dst_pos.x / viewport_size.x - 1, 1 - 2.0*dst_pos.y / viewport_size.y, 0.0, 1.0);

    out.position = vert;
    out.uv = src_pos;
    out.dst_pos = dst_pos;
    out.dst_half_size = dst_half_size;
    out.dst_centre = dst_centre;
    out.fill_colour = instances[iid].fill_colour;
    out.stroke_colour = instances[iid].stroke_colour;
    out.stroke_width = instances[iid].stroke_width;
    out.corner_radius = instances[iid].corner_radius;
    out.edge_softness = instances[iid].edge_softness;

    return out;
}

float
RoundedRectSDF(float2 sample_pos,
               float2 rect_center,
               float2 rect_half_size,
               float r)
{
    float2 d2 = (abs(rect_center - sample_pos) -
                 rect_half_size +
                 float2(r, r));
    return min(max(d2.x, d2.y), 0.0) + length(max(d2, 0.0)) - r;
}

fragment float4
ps(MAC_Renderer2DRasteriserData in [[stage_in]],
   texture2d<float> texture [[ texture(MAC_Renderer2DFragmentTextureIndex_Colour) ]])
{
    float dist = RoundedRectSDF(in.dst_pos, in.dst_centre, in.dst_half_size, in.corner_radius);
    
    float sdf_factor = 1.0 - smoothstep(-2.0*in.edge_softness, 0.0, dist);
    float stroke_factor = 0;
    if(in.stroke_width != 0.0)
    {
        float2 interior_half_size = in.dst_half_size - in.stroke_width;
        
        float interior_radius_reduce_f = min(interior_half_size.x / in.dst_half_size.x,
                                             interior_half_size.y / in.dst_half_size.y);
        float interior_corner_radius = (in.corner_radius*interior_radius_reduce_f);
        
        float inside_d = RoundedRectSDF(in.dst_pos,
                                        in.dst_centre,
                                        interior_half_size,
                                        interior_corner_radius);
        float inside_f = smoothstep(-2.0*in.edge_softness, 0.0, inside_d);
        
        stroke_factor = inside_f;
    }
    
    float4 colour = mix(in.fill_colour, in.stroke_colour, stroke_factor);

    constexpr sampler texture_sampler (mag_filter::linear, min_filter::linear);
    const float4 texture_sample = texture.sample(texture_sampler, in.uv);

    return texture_sample*colour*sdf_factor;
}


struct VS_Input
{
    float2 dst_min          : dst_min;
    float2 dst_max          : dst_max;
    float2 src_min          : src_min;
    float2 src_max          : src_max;
    float2 mask_min         : mask_min;
    float2 mask_max         : mask_max;
    float4 fill_colour      : fill_colour;
    float4 stroke_colour    : stroke_colour;
    float stroke_width      : stroke_width;
    float corner_radius     : corner_radius;
    float edge_softness     : edge_softness;
    uint vertex_id          : SV_VertexID;
};

struct PS_Input
{
    float4 vertex        : SV_Position;
    float2 mask_min      : mask_min;
    float2 mask_max      : mask_max;
    float2 uv            : uv;
    float2 dst_pos       : dst_pos;
    float2 dst_half_size : dst_half_size;
    float2 dst_center    : dst_center;
    float4 fill_colour   : fill_colour;
    float4 stroke_colour : stroke_colour;
    float stroke_width   : stroke_width;
    float corner_radius  : corner_radius;
    float edge_softness  : edge_softness;
};

cbuffer cbuffer0 : register(b0)
{
    float2 resolution;
};

sampler sampler0 : register(s0);
Texture2D<float4> texture0 : register(t0);

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

PS_Input
vs(VS_Input input)
{
    PS_Input output;
    
    static float2 vertices[] =
    {
        {-1, -1},
        {-1, +1},
        {+1, -1},
        {+1, +1},
    };
    
    float2 dst_half_size = (input.dst_max - input.dst_min) / 2;
    float2 dst_center = (input.dst_max + input.dst_min) / 2;
    float2 dst_pos = vertices[input.vertex_id]*dst_half_size + dst_center;
    
    float2 src_half_size = (input.src_max - input.src_min) / 2;
    float2 src_center = (input.src_max + input.src_min) / 2;
    float2 src_pos = vertices[input.vertex_id]*src_half_size + src_center;
    
    output.vertex = float4(+2*dst_pos.x / resolution.x - 1,
                           -2*dst_pos.y / resolution.y + 1,
                           0, 1);
    output.mask_min = input.mask_min;
    output.mask_max = input.mask_max;
    output.uv = src_pos;
    output.dst_center = dst_center;
    output.dst_half_size = dst_half_size;
    output.dst_pos = dst_pos;
    output.fill_colour = input.fill_colour;
    output.stroke_colour = input.stroke_colour;
    output.stroke_width = input.stroke_width;
    output.corner_radius = input.corner_radius;
    output.edge_softness = input.edge_softness;
    
    return output;
}

float4
ps(PS_Input input) : SV_TARGET
{
    if(input.dst_pos.x < input.mask_min.x || input.dst_pos.x > input.mask_max.x ||
       input.dst_pos.y < input.mask_min.y || input.dst_pos.y > input.mask_max.y)
    {
        discard;
    }
    
    float2 softness_padding = float2(max(0, input.edge_softness*2 - 1), max(0, input.edge_softness*2 - 1));
    float dist = RoundedRectSDF(input.dst_pos, input.dst_center, input.dst_half_size - softness_padding, input.corner_radius);
    float sdf_factor = 1 - smoothstep(0, 2*input.edge_softness, dist);
    float stroke_factor = 0;
    if(input.stroke_width != 0)
    {
        float2 interior_half_size = input.dst_half_size - float2(input.stroke_width, input.stroke_width);
        
        float interior_radius_reduce_f = min(interior_half_size.x / input.dst_half_size.x,
                                             interior_half_size.y / input.dst_half_size.y);
        float interior_corner_radius = (input.corner_radius*interior_radius_reduce_f);
        
        float inside_d = RoundedRectSDF(input.dst_pos,
                                        input.dst_center,
                                        interior_half_size-softness_padding,
                                        interior_corner_radius);
        float inside_f = smoothstep(0, 2*input.edge_softness, inside_d);
        
        stroke_factor = inside_f;
    }
    
    float4 tex = input.fill_colour.a > 0 ? texture0.Sample(sampler0, input.uv) : float4(1.0f, 1.0f, 1.0f, 1.0f);
    
    float4 colour = lerp(tex*abs(input.fill_colour), input.stroke_colour, stroke_factor);
    
    float4 result = colour*sdf_factor;
    return result;
}

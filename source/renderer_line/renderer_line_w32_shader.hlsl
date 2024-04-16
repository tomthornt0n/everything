struct VS_Input
{
    float2 a_position : a_position;
    float4 a_colour   : a_colour;
    float  a_width    : a_width;
    float2 b_position : b_position;
    float4 b_colour   : b_colour;
    float  b_width    : b_width;
    uint vertex_id    : SV_VertexID;
};

struct PS_Input
{
    float4 vertex     : SV_Position;
    
    float2 dst_position : dst_position;
    
    float2 a_position : a_position;
    float4 a_colour   : a_colour;
    float  a_radius   : a_radius;
    float2 b_position : b_position;
    float4 b_colour   : b_colour;
    float  b_radius   : b_radius;
};

cbuffer cbuffer0 : register(b0)
{
    float2 resolution;
};

float cro(float2 a, float2 b) { return a.x*b.y - a.y*b.x; }

float
UnevenCapsuleSDF(float2 sample_position,
                 float2 a_position,
                 float2 b_position,
                 float a_radius,
                 float b_radius)
{
    float2 p = sample_position - a_position;
    float2 pb = b_position - a_position;
    float h = dot(pb, pb);
    
    float2 q = float2(abs(dot(p, float2(pb.y, -pb.x))), dot(p, pb)) / h;
    
    float b = a_radius - b_radius;
    float2 c = float2(sqrt(h - b*b), b);
    
    float k = cro(c, q);
    float m = dot(c, q);
    float n = dot(q, q);
    
    if     (k < 0.0) return sqrt(h*(n)) - a_radius;
    else if(k > c.x) return sqrt(h*(n + 1.0 - 2.0*q.y)) - b_radius;
    else             return m - a_radius;
}

PS_Input
vs(VS_Input input)
{
    PS_Input output;
    
    static float2 padding = float2(4.0f, 4.0f);
    float a_radius = input.a_width / 2;
    float b_radius = input.b_width / 2;
    float2 dst_min = min(input.a_position - a_radius, input.b_position - b_radius) - padding;
    float2 dst_max = max(input.a_position + a_radius, input.b_position + b_radius) + padding;
    
    static float2 vertices[] =
    {
        {-1, -1},
        {-1, +1},
        {+1, -1},
        {+1, +1},
    };
    
    float2 dst_half_size = (dst_max - dst_min) / 2;
    float2 dst_center = (dst_max + dst_min) / 2;
    float2 dst_position = vertices[input.vertex_id]*dst_half_size + dst_center;
    
    output.vertex = float4(+2*dst_position.x / resolution.x - 1,
                           -2*dst_position.y / resolution.y + 1,
                           0, 1);
    output.dst_position = dst_position;
    output.a_position = input.a_position;
    output.b_position = input.b_position;
    output.a_colour = input.a_colour;
    output.b_colour = input.b_colour;
    output.a_radius = a_radius;
    output.b_radius = b_radius;
    
    return output;
}

float4
ps(PS_Input input) : SV_TARGET
{
    float dist = UnevenCapsuleSDF(input.dst_position, input.a_position, input.b_position, input.a_radius, input.b_radius);
    float sdf_factor = 1 - smoothstep(0, 2, dist);
    
    float2 a_to_dst = input.dst_position - input.a_position;
    float2 a_to_b = input.b_position - input.a_position;
    float l = length(a_to_b);
    float t = dot(a_to_dst, a_to_b) / (l*l);
    float4 colour = lerp(input.a_colour, input.b_colour, t);
    
    float4 result = colour*sdf_factor;
    
    return result;
}

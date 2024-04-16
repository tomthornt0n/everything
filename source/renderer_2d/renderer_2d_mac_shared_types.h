
#ifndef R2D_MAC_SHARED_TYPES_H
#define R2D_MAC_SHARED_TYPES_H

#include <simd/simd.h>

typedef enum
{
    MAC_Renderer2DVertexInputIndex_Vertices     = 0,
    MAC_Renderer2DVertexInputIndex_Instances    = 1,
    MAC_Renderer2DVertexInputIndex_ViewportSize = 2,
} MAC_Renderer2DVertexInputIndex;

typedef enum
{
    MAC_Renderer2DFragmentTextureIndex_Colour = 0,
} MAC_Renderer2DFragmentTextureIndex; 

typedef struct MAC_Renderer2DVertex MAC_Renderer2DVertex;
struct MAC_Renderer2DVertex
{
    vector_float2 position;
};

typedef struct MAC_Renderer2DInstance MAC_Renderer2DInstance;
struct MAC_Renderer2DInstance
{
    vector_float2 dst_min;
    vector_float2 dst_max;
    vector_float2 src_min;
    vector_float2 src_max;
    vector_float4 fill_colour;
    vector_float4 stroke_colour;
    float stroke_width;
    float corner_radius;
    float edge_softness;
};

#endif

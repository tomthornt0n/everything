
#if !defined(RENDERER_2D_INCL_H)
#define RENDERER_2D_INCL_H

//~NOTE(tbt): types

typedef struct R2D_Quad R2D_Quad;
struct R2D_Quad
{
    Range2F dst;
    Range2F src;
    Range2F mask;
    V4F fill_colour;
    V4F stroke_colour;
    F32 stroke_width;
    F32 corner_radius;
    F32 edge_softness;
};
#define R2D_Quad(...) ((R2D_Quad){ __VA_ARGS__ })

enum { R2D_QuadListCap = 4096, };

typedef struct R2D_QuadList R2D_QuadList;
struct R2D_QuadList
{
    R2D_Quad *quads;
    U64 cap;
    U64 count;
};

//~NOTE(tbt): functions

// NOTE(tbt): initialisation and cleanup
Function void R2D_Init (void);
Function void R2D_Cleanup (void);

// NOTE(tbt): textures
Function OpaqueHandle R2D_TextureAlloc (Pixel *data, V2I dimensions);
Function void R2D_TextureRelease (OpaqueHandle texture);
Function OpaqueHandle R2D_TextureNil (void);
Function B32 R2D_TextureIsNil (OpaqueHandle texture);
Function void R2D_TexturePixelsSet (OpaqueHandle texture, Pixel *data);
Function V2I R2D_TextureDimensionsGet (OpaqueHandle texture);
Function Range2F R2D_TextureSrcRegionFromPixelCoordinates (OpaqueHandle texture, Range2F region);

// NOTE(tbt): quad lists
Function R2D_QuadList R2D_PushQuadList (Arena *arena, U64 cap);
Function B32 R2D_QuadListAppend (R2D_QuadList *list, R2D_Quad quad);
Function void R2D_QuadListClear (R2D_QuadList *list);
Function void R2D_QuadListSubmit (OpaqueHandle window, R2D_QuadList list, OpaqueHandle texture);

//~NOTE(tbt): globals

Global Range2F r2d_entire_texture = { .min = { 0.0f, 0.0f }, .max = { 1.0f, 1.0f } };

#endif

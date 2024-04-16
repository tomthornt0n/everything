
//~NOTE(tbt): sprites

Function B32
R2D_TextureIsNil(OpaqueHandle texture)
{
    B32 result = (0 == texture.p || R2D_TextureNil().p == texture.p);
    return result;
}

Function Range2F
R2D_TextureSrcRegionFromPixelCoordinates(OpaqueHandle texture, Range2F region)
{
    V2I dimensions_i = R2D_TextureDimensionsGet(texture);
    V2F dimensions_f = V2F(dimensions_i.x, dimensions_i.y);
    Range2F result =
    {
        .min = Div2F(region.min, dimensions_f),
        .max = Div2F(region.max, dimensions_f),
    };
    return result;
}

//~NOTE(tbt): quad lists

Function R2D_QuadList
R2D_PushQuadList(Arena *arena, U64 cap)
{
    R2D_QuadList result =
    {
        .cap = Min1U64(cap, R2D_QuadListCap),
    };
    result.quads = PushArray(arena, R2D_Quad, result.cap);
    return result;
}

Function B32
R2D_QuadListAppend(R2D_QuadList *list, R2D_Quad quad)
{
    B32 result = False;
    if(list->count < list->cap)
    {
        MemoryCopy(&list->quads[list->count], &quad, sizeof(quad));
        list->count += 1;
        result = True;
    }
    return result;
}

Function void
R2D_QuadListClear(R2D_QuadList *list)
{
    list->count = 0;
}

//~NOTE(tbt): platform specific backends

#if BuildOSWindows
# include "renderer_2d_w32.c"
#elif BuildOSMac
# include "renderer_2d_mac.m"
#else
# error "no renderer 2d backend for current platform"
#endif

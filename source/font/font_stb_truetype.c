
#define STBTT_STATIC
#define STB_TRUETYPE_IMPLEMENTATION
#include "external/stb_truetype.h"

// NOTE(tbt): stb_truetype doesn't produce very good small glphs - clamp min size to rasterise and just use gpu downsampling with bilinear filtering for smaller sizes
enum
{
    STB_TRUETYPE_MinSize = 16,
};

Function I32
STB_TRUETYPE_ClampSize(I32 size)
{
    I32 clamped_size = size;
    while(0 != clamped_size && clamped_size < STB_TRUETYPE_MinSize)
    {
        clamped_size *= 2;
    }
    return clamped_size;
}

typedef struct STB_TRUETYPE_FontProvider STB_TRUETYPE_FontProvider;
struct STB_TRUETYPE_FontProvider
{
    stbtt_fontinfo font_info;
    S8 ttf_data;
};

Function OpaqueHandle
FONT_ProviderFromTTF(Arena *arena, S8 ttf_data)
{
    STB_TRUETYPE_FontProvider *provider = PushArray(arena, STB_TRUETYPE_FontProvider, 1);
    provider->ttf_data = ttf_data;
    stbtt_InitFont(&provider->font_info, (const unsigned char *)provider->ttf_data.buffer, 0);
    OpaqueHandle result = { .p = IntFromPtr(provider), };
    return result;
}

Function U32
FONT_ProviderGlyphIndexFromCodepoint(OpaqueHandle provider, U64 codepoint)
{
    STB_TRUETYPE_FontProvider *p = PtrFromInt(provider.p);
    U32 result = stbtt_FindGlyphIndex(&p->font_info, codepoint);
    return result;
}

Function FONT_GlyphMetrics
FONT_ProviderGlyphMetricsFromGlyphIndex(OpaqueHandle provider, U32 glyph_index, I32 size)
{
    FONT_GlyphMetrics result = {0};
    
    STB_TRUETYPE_FontProvider *p = PtrFromInt(provider.p);
    
    I32 clamped_size = STB_TRUETYPE_ClampSize(size);
    
    F32 clamped_scale = stbtt_ScaleForPixelHeight(&p->font_info, clamped_size);
    
    I32 src_ix_0;
    I32 src_ix_1;
    I32 src_iy_0;
    I32 src_iy_1;
    stbtt_GetGlyphBitmapBox(&p->font_info, glyph_index,
                            clamped_scale, clamped_scale,
                            &src_ix_0, &src_iy_0, &src_ix_1, &src_iy_1);
    
    I32 advance;
    I32 left_side_bearing;
    stbtt_GetGlyphHMetrics(&p->font_info, glyph_index, &advance, &left_side_bearing);
    
    result.src_dimensions = V2I(src_ix_1 - src_ix_0, src_iy_1 - src_iy_0);
    result.dst_dimensions = V2I((result.src_dimensions.x*size) / clamped_size,
                                (result.src_dimensions.y*size) / clamped_size);
    result.bearing = V2F((src_ix_0*size) / clamped_size,
                         (src_iy_0*size) / clamped_size);
    result.advance = (advance*clamped_scale*size) / clamped_size;
    
    return result;
}

Function F32
FONT_ProviderVerticalAdvanceFromSize(OpaqueHandle provider, I32 size)
{
    STB_TRUETYPE_FontProvider *p = PtrFromInt(provider.p);
    
    F32 scale = stbtt_ScaleForPixelHeight(&p->font_info, size);
    
    I32 ascent;
    I32 descent;
    I32 line_gap;
    stbtt_GetFontVMetrics(&p->font_info, &ascent, &descent, &line_gap);
    
    F32 result = size + scale*line_gap;
    return result;
}

Function void
FONT_ProviderBitmapFromGlyphIndexAndSize(OpaqueHandle provider, void *pixels, V2I dimensions, U32 glyph_index, I32 size)
{
    STB_TRUETYPE_FontProvider *p = PtrFromInt(provider.p);
    
    I32 clamped_size = STB_TRUETYPE_ClampSize(size);
    
    F32 clamped_scale = stbtt_ScaleForPixelHeight(&p->font_info, clamped_size);
    
    stbtt_MakeGlyphBitmap(&p->font_info,
                          pixels,
                          dimensions.x,
                          dimensions.y,
                          dimensions.x,
                          clamped_scale,
                          clamped_scale,
                          glyph_index);
}

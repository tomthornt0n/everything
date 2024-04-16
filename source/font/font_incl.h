
#if !defined(FONT_INCL_H)
#define FONT_INCL_H

// TODO(tbt): kerning
// TODO(tbt): sub pixel rendering

//~NOTE(tbt): build script macros

#if !defined(BuildFontProviderSTBTruetype)
# define BuildFontProviderSTBTruetype 0
#endif

//~NOTE(tbt): font provider abstaction

typedef struct FONT_GlyphMetrics FONT_GlyphMetrics;
struct FONT_GlyphMetrics
{
    V2I dst_dimensions;
    V2I src_dimensions;
    V2F bearing;
    F32 advance;
};

Function OpaqueHandle FONT_ProviderFromTTF (Arena *arena, S8 ttf_data);
Function U32 FONT_ProviderGlyphIndexFromCodepoint (OpaqueHandle provider, U64 codepoint);
Function FONT_GlyphMetrics FONT_ProviderGlyphMetricsFromGlyphIndex (OpaqueHandle provider, U32 glyph_index, I32 size);
Function F32 FONT_ProviderVerticalAdvanceFromSize (OpaqueHandle provider, I32 size);
Function void FONT_ProviderBitmapFromGlyphIndexAndSize (OpaqueHandle provider, void *pixels, V2I dimensions, U32 glyph_index, I32 size);

//~NOTE(tbt): font cache

// NOTE(tbt): the parameters which identify a rasterised glyph
// could later be expanded to include more features, such as a sub-pixel shift
typedef struct FONT_GlyphId FONT_GlyphId;
struct FONT_GlyphId
{
    OpaqueHandle provider;
    U32 index;
    U32 size;
};

typedef struct FONT_CacheGlyph FONT_CacheGlyph;
struct FONT_CacheGlyph
{
    FONT_CacheGlyph *next;
    FONT_CacheGlyph *next_hash;
    FONT_GlyphId key;
    FONT_GlyphMetrics metrics;
    Range2F atlas_region;
    U8 *pixels;
};

enum
{
    FONT_CacheBucketsCapBits = 10,
    FONT_CacheBucketsCap = Bit(FONT_CacheBucketsCapBits),
};

typedef struct FONT_Cache FONT_Cache;
struct FONT_Cache
{
    Arena *arena;
    OpaqueHandle texture;
    B32 is_dirty;
    U64 square_size_in_pixels;
    FONT_CacheGlyph *buckets[FONT_CacheBucketsCap];
    FONT_CacheGlyph *glyphs;
    U64 glyphs_count;
};

Function I32 FONT_CacheSortComparatorGlyphHeight (const void *a, const void *b, void *user_data);

Function U64 FONT_GlyphIdIsNil (FONT_GlyphId glyph_id);
Function U64 FONT_GlyphIdMatch (FONT_GlyphId a, FONT_GlyphId b);
Function U64 FONT_CacheBucketIndexFromGlyphId (FONT_GlyphId glyph_id);
Function FONT_CacheGlyph *FONT_CacheGlyphWithIdFromChain (FONT_CacheGlyph *chain, FONT_GlyphId glyph_id);
Function FONT_Cache *FONT_PushCache (Arena *arena);
Function FONT_CacheGlyph *FONT_CacheGlyphFromGlyphId (FONT_Cache *cache, FONT_GlyphId glyph_id);
Function OpaqueHandle FONT_Renderer2DTextureFromCache (FONT_Cache *cache);

Global FONT_Cache *font_cache = 0;
Function void FONT_Init (void);

//~NOTE(tbt): rendering

typedef struct FONT_PreparedTextGlyph FONT_PreparedTextGlyph;
struct FONT_PreparedTextGlyph
{
    FONT_PreparedTextGlyph *next;
    Range2F bounds;
    U64 char_index;
    U32 codepoint;
    U32 glyph_index;
};

typedef struct FONT_PreparedTextLine FONT_PreparedTextLine;
struct FONT_PreparedTextLine
{
    FONT_PreparedTextLine *next;
    FONT_PreparedTextLine *prev;
    F32 base_line;
    Range2F bounds;
    FONT_PreparedTextGlyph *glyphs;
    U64 glyphs_count;
};

typedef struct FONT_PreparedText FONT_PreparedText;
struct FONT_PreparedText
{
    OpaqueHandle provider;
    I32 size;
    
    Range2F bounds;
    FONT_PreparedTextLine *lines;
    U64 lines_count;
    U64 glyphs_count;
};

typedef struct FONT_PreparedTextFreeList FONT_PreparedTextFreeList;
struct FONT_PreparedTextFreeList
{
    FONT_PreparedTextGlyph *glyphs;
    FONT_PreparedTextLine *lines;
};

Function FONT_PreparedTextLine *FONT_PreparedTextLineAlloc (Arena *arena, FONT_PreparedTextFreeList *free_list);
Function FONT_PreparedTextGlyph *FONT_PreparedTextGlyphAlloc (Arena *arena, FONT_PreparedTextFreeList *free_list);
Function void FONT_PreparedTextRelease (FONT_PreparedTextFreeList *free_list, FONT_PreparedText *pt);

// TODO(tbt): for completeness sake, should probably have FONT_PrepareS16, ...
Function FONT_PreparedText FONT_PrepareS8 (Arena *arena, FONT_PreparedTextFreeList *free_list, OpaqueHandle provider, I32 size, S8 string);

Function U64 FONT_PreparedTextNearestIndexFromPoint (FONT_PreparedText *pt, V2F text_position, V2F point);
Function FONT_PreparedTextGlyph *FONT_PreparedTextGlyphFromIndex (FONT_PreparedText *pt, U64 index, FONT_PreparedTextLine **line_result);

// NOTE(tbt): MUST CALL FONT_Renderer2DTextureFromCache FIRST AND THEN NOT AGAIN BEFORE SUBMITTING THE QUAD LIST TO ENSURE ATLAS SRC REGIONS ARE UP TO DATE!!!
Function void FONT_Renderer2DQuadsFromPreparedText(R2D_QuadList *result, V2F position, V4F colour, FONT_PreparedText *pt, Range2F mask);

#endif

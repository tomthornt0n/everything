
// TODO(tbt): right now I just do bilinear filtering for subpixel glyph positions - this is pretty terrible
//            glyphs should be rasterised per subpixel position, and the quad used for rendering should always be aligned with the pixel grid

//~NOTE(tbt): font provider abstraction

#if BuildFontProviderSTBTruetype
# include "font_stb_truetype.c"
#else
# error "no font provider specified"
#endif

//~NOTE(tbt): font cache

Function I32
FONT_CacheSortComparatorGlyphHeight(const void *a, const void *b, void *user_data)
{
    const FONT_CacheGlyph *const *a_indirect = a;
    const FONT_CacheGlyph *const *b_indirect = b;
    I32 a_height = Round1F((*a_indirect)->metrics.dst_dimensions.y);
    I32 b_height = Round1F((*b_indirect)->metrics.dst_dimensions.y);
    I32 result = a_height - b_height;
    return result;
}

Function FONT_Cache *
FONT_PushCache(Arena *arena)
{
    ArenaTemp scratch = OS_TCtxScratchBegin(&arena, 1);
    
    U64 initial_size = 256;
    
    FONT_Cache *result = PushArray(arena, FONT_Cache, 1);
    result->square_size_in_pixels = initial_size;
    result->texture = R2D_TextureNil();
    result->arena = arena;
    
    ArenaTempEnd(scratch);
    
    return result;
}

Function U64
FONT_CacheBucketIndexFromGlyphId(FONT_GlyphId glyph_id)
{
    U64 hash[2];
    MurmurHash3_x64_128(&glyph_id, sizeof(glyph_id), 0, hash);
    
    U64 result = (hash[0]*11400714819323198485llu) >> (64 - FONT_CacheBucketsCapBits);
    return result;
}

Function U64
FONT_GlyphIdMatch(FONT_GlyphId a, FONT_GlyphId b)
{
    B32 result = MemoryMatch(&a, &b, sizeof(FONT_GlyphId));
    return result;
}

Function U64
FONT_GlyphIdIsNil(FONT_GlyphId glyph_id)
{
    B32 result = FONT_GlyphIdMatch(glyph_id, (FONT_GlyphId){0});
    return result;
}

Function FONT_CacheGlyph *
FONT_CacheGlyphWithIdFromChain(FONT_CacheGlyph *chain, FONT_GlyphId glyph_id)
{
    FONT_CacheGlyph *glyph = chain;
    while(0 != glyph && !FONT_GlyphIdIsNil(glyph->key) && !FONT_GlyphIdMatch(glyph_id, glyph->key))
    {
        glyph = glyph->next_hash;
    }
    return glyph;
}

Function FONT_CacheGlyph *
FONT_CacheGlyphFromGlyphId(FONT_Cache *cache, FONT_GlyphId glyph_id)
{
    Arena *arena = cache->arena;
    
    // NOTE(tbt): lookup glyph, or allocate a new one if necessary
    U64 bucket_index = FONT_CacheBucketIndexFromGlyphId(glyph_id);
    FONT_CacheGlyph *glyph = FONT_CacheGlyphWithIdFromChain(cache->buckets[bucket_index], glyph_id);
    if(0 == glyph)
    {
        glyph = PushArray(arena, FONT_CacheGlyph, 1);
        glyph->next_hash = cache->buckets[bucket_index];
        cache->buckets[bucket_index]= glyph;
    }
    
    // NOTE(tbt): insert new glyph if necessary
    if(FONT_GlyphIdIsNil(glyph->key))
    {
        cache->is_dirty = True;
        
        cache->glyphs_count += 1;
        
        // NOTE(tbt): populate hash map slot
        glyph->key = glyph_id;
        glyph->metrics = FONT_ProviderGlyphMetricsFromGlyphIndex(glyph_id.provider, glyph_id.index, glyph_id.size);
        glyph->pixels = ArenaPushNoZero(arena, glyph->metrics.src_dimensions.x*glyph->metrics.src_dimensions.y, _Alignof(U8));
        FONT_ProviderBitmapFromGlyphIndexAndSize(glyph_id.provider, glyph->pixels, glyph->metrics.src_dimensions, glyph_id.index, glyph_id.size);
        
        // NOTE(tbt): insert into list
        glyph->next = cache->glyphs;
        cache->glyphs = glyph;
    }
    
    return glyph;
}

Function OpaqueHandle
FONT_Renderer2DTextureFromCache(FONT_Cache *cache)
{
    if(cache->is_dirty)
    {
        ArenaTemp scratch = OS_TCtxScratchBegin(0, 0);
        
        // NOTE(tbt): sort list by height
        U64 glyph_count = 0;
        FONT_CacheGlyph **glyphs_indirect = PushArray(scratch.arena, FONT_CacheGlyph *, cache->glyphs_count);
        for(FONT_CacheGlyph *glyph = cache->glyphs; 0 != glyph; glyph = glyph->next)
        {
            glyphs_indirect[glyph_count] = glyph;
            glyph_count += 1;
        }
        Sort(glyphs_indirect, glyph_count, sizeof(FONT_CacheGlyph *), FONT_CacheSortComparatorGlyphHeight, 0);
        cache->glyphs = 0;
        for(U64 i = 0; i < glyph_count; i += 1)
        {
            glyphs_indirect[i]->next = cache->glyphs;
            cache->glyphs = glyphs_indirect[i];
        }
        ArenaTempEnd(scratch);
        
        // NOTE(tbt): rect pack (this is just pretty naieve row packing rn)
        I32 padding_pixels = 2;
        B32 did_pack = False;
        while(!did_pack)
        {
            did_pack = True;
            
            V2F cur = V2F(0.0f, 0.0f);
            F32 max_height_in_row = 0.0f;
            for(FONT_CacheGlyph *glyph = cache->glyphs; 0 != glyph; glyph = glyph->next)
            {
                V2I src_dimensions = Add2I(glyph->metrics.src_dimensions, U2I(padding_pixels*2));
                
                if(cur.x + src_dimensions.x >= cache->square_size_in_pixels)
                {
                    cur.x = 0.0f;
                    cur.y += max_height_in_row;
                    max_height_in_row = 0.0f;
                }
                
                if(cur.y + src_dimensions.y > cache->square_size_in_pixels)
                {
                    did_pack = False;
                    R2D_TextureRelease(cache->texture);
                    cache->texture = R2D_TextureNil();
                    cache->square_size_in_pixels *= 2;
                    
                    break;
                }
                
                glyph->atlas_region = RectMake2F(Add2F(cur, U2F(padding_pixels)), V2F(glyph->metrics.src_dimensions.x, glyph->metrics.src_dimensions.y));
                
                cur.x += src_dimensions.x;
                max_height_in_row = Max1F(max_height_in_row, src_dimensions.y);
            }
            ArenaTempEnd(scratch);
        }
        
        // NOTE(tbt): build texture
        Pixel *pixels = PushArray(scratch.arena, Pixel, cache->square_size_in_pixels*cache->square_size_in_pixels);
        for(FONT_CacheGlyph *glyph = cache->glyphs; 0 != glyph; glyph = glyph->next)
        {
            for(U64 y = 0; y < glyph->metrics.src_dimensions.y; y += 1)
            {
                for(U64 x = 0; x < glyph->metrics.src_dimensions.x; x += 1)
                {
                    U64 dst_index = (glyph->atlas_region.min.x + x) + (glyph->atlas_region.min.y + y)*cache->square_size_in_pixels;
                    U64 src_index = x + y*glyph->metrics.src_dimensions.x;
                    pixels[dst_index].r = glyph->pixels[src_index];
                    pixels[dst_index].g = glyph->pixels[src_index];
                    pixels[dst_index].b = glyph->pixels[src_index];
                    pixels[dst_index].a = glyph->pixels[src_index];
                }
            }
        }
        
        // NOTE(tbt): upload texture
        if(R2D_TextureIsNil(cache->texture))
        {
            cache->texture = R2D_TextureAlloc(pixels, U2I(cache->square_size_in_pixels));
        }
        else
        {
            R2D_TexturePixelsSet(cache->texture, pixels);
        }
        
        ArenaTempEnd(scratch);
        
        cache->is_dirty = False;
    }
    
    return cache->texture;
}

Function void
FONT_Init(void)
{
    font_cache = FONT_PushCache(OS_TCtxGet()->permanent_arena);
}

//~NOTE(tbt): rendering

Function FONT_PreparedTextLine *
FONT_PreparedTextLineAlloc(Arena *arena, FONT_PreparedTextFreeList *free_list)
{
    FONT_PreparedTextLine *result = 0;
    if(0 == free_list)
    {
        result = PushArray(arena, FONT_PreparedTextLine, 1);
    }
    else if(0 == free_list->lines)
    {
        result = PushArray(arena, FONT_PreparedTextLine, 1);
    }
    else
    {
        result = free_list->lines;
        free_list->lines = free_list->lines->next;
        MemorySet(result, 0, sizeof(*result));
    }
    return result;
}

Function FONT_PreparedTextGlyph *
FONT_PreparedTextGlyphAlloc(Arena *arena, FONT_PreparedTextFreeList *free_list)
{
    FONT_PreparedTextGlyph *result = 0;
    if(0 == free_list)
    {
        result = PushArray(arena, FONT_PreparedTextGlyph, 1);
    }
    else if(0 == free_list->glyphs)
    {
        result = PushArray(arena, FONT_PreparedTextGlyph, 1);
    }
    else
    {
        result = free_list->glyphs;
        free_list->glyphs = free_list->glyphs->next;
        MemorySet(result, 0, sizeof(*result));
    }
    return result;
}

Function void
FONT_PreparedTextRelease(FONT_PreparedTextFreeList *free_list, FONT_PreparedText *pt)
{
    FONT_PreparedTextLine *next_line = 0;
    for(FONT_PreparedTextLine *line = pt->lines; 0 != line; line = next_line)
    {
        next_line = line->next;
        line->next = free_list->lines;
        free_list->lines = line;
        
        FONT_PreparedTextGlyph *next_glyph = 0;
        for(FONT_PreparedTextGlyph *glyph = line->glyphs; 0 != glyph; glyph = next_glyph)
        {
            next_glyph = glyph->next;
            glyph->next = free_list->glyphs;
            free_list->glyphs = glyph;
        }
    }
}

Function FONT_PreparedText
FONT_PrepareS8(Arena *arena, FONT_PreparedTextFreeList *free_list, OpaqueHandle provider, I32 size, S8 string)
{
    FONT_PreparedText result =
    {
        .provider = provider,
        .size = size,
    };
    FONT_PreparedTextLine **last_line = &result.lines;
    FONT_PreparedTextLine *prev_line = 0;
    
    U64 char_index = 0;
    
    // NOTE(tbt): state for layout
    V2F cur = V2F(0.0f);
    F32 v_advance = FONT_ProviderVerticalAdvanceFromSize(provider, size);
    
    // NOTE(tbt): split into lines
    S8Split split = S8SplitMake(string, S8("\n"), 0);
    while(S8SplitNext(&split))
    {
        // NOTE(tbt): allocate and append line
        FONT_PreparedTextLine *line = FONT_PreparedTextLineAlloc(arena, free_list);
        line->base_line = cur.y;
        line->bounds = Range2F(cur, cur);
        line->prev = prev_line;
        *last_line = line;
        last_line = &line->next;
        result.lines_count += 1;
        prev_line = line;
        
        FONT_PreparedTextGlyph **last_glyph = &line->glyphs;
        
        // NOTE(tbt): iterate codepoints
        UTFConsume consume = {0};
        for(U64 i = 0; i < split.current.len; i += consume.advance)
        {
            consume = CodepointFromUTF8(split.current, i);
            U64 codepoint = consume.codepoint;
            U32 gi = FONT_ProviderGlyphIndexFromCodepoint(provider, codepoint);
            
            // NOTE(tbt): get glyph metrics
            FONT_GlyphMetrics metrics = {0};
            if(CharIsSpace(codepoint))
            {
                metrics = FONT_ProviderGlyphMetricsFromGlyphIndex(provider, gi, size);
            }
            else
            {
                FONT_GlyphId key =
                {
                    .index = gi,
                    .size = size,
                    .provider = provider,
                };
                FONT_CacheGlyph *glyph = FONT_CacheGlyphFromGlyphId(font_cache, key);
                metrics = glyph->metrics;
            }
            
            // NOTE(tbt): allocate and append glyph
            FONT_PreparedTextGlyph *g = FONT_PreparedTextGlyphAlloc(arena, free_list);
            g->char_index = char_index;
            g->codepoint = codepoint;
            g->glyph_index = gi;
            *last_glyph = g;
            last_glyph = &g->next;
            line->glyphs_count += 1;
            
            // NOTE(tbt): get glyph rect
            V2F glyph_dimensions = V2F(metrics.dst_dimensions.x, metrics.dst_dimensions.y);
            V2F bearing = V2F(metrics.bearing.x, metrics.bearing.y);
            g->bounds = RectMake2F(Add2F(Round2F(cur), bearing), glyph_dimensions);
            
            // NOTE(tbt): update line bounds
            line->bounds = Bounds2F(line->bounds, g->bounds);
            
            // NOTE(tbt): advance layout
            cur.x += metrics.advance;
            
            char_index += 1;
        }
        
        // NOTE(tbt): allocate and append newline glyph
        FONT_PreparedTextGlyph *g = FONT_PreparedTextGlyphAlloc(arena, free_list);
        g->char_index = char_index;
        g->codepoint = '\n';
        g->bounds = Range2F(cur, cur);
        *last_glyph = g;
        last_glyph = &g->next;
        line->glyphs_count += 1;
        
        // NOTE(tbt): advance vertically and reset x-coordinate to start of line
        cur.x = 0.0f;
        cur.y += v_advance;
        
        // NOTE(tbt): update bounds
        result.bounds = Bounds2F(result.bounds, line->bounds);
        
        char_index += 1;
    }
    
    result.glyphs_count = char_index;
    
    return result;
}

Function U64
FONT_PreparedTextNearestIndexFromPoint(FONT_PreparedText *pt, V2F text_position, V2F point)
{
    U64 result = 0;
    
    V2F p = Sub2F(point, text_position);
    
    if(0 != pt->lines)
    {
        FONT_PreparedTextLine *nearest_line = 0;
        F32 nearest_line_dist = Infinity;
        for(FONT_PreparedTextLine *line = pt->lines; 0 != line; line = line->next)
        {
            V2F line_centre = Centre2F(line->bounds);
            F32 d = Abs1F(line_centre.y - p.y);
            if(d < nearest_line_dist)
            {
                nearest_line = line;
                nearest_line_dist = d;
            }
        }
        Assert(0 != nearest_line);
        
        FONT_PreparedTextGlyph *nearest_glyph = 0;
        F32 nearest_glyph_dist = Infinity;
        for(FONT_PreparedTextGlyph *glyph = nearest_line->glyphs; 0 != glyph; glyph = glyph->next)
        {
            V2F glyph_centre = Centre2F(glyph->bounds);
            F32 d = Abs1F(glyph_centre.x - p.x);
            if(d < nearest_glyph_dist)
            {
                nearest_glyph = glyph;
                nearest_glyph_dist = d;
            }
        }
        Assert(0 != nearest_glyph);
        
        result = nearest_glyph->char_index;
        
        if(p.x > pt->bounds.max.x)
        {
            result += 1;
        }
    }
    
    return result;
}

Function FONT_PreparedTextGlyph *
FONT_PreparedTextGlyphFromIndex(FONT_PreparedText *pt, U64 index, FONT_PreparedTextLine **line_result)
{
    FONT_PreparedTextGlyph *result = 0;
    
    U64 char_index = 0;
    for(FONT_PreparedTextLine *line = pt->lines; 0 != line && 0 == result && char_index <= index; line = line->next)
    {
        if(index <= char_index + line->glyphs_count + 1)
        {
            for(FONT_PreparedTextGlyph *glyph = line->glyphs; 0 != glyph && 0 == result; glyph = glyph->next)
            {
                if(index == glyph->char_index)
                {
                    result = glyph;
                    if(0 != line_result)
                    {
                        *line_result = line;
                    }
                }
                char_index += 1;
            }
        }
        else
        {
            char_index += line->glyphs_count;
        }
    }
    
    return result;
}

Function void
FONT_Renderer2DQuadsFromPreparedText(R2D_QuadList *result, V2F position, V4F colour, FONT_PreparedText *pt, Range2F mask)
{
    for(FONT_PreparedTextLine *line = pt->lines; 0 != line; line = line->next)
    {
        for(FONT_PreparedTextGlyph *glyph = line->glyphs; 0 != glyph; glyph = glyph->next)
        {
            if(!CharIsSpace(glyph->codepoint))
            {
                FONT_GlyphId key =
                {
                    .index = glyph->glyph_index,
                    .size = pt->size,
                    .provider = pt->provider,
                };
                U64 bucket_index = FONT_CacheBucketIndexFromGlyphId(key);
                FONT_CacheGlyph *cache_glyph = FONT_CacheGlyphWithIdFromChain(font_cache->buckets[bucket_index], key);
                if(0 != cache_glyph)
                {
                    R2D_Quad quad =
                    {
                        .dst = Range2F(Add2F(glyph->bounds.min, position), Add2F(glyph->bounds.max, position)),
                        .src.v = Div4F(cache_glyph->atlas_region.v, U4F(font_cache->square_size_in_pixels)),
                        .fill_colour = colour,
                        .mask = mask,
                    };
                    R2D_QuadListAppend(result, quad);
                }
            }
        }
    }
}

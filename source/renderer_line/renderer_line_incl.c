
//~ NOTE(tbt): functions

Function Range2F
LINE_BoundsFromSegment (LINE_Segment segment)
{
    Range2F result = {0};
    
    if(segment.a.pos.x < segment.b.pos.x)
    {
        result.min.x = segment.a.pos.x - segment.a.width;
        result.max.x = segment.b.pos.x + segment.b.width;
    }
    else
    {
        result.min.x = segment.b.pos.x - segment.b.width;
        result.max.x = segment.a.pos.x + segment.a.width;
    }
    
    if(segment.a.pos.y < segment.b.pos.y)
    {
        result.min.y = segment.a.pos.y - segment.a.width;
        result.max.y = segment.b.pos.y + segment.b.width;
    }
    else
    {
        result.min.y = segment.b.pos.y - segment.b.width;
        result.max.y = segment.a.pos.y + segment.a.width;
    }
    
    return result;
}

Function LINE_SegmentList
LINE_PushSegmentList(Arena *arena, U64 cap)
{
    LINE_SegmentList result =
    {
        .cap = Min1U64(cap, LINE_SegmentListCap),
    };
    result.segments = PushArray(arena, LINE_Segment, result.cap);
    return result;
}

Function B32
LINE_SegmentListAppend(LINE_SegmentList *list, LINE_Segment segment)
{
    if(list->count < list->cap && LengthSquared2F(Sub2F(segment.a.pos, segment.b.pos)) > 1.0f)
    {
        MemoryCopy(&list->segments[list->count], &segment, sizeof(segment));
        list->count += 1;
    }
    return (list->count <= list->cap);
}

Function void
LINE_SegmentListClear(LINE_SegmentList *list)
{
    list->count = 0;
}

//~NOTE(tbt): platform specific backends

#if BuildOSWindows
# include "renderer_line_w32.c"
#else
# error "no renderer line backend for current platform"
#endif

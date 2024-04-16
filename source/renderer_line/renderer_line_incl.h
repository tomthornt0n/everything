
#if !defined(RENDERER_LINE_INCL_H)
#define RENDERER_LINE_INCL_H

//~NOTE(tbt): types

typedef struct LINE_Point LINE_Point;
struct LINE_Point
{
    V2F pos;
    V4F col;
    F32 width;
};

typedef struct LINE_Segment LINE_Segment;
struct LINE_Segment
{
    LINE_Point a;
    LINE_Point b;
};

enum { LINE_SegmentListCap = 4096, };

typedef struct LINE_SegmentList LINE_SegmentList;
struct LINE_SegmentList
{
    LINE_Segment *segments;
    U64 cap;
    U64 count;
};

//~NOTE(tbt): functions

Function void LINE_Init    (void);
Function void LINE_Cleanup (void);

Function Range2F LINE_BoundsFromSegment (LINE_Segment segment);

Function LINE_SegmentList LINE_PushSegmentList (Arena *arena, U64 cap);
Function B32 LINE_SegmentListAppend (LINE_SegmentList *list, LINE_Segment segment);
Function void LINE_SegmentListClear (LINE_SegmentList *list);
Function void LINE_SegmentListSubmit (OpaqueHandle window, LINE_SegmentList list, Range2F mask);

#endif

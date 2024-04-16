
typedef struct PTBD_IndexCoord PTBD_IndexCoord;
struct PTBD_IndexCoord
{
    U64 record;
    U64 index;
};

typedef struct PTBD_IndexRecord PTBD_IndexRecord;
struct PTBD_IndexRecord
{
    U64 key;
    U64 indices_count;
    U64 *indices;
};

typedef struct PTBD_Index PTBD_Index;
struct PTBD_Index
{
    PTBD_IndexRecord *records;
    U64 records_count;
};

Function PTBD_IndexCoord PTBD_IndexNextFromCoord (PTBD_Index *index, PTBD_IndexCoord coord);
Function PTBD_IndexCoord PTBD_IndexPrevFromCoord (PTBD_Index *index, PTBD_IndexCoord coord);
Function U64             PTBD_IndexCoordLookup   (PTBD_Index *index, PTBD_IndexCoord coord);

Function PTBD_Index PTBD_IndexMakeForTimelineHistogram  (Arena *arena, DB_Cache data, S8 search);
Function PTBD_Index PTBD_IndexMakeForInvoicingHistogram (Arena *arena, DB_Cache timesheets, DB_Cache jobs, S8 search);
Function PTBD_Index PTBD_IndexMakeForBreakdownList      (Arena *arena, DB_Cache timesheets, DB_Cache data, U64 link_member_offset, S8 search, B32 reverse);


typedef struct PTBD_Histogram PTBD_Histogram;
struct PTBD_Histogram
{
    V2F scroll;
    F32 height_per_row;
    F32 width_per_hour;
    F32 leader_column_width;
    I64 keyboard_focus_id;
    I64 last_selected_id;
    Range2F selection_rect;
    B32 is_dragging_selection_rect;
};

typedef enum
{
    // NOTE(tbt): these two are mutually exclusive
    PTBD_BuildHistogramLeaderColumnFlags_DateMode           = Bit(0),
    PTBD_BuildHistogramLeaderColumnFlags_JobMode            = Bit(1),
    
    PTBD_BuildHistogramLeaderColumnFlags_NewTimesheetButton = Bit(2),
} PTBD_BuildHistogramLeaderColumnFlags;

typedef enum
{
    PTBD_BuildHistogramInnerFlags_JobInfoInBars     = Bit(0),
    PTBD_BuildHistogramInnerFlags_AnimateSmoothBars = Bit(1),
} PTBD_BuildHistogramInnerFlags;

// NOTE(tbt): internal UI helpers
Function void     PTBD_SetNextUIStyleForHistogramBar (DB_Row *timesheet, F32 width_per_hour, F32 height_per_row, F32 padding, B32 is_selected);
Function void     PTBD_BuildVerticalLabelFromFmt(const char *fmt, ...);
Function void     PTBD_BuildVerticalLabelFromFmtV(const char *fmt, va_list args);
Function void     PTBD_BuildVerticalLabel(S8 string);
Function UI_Use   PTBD_BuildScrollBarForHistogram    (S8 string, F32 *scroll, F32 overflow, F32 container_size);
Function UI_Use   PTBD_BuildHistogramHoursScale      (S8 string, DB_Cache timesheets, PTBD_Index *index, F32 *width_per_hour, F32 leader_column_width, F32 horizontal_scroll);
Function UI_Node *PTBD_BuildHistogramLeaderColumn    (PTBD_BuildHistogramLeaderColumnFlags flags, S8 string, DB_Cache timesheets, DB_Cache jobs, PTBD_Index *index, UI_Window window, PTBD_Selection *selection, I64 *last_selected_id, F32 height_per_row, F32 padding, F32 vertical_scroll, PTBD_MsgQueue *m2c);

Function UI_Node *PTBD_BuildHistogramInner           (S8 string, DB_Cache timesheets, DB_Cache jobs, DB_Cache employees, PTBD_Index *index, UI_Window window, PTBD_Selection *selection, PTBD_BuildHistogramInnerFlags flags, PTBD_Histogram *view, F32 padding, PTBD_MsgQueue *m2c, PTBD_Inspectors *inspectors);
Function void     PTBD_BuildTimelineHistogram        (DB_Cache timesheets, DB_Cache jobs, DB_Cache employees, PTBD_Index *index, PTBD_Selection *selection, PTBD_Histogram *view, PTBD_MsgQueue *m2c, PTBD_Inspectors *inspectors);
Function void     PTBD_BuildInvoicingHistogram       (DB_Cache timesheets, DB_Cache jobs, DB_Cache employees, PTBD_Index *index, PTBD_Selection *selection, PTBD_Histogram *view, PTBD_MsgQueue *m2c, PTBD_Inspectors *inspectors);

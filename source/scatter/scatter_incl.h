
#if !defined(SCATTER_INCL_H)
#define SCATTER_INCL_H

Function void SCTR_WindowHookOpen  (OpaqueHandle window);
Function void SCTR_WindowHookDraw  (OpaqueHandle window);
Function void SCTR_WindowHookClose (OpaqueHandle window);

ReadOnlyVar Global G_WindowHooks sctr_window_hooks =
{
    SCTR_WindowHookOpen,
    SCTR_WindowHookDraw,
    SCTR_WindowHookClose,
};

typedef struct SCTR_Point SCTR_Point;
struct SCTR_Point
{
    F64 x;
    F64 y;
};

typedef struct SCTR_Points SCTR_Points;
struct SCTR_Points
{
    SCTR_Points *next;
    SCTR_Point points[128];
    U64 points_count;
};

typedef struct SCTR_Series SCTR_Series;
struct SCTR_Series
{
    SCTR_Series *next;
    
    S8 name;
    SCTR_Points points;
    
    Range2F range;
    
    SCTR_Point *last_plot;
    U64 last_plot_t;
    U64 min_difference_t;
    F64 min_difference_x;
    F64 min_difference_y;
    B32 filter;
};

typedef struct SCTR_PanelPerSeriesData SCTR_PanelPerSeriesData;
struct SCTR_PanelPerSeriesData
{
    SCTR_PanelPerSeriesData *next;
    S8 series_name;
    B32 is_showing;
    U8 col_index;
    B32 is_connected;
};

typedef struct SCTR_Panel SCTR_Panel;
struct SCTR_Panel
{
    SCTR_Panel *next;
    SCTR_PanelPerSeriesData *series;
    F64 pan_x;
    F64 pan_y;
    F64 scale_x;
    F64 scale_y;
    Range2F rect;
};

typedef struct SCTR_Dict SCTR_Dict;
struct SCTR_Dict
{
    SCTR_Series *series;
    SCTR_Panel *panels;
    R2D_QuadList *quads;
    LINE_Batch *line_batch;
    UI_State *ui_state;
};

Function SCTR_Series *SCTR_SeriesFromName (OpaqueHandle window, S8 name);

Function void SCTR_SeriesChecklistBuildUI (OpaqueHandle window, Arena *arena, SCTR_Panel *panel, SCTR_Series *series_list);

Function SCTR_PanelPerSeriesData *SCTR_PanelPerSeriesDataFromName (SCTR_Panel *panel, S8 series_name);
Function void                     SCTR_PanelBuildUI               (OpaqueHandle window, SCTR_Panel *panel);
Function void                     SCTR_PanelDraw                  (OpaqueHandle window, R2D_QuadList *quads, LINE_Batch *line_batch, SCTR_Panel *panel);

Function OpaqueHandle SCTR_OpenWindow             (S8 window_title);
Function void      SCTR_SetMinTimeBetweenPlots (OpaqueHandle window, S8 series_name, U64 microseconds);
Function void      SCTR_SetMinDifferenceX      (OpaqueHandle window, S8 series_name, F64 difference);
Function void      SCTR_SetMinDifferenceY      (OpaqueHandle window, S8 series_name, F64 difference);
Function void      SCTR_PlotPoint              (OpaqueHandle window, S8 series_name, F64 x, F64 y);

ReadOnlyVar Global V4F sctr_palette[] =
{
    ColFromHexInitialiser(0xa3be8cff),
    ColFromHexInitialiser(0xd08770ff),
    ColFromHexInitialiser(0x8fbcbbff),
    ColFromHexInitialiser(0x88c0d0ff),
    ColFromHexInitialiser(0x81a1c1ff),
    ColFromHexInitialiser(0x5e81acff),
    ColFromHexInitialiser(0xb48eadff),
};

#endif

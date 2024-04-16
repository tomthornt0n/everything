
typedef struct PTBD_DragDrop PTBD_DragDrop;
struct PTBD_DragDrop
{
    Arena *arena;
    DB_Table kind;
    DB_Row *data;
    U64 count;
};

Global PTBD_DragDrop ptbd_drag_drop = {0};

Function void PTBD_DraggingClear   (void);
Function void PTBD_DraggingBuildUI (void);

Function void PTBD_DragSelection (DB_Table kind, DB_Cache data, PTBD_Selection *selection);
Function void PTBD_DragRow       (DB_Table kind, DB_Row data);

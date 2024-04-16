
typedef struct PTBD_Selection PTBD_Selection;
struct PTBD_Selection
{
    PTBD_Selection *next;
    U64 cap;
    I64 ids[];
};

Function void PTBD_SelectionSet   (Arena *arena, PTBD_Selection *selection, I64 id, B32 is_selected);
Function B32  PTBD_SelectionHas   (PTBD_Selection *selection, I64 id);
Function void PTBD_SelectionClear (PTBD_Selection *selection);

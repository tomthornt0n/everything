
typedef struct PTBD_UndoAction PTBD_UndoAction;
struct PTBD_UndoAction
{
    PTBD_UndoAction *next;
    ArenaTemp checkpoint;
    B32 is_continuation;
    DB_Table table;
    DB_Cache before;
    DB_Cache after;
};

OpaqueHandle ptbd_undo_lock;
Arena *ptbd_undo_arena;
PTBD_UndoAction *ptbd_undo = 0;
Arena *ptbd_redo_arena;
PTBD_UndoAction *ptbd_redo = 0;

Function void PTBD_DeletedRows    (DB_Table kind, DB_Cache data, U64 ids_count, I64 *ids, B32 is_continuation);
Function void PTBD_EditedRow      (DB_Table kind, DB_Row before, DB_Row after, B32 is_continuation);
Function void PTBD_InsertedNewRow (DB_Table kind, DB_Row row, B32 is_continuation);

Function void PTBD_Undo (PTBD_MsgQueue *m2c, PTBD_Inspectors *inspectors);
Function void PTBD_Redo (PTBD_MsgQueue *m2c, PTBD_Inspectors *inspectors);

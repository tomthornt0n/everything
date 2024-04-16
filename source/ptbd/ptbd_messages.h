
//~NOTE(tbt): ringbuffer for inter-thread communication

typedef struct PTBD_MsgQueue PTBD_MsgQueue;
struct PTBD_MsgQueue
{
    OpaqueHandle sem;
    OpaqueHandle lock;
    U8 buffer[4096];
    U64 read_pos;
    U64 write_pos;
};

//~NOTE(tbt): main -> commands thread messages

typedef enum
{
    M2C_Kind_NONE,
    M2C_Kind_Reload,
    M2C_Kind_Refresh,
    M2C_Kind_WriteTimesheet,
    M2C_Kind_WriteJob,
    M2C_Kind_WriteInvoice,
    M2C_Kind_WriteEmployee,
    M2C_Kind_NewInvoiceForTimesheets,
    M2C_Kind_NewInvoiceForJob,
    M2C_Kind_DeleteRecords,
    M2C_Kind_ExportInvoice,
    M2C_Kind_SetSearch,
    
    M2C_Kind_UndoRestoreBegin,
    M2C_Kind_UndoRestoreEnd,
    M2C_Kind_UndoContinuationBegin,
    M2C_Kind_UndoContinuationEnd,
    
    M2C_Kind_DidCopyState,
    
    
    M2C_Kind_MAX,
} M2C_Kind;

Function void M2C_Reload                  (PTBD_MsgQueue *queue);
Function void M2C_Refresh                 (PTBD_MsgQueue *queue);
Function void M2C_WriteTimesheet          (PTBD_MsgQueue *queue, I64 id, DenseTime date, I64 employee_id, I64 job_id, F64 hours, F64 miles, F64 cost, S8 description, I64 invoice_id);
Function void M2C_WriteJob                (PTBD_MsgQueue *queue, I64 id, S8 number, S8 name);
Function void M2C_WriteInvoice            (PTBD_MsgQueue *queue, I64 id, S8 address, S8 title, S8 description, I64 number, DenseTime date, F64 cost);
Function void M2C_WriteEmployee           (PTBD_MsgQueue *queue, I64 id, S8 name, F64 rate);
Function void M2C_NewInvoiceForTimesheets (PTBD_MsgQueue *queue, U64 count, I64 *ids);
Function void M2C_NewInvoiceForJob        (PTBD_MsgQueue *queue, I64 id);
Function void M2C_DeleteRow               (PTBD_MsgQueue *queue, DB_Table table, I64 id);
Function void M2C_DeleteRows              (PTBD_MsgQueue *queue, DB_Table table, U64 ids_count, I64 *ids);
Function void M2C_DeleteSelectedRows      (PTBD_MsgQueue *queue, DB_Table table, PTBD_Selection *selection);
Function void M2C_ExportInvoice           (PTBD_MsgQueue *queue, I64 id);
Function void M2C_SetSearch               (PTBD_MsgQueue *queue, S8 search);

// NOTE(tbt): tell the commands thread that the commands being issued are to restore an undo/redo state, so undo actions should not be pushed
Function void M2C_UndoRestoreBegin        (PTBD_MsgQueue *queue);
Function void M2C_UndoRestoreEnd          (PTBD_MsgQueue *queue);

// NOTE(tbt): tell the commands thread that the commands being issued are part of one semantic action, so undo actions should be pushed as a single continuation
Function void M2C_UndoContinuationBegin   (PTBD_MsgQueue *queue);
Function void M2C_UndoContinuationEnd     (PTBD_MsgQueue *queue);

Function void M2C_DidCopyState            (PTBD_MsgQueue *queue);

//~NOTE(tbt): commands -> main thread messages

typedef enum
{
    C2M_Kind_WroteNewRecord,
    C2M_Kind_MAX,
} C2M_Kind;

Function void C2M_WroteNewRecord (PTBD_MsgQueue *queue, DB_Table table, I64 id);

//~NOTE(tbt): commands thread

// NOTE(tbt): commands thread state machine for how to push undo actions
typedef enum
{
    PTBD_CmdUndoState_Normal,
    PTBD_CmdUndoState_Restore,
    PTBD_CmdUndoState_ContinuationFirst,
    PTBD_CmdUndoState_Continuation,
} PTBD_CmdUndoState;

// NOTE(tbt): parsed result of deserialising a command from the ringbuffer on the commands thread
typedef struct PTBD_Cmd PTBD_Cmd;
struct PTBD_Cmd
{
    M2C_Kind kind;
    DB_Table table;
    I64 id;
    DenseTime date;
    I64 employee_id;
    I64 job_id;
    I64 invoice_id;
    I64 invoice_number;
    F64 hours;
    F64 miles;
    F64 cost;
    F64 rate;
    S8 title;
    S8 address;
    S8 description;
    S8 search;
    S8 job_number;
    S8 job_name;
    S8 employee_name;
    I64 *ids;
    U64 ids_count;
};

Function PTBD_Cmd PTBD_CmdPop        (Arena *arena, PTBD_MsgQueue *queue);
Function void     PTBD_CmdExec       (PTBD_ArenasForCriticalState arenas, sqlite3 *db, OpaqueHandle lock, B32 volatile *is_next_state_dirty, PTBD_CriticalState *state, PTBD_Cmd cmd, PTBD_MsgQueue *c2m, PTBD_CmdUndoState *undo_state);
Function void     PTBD_CmdThreadProc (void *user_data);


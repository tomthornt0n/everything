
#define PTBD_InspectorColumnList \
PTBD_InspectorColumnDef(Date,        0.1f, 12,  Initialiser((0.0f / 0.0f), (0.0f / 0.0f)))      \
PTBD_InspectorColumnDef(Employee,    0.1f, 256, Initialiser((0.0f / 0.0f), (0.0f / 0.0f)))      \
PTBD_InspectorColumnDef(Description, 0.3f, 512, Initialiser((0.0f / 0.0f), (0.0f / 0.0f)))      \
PTBD_InspectorColumnDef(Hours,       0.1f, 12,  Initialiser(0.5f, 24.0f))                       \
PTBD_InspectorColumnDef(Miles,       0.1f, 12,  Initialiser(0.0f, 1000.0f))                     \
PTBD_InspectorColumnDef(Cost,        0.1f, 12,  Initialiser(0.01f, 1000.0f))                    \
PTBD_InspectorColumnDef(Job,         0.1f, 32,  Initialiser((0.0f / 0.0f), (0.0f / 0.0f)))

typedef enum
{
#define PTBD_InspectorColumnDef(N, W, C, R) PTBD_InspectorColumn_ ## N,
    PTBD_InspectorColumnList
#undef PTBD_InspectorColumnDef
    PTBD_InspectorColumn_MAX,
} PTBD_InspectorColumn;

typedef struct PTBD_Inspector PTBD_Inspector;
struct PTBD_Inspector
{
    PTBD_Inspector *next;
    B32 was_touched;
    F32 spawn_t;
    
    I64 id;
    I64 invoice_id;
    S8Builder fields[PTBD_InspectorColumn_MAX];
    
    I32 autocomplete_index[PTBD_InspectorColumn_MAX];
    
    B32 should_write;
};

typedef struct PTBD_Inspectors PTBD_Inspectors;
struct PTBD_Inspectors
{
    PTBD_Inspector *first;
    PTBD_Inspector *last;
    PTBD_Inspector *free_list;
    U64 count;
    U64 touched_count;
    U64 last_edit_time;
    
    I64 grab_keyboard_focus;
};

ReadOnlyVar Global struct
{
    S8 title;
    UI_Size width;
    U64 line_edit_cap;
    Range1F range;
} ptbd_inspector_columns[PTBD_InspectorColumn_MAX] =
{
#define PTBD_InspectorColumnDef(N, W, C, R) { .title = S8Initialiser(Stringify(N)), .width = { UI_SizeMode_PercentageOfAncestor, W, 1.0f, }, .line_edit_cap = C, .range = R, },
    PTBD_InspectorColumnList
#undef PTBD_InspectorColumnDef
};

Function void            PTBD_InspectorFieldsFromTimesheet    (PTBD_Inspector *inspector, DB_Row *timesheet);
Function PTBD_Inspector *PTBD_InspectorFromId                 (PTBD_Inspectors *inspectors, I64 id);
Function void            PTBD_RequireInspectorForTimesheet    (Arena *arena, DB_Cache timesheets, PTBD_Inspectors *inspectors, I64 id);
Function void            PTBD_UpdateInspectors                (PTBD_Inspectors *inspectors, DB_Cache timesheets);
Function void            PTBD_PushWriteRecordCmdFromInspector (PTBD_MsgQueue *queue, PTBD_Inspector *inspector, DB_Cache jobs, DB_Cache timesheets, DB_Cache employees);

Function B32 PTBD_InspectorIsTimesheet    (PTBD_Inspector *inspector, B32 is_disbursement);
Function B32 PTBD_InspectorIsDisbursement (PTBD_Inspector *inspector);

Function UI_Use PTBD_BuildInspectorField    (PTBD_Inspector *inspector, PTBD_InspectorColumn field_index, U64 *last_edit_time, S8List valid_strings);
Function void   PTBD_BuildInspectorSentence (PTBD_Inspector *inspector, U64 *last_edit_time, S8List job_numbers, S8List employee_names, B32 should_grab_keyboard_focus);
Function void   PTBD_BuildInspectorTableRow (PTBD_Inspector *inspector, U64 *last_edit_time, S8List job_numbers, S8List employee_names, B32 should_grab_keyboard_focus);
Function void   PTBD_BuildInspectorsPanel   (PTBD_Inspectors *inspectors, DB_Cache timesheets, DB_Cache jobs, DB_Cache employees, DB_Cache invoices);

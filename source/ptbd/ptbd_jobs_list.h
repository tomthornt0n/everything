
typedef struct PTBD_JobEditor PTBD_JobEditor;
struct PTBD_JobEditor
{
    I64 id;
    S8Builder number;
    S8Builder name;
};

typedef struct PTBD_JobsListRowId PTBD_JobsListRowId;
struct PTBD_JobsListRowId
{
    I64 job_id;
    I64 timesheet_id;
};

Function void   PTBD_JobsListNav          (void *user_data, UI_Node *node);
Function UI_Use PTBD_BuildJobsListRow     (DB_Row *data, DB_Table kind, Range1U64 index_range, U64 *row_index, B32 is_toggled, DB_Cache jobs, DB_Cache invoices, PTBD_Inspectors *inspectors, PTBD_JobEditor *job_editor, PTBD_MsgQueue *m2c);
Function UI_Use PTBD_BuildJobNumberEditor (DB_Cache jobs, PTBD_JobEditor *job_editor);
Function UI_Use PTBD_BuildJobNameEditor   (PTBD_JobEditor *job_editor);
Function void   PTBD_BuildJobEditor       (DB_Cache jobs, PTBD_JobEditor *job_editor, PTBD_MsgQueue *m2c);
Function void   PTBD_BuildJobsList        (DB_Cache jobs, DB_Cache timesheets, DB_Cache invoices, PTBD_Index *index, PTBD_Selection *selection, PTBD_Selection *expanded, I64 *last_selected_id, PTBD_JobsListRowId *keyboard_focus_id, PTBD_Inspectors *inspectors, PTBD_JobEditor *job_editor, PTBD_MsgQueue *m2c);

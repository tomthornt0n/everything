
//~NOTE(tbt): types

typedef enum
{
    DB_Table_Jobs,
    DB_Table_Timesheets,
    DB_Table_Employees,
    DB_Table_Invoices,
    DB_Table_MAX,
} DB_Table;

typedef struct DB_Row DB_Row;
struct DB_Row
{
    I64 id;
    
    S8 job_number;
    S8 job_name;
    S8 employee_name;
    S8 description;
    S8 address;
    S8 title;
    I64 job_id;
    I64 employee_id;
    I64 invoice_id;
    I64 invoice_number;
    DenseTime date;
    F64 rate;
    F64 hours;
    F64 miles;
    F64 cost;
    F64 cost_raw;
    F64 cost_calculated;
};

typedef struct DB_Cache DB_Cache;
struct DB_Cache
{
    DB_Row *rows;
    U64 rows_count;
};

typedef enum
{
    DB_InvoiceNumber_Draft      =  0,
    DB_InvoiceNumber_WrittenOff = -1,
    DB_InvoiceNumber_Next       = -2,
} DB_InvoiceNumber;

//~NOTE(tbt): functions

// NOTE(tbt): internal helpers
Function void      DB_ErrorPush            (sqlite3 *db, B32 success, S8 message);
Function void      DB_ErrorPushFmt         (sqlite3 *db, B32 success, const char *fmt, ...);
Function S8        DB_S8FromDenseTime      (Arena *arena, DenseTime time);
Function DenseTime DB_DenseTimeFromCString (const char *string);

Function sqlite3 *DB_Open (S8 filename);

// NOTE(tbt): data retrieval
typedef void DB_SelectHook(Arena *arena, sqlite3 *db, DB_Cache *result);
Function void DB_GetJobs          (Arena *arena, sqlite3 *db, DB_Cache *result);
Function void DB_GetTimesheets    (Arena *arena, sqlite3 *db, DB_Cache *result);
Function void DB_GetEmployees     (Arena *arena, sqlite3 *db, DB_Cache *result);
Function void DB_GetInvoices      (Arena *arena, sqlite3 *db, DB_Cache *result);
Function I64  DB_GetWrittenOffInvoiceId        (sqlite3 *db);
Function I64  DB_GetNextInvoiceNumber          (sqlite3 *db);
Function I64  DB_GetDefaultJobId               (sqlite3 *db);
Function I64  DB_GetDefaultEmployeeId          (sqlite3 *db);
Function U64  DB_GetUninvoicedTimesheetsForJob (Arena *arena, sqlite3 *db, I64 job_id, I64 **timesheet_ids);

// NOTE(tbt): insertion/update ((0 == id) ? insert : update)
Function I64 DB_WriteJob          (sqlite3 *db, I64 id, S8 number, S8 name);
Function I64 DB_WriteTimesheet    (sqlite3 *db, I64 id, DenseTime date, I64 employee_id, I64 job_id, F64 hours, F64 miles, F64 cost, S8 description, I64 invoice_id);
Function I64 DB_WriteEmployee     (sqlite3 *db, I64 id, S8 name, F64 rate);
Function I64 DB_WriteInvoice      (sqlite3 *db, I64 id, S8 address, S8 title, S8 description, I64 number, DenseTime date, F64 cost);

// NOTE(tbt): deletion
Function void DB_Delete (sqlite3 *db, DB_Table table, U64 ids_count, I64 *ids);

// NOTE(tbt): misc.
Function DB_Row *DB_RowFromId               (DB_Cache data, I64 id);
Function S8      DB_S8FromTable             (DB_Table table);
Function B32     DB_TimesheetIsDisbursement (DB_Row *row);

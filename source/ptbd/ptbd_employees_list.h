
typedef struct PTBD_EmployeeEditor PTBD_EmployeeEditor;
struct PTBD_EmployeeEditor
{
    I64 id;
    S8Builder name;
    S8Builder rate;
};


// TODO(tbt): avoid hard limit
enum { PTBD_WrappedEmployeesListMaxPerRow = 32, };

typedef struct PTBD_WrappedEmployeesListRow PTBD_WrappedEmployeesListRow;
struct PTBD_WrappedEmployeesListRow 
{
    PTBD_WrappedEmployeesListRow *next;
    U64 count;
    DB_Row *employees[PTBD_WrappedEmployeesListMaxPerRow];
};

typedef struct PTBD_WrappedEmployeesList PTBD_WrappedEmployeesList;
struct PTBD_WrappedEmployeesList
{
    U64 count;
    PTBD_WrappedEmployeesListRow *first;
    PTBD_WrappedEmployeesListRow *last;
};

Function void PTBD_WrappedEmployeesListPush(Arena *arena, PTBD_WrappedEmployeesList *list, U64 employees_per_row, DB_Row *employee);

Function void PTBD_EmployeesListNav      (void *user_data, UI_Node *node);
Function U64  PTBD_EmployeesListNavRight (PTBD_WrappedEmployeesList *list, I64 *keyboard_focus_id);
Function U64  PTBD_EmployeesListNavLeft  (PTBD_WrappedEmployeesList *list, I64 *keyboard_focus_id);
Function U64  PTBD_EmployeesListNavDown  (PTBD_WrappedEmployeesList *list, I64 *keyboard_focus_id);
Function U64  PTBD_EmployeesListNavUp    (PTBD_WrappedEmployeesList *list, I64 *keyboard_focus_id);

Function void PTBD_BuildEmployeesList (DB_Cache employees, PTBD_Index *index, I64 *keyboard_focus_id, PTBD_EmployeeEditor *employee_editor, PTBD_MsgQueue *m2c);

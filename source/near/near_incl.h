
typedef enum
{
    AppCmdKind_None,
    AppCmdKind_BuildPathPush,
    AppCmdKind_BuildPathPop,
    AppCmdKind_BuildPathPushLastPopped,
} AppCmdKind;

typedef struct AppCmd AppCmd;
struct AppCmd
{
    AppCmd *next;
    B32 is_consumed;
    AppCmdKind kind;
    S8 filename;
};

Function AppCmd *AppCmdAlloc (void);

#include "near_ring.h"
#include "near_db.h"
#include "near_file_info.h"
#include "near_view.h"

typedef struct BuildPath BuildPath;
struct BuildPath
{
    ArenaTemp checkpoint;
    BuildPath *next;
    S8 filename;
};

typedef struct BuildPathStack BuildPathStack;
struct BuildPathStack
{
    Arena *arena;
    BuildPath *top;
    BuildPath *pop_history;
};

Function void BuildPathPush (BuildPathStack *stack, S8 filename);
Function void BuildPathPop (BuildPathStack *stack);
Function void BuildPathPushLastPopped (BuildPathStack *stack);

typedef struct AppState AppState;
struct AppState
{
    UI_State *ui;
    AppCmd *cmds_first;
    BuildPathStack build_path_stack;
};

Global AppCmd *volatile app_cmd_free_list = 0;

Function void WindowHookOpen  (OpaqueHandle window);
Function void WindowHookDraw  (OpaqueHandle window);
Function void WindowHookClose (OpaqueHandle window);

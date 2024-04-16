
#include "near_ring.c"
#include "near_db.c"
#include "near_file_info.c"
#include "near_view.c"

Function AppCmd *
AppCmdAlloc(void)
{
    AppCmd *result = app_cmd_free_list;
    if(0 == result)
    {
        Arena *permanent_arena = OS_TCtxGet()->permanent_arena;
        result = PushArray(permanent_arena, AppCmd, 1);
    }
    else
    {
        app_cmd_free_list = app_cmd_free_list->next;
        MemorySet(result, 0, sizeof(result));
    }
    return result;
}

Function void
BuildPathPush(BuildPathStack *stack, S8 filename)
{
    stack->pop_history = 0;
    if(0 != stack->top)
    {
        ArenaTempEnd(stack->top->checkpoint);
    }
    
    BuildPath *path = PushArray(stack->arena, BuildPath, 1);
    path->filename = S8Clone(stack->arena, filename);
    path->checkpoint = ArenaTempBegin(stack->arena);
    path->next = stack->top;
    stack->top = path;
}

Function void
BuildPathPop(BuildPathStack *stack)
{
    if(0 != stack->top && 0 != stack->top->next)
    {
        BuildPath *path = stack->top;
        stack->top = stack->top->next;
        path->next = stack->pop_history;
        stack->pop_history = path;
    }
}


Function void
BuildPathPushLastPopped(BuildPathStack *stack)
{
    if(0 != stack->pop_history)
    {
        BuildPath *path = stack->pop_history;
        stack->pop_history = stack->pop_history->next;
        path->next = stack->top;
        stack->top = path;
    }
}


Function void
WindowHookOpen(OpaqueHandle window)
{
    AppState *app = G_WindowUserDataGet(window);
    G_WindowClearColourSet(window, ColFromHex(0xd8dee9ff));
    app->ui = UI_Init(window);
    
}

Function void
WindowHookDraw(OpaqueHandle window)
{
    AppState *app = G_WindowUserDataGet(window);
    
    Arena *frame_arena = G_WindowFrameArenaGet(window);
    
    UI_Begin(app->ui);
    
    G_EventQueue *events = G_WindowEventQueueGet(window);
    if(G_EventQueueHasKeyDown(events, G_Key_Esc, 0, True) ||
       G_EventQueueHasKeyDown(events, G_Key_Left, G_ModifierKeys_Ctrl, True))
    {
        AppCmd *cmd = AppCmdAlloc();
        cmd->kind = AppCmdKind_BuildPathPop;
        cmd->next = app->cmds_first;
        app->cmds_first = cmd;
    }
    else if(G_EventQueueHasKeyDown(events, G_Key_Right, G_ModifierKeys_Ctrl, True))
    {
        AppCmd *cmd = AppCmdAlloc();
        cmd->kind = AppCmdKind_BuildPathPushLastPopped;
        cmd->next = app->cmds_first;
        app->cmds_first = cmd;
    }
    
    UI_SetNextChildrenLayout(Axis2_Y);
    UI_Node *scroll_region =
        UI_NodeMake(UI_Flags_Scrollable |
                    UI_Flags_ScrollChildrenY |
                    UI_Flags_ClampViewOffsetToOverflow,
                    S8("scroll region"));
    UI_UseFromNode(scroll_region);
    UI_Parent(scroll_region)
    {
        V_Default(app->build_path_stack.top->filename, 0, &app->cmds_first);
    }
    
    for(AppCmd *cmd = app->cmds_first; 0 != cmd; cmd = cmd->next)
    {
        if(AppCmdKind_BuildPathPush == cmd->kind)
        {
            BuildPathPush(&app->build_path_stack, cmd->filename);
            cmd->is_consumed = True;
        }
        else if(AppCmdKind_BuildPathPop == cmd->kind)
        {
            BuildPathPop(&app->build_path_stack);
            cmd->is_consumed = True;
        }
        else if(AppCmdKind_BuildPathPushLastPopped == cmd->kind)
        {
            BuildPathPushLastPopped(&app->build_path_stack);
            cmd->is_consumed = True;
        }
    }
    
    AppCmd **from = &app->cmds_first;
    AppCmd *next_cmd = 0;
    for(AppCmd *cmd = *from; 0 != cmd; cmd = next_cmd)
    {
        next_cmd = cmd->next;
        if(cmd->is_consumed)
        {
            *from = cmd->next;
            cmd->next = app_cmd_free_list;
            OS_InterlockedExchange1Ptr(&app_cmd_free_list, cmd);
        }
        else
        {
            from = &cmd->next;
        }
    }
    
    UI_End();
}

Function void
WindowHookClose(OpaqueHandle window)
{
    AppState *app = G_WindowUserDataGet(window);
}

EntryPoint
{
    OS_Init();
    G_Init();
    R2D_Init();
    LINE_Init();
    FONT_Init();
    IMG_Init();
    
    Arena *permanent_arena = OS_TCtxGet()->permanent_arena;
    
    {
        fi.u2w = MsgRingAlloc(permanent_arena, 4096);
        fi.thread = OS_ThreadStart(FI_ThreadProc, 0);
    }
    
    {
        ArenaTemp scratch = OS_TCtxScratchBegin(0, 0);
        S8 db_filename = FilenamePush(scratch.arena, OS_S8FromStdPath(scratch.arena, OS_StdPath_Config), S8("near_manager.db"));
        sqlite3_open(db_filename.buffer, &db.sqlite);
        ArenaTempEnd(scratch);
        
        S8 sql = S8("CREATE TABLE IF NOT EXISTS near (id INTEGER PRIMARY KEY, filename TEXT, view_name TEXT, data BLOB)");
        sqlite3_stmt *statement = 0;
        sqlite3_prepare_v2(db.sqlite, sql.buffer, sql.len, &statement, 0);
        while(SQLITE_ROW == sqlite3_step(statement))
        {
        }
        
        for(U64 stripe_index = 0; stripe_index < DB_CacheStripesCount; stripe_index += 1)
        {
            db.stripes[stripe_index] = OS_SemaphoreAlloc(1);
        }
        
        db.u2w = MsgRingAlloc(permanent_arena, 4096);
        db.thread = OS_ThreadStart(DB_ThreadProc, 0);
    }
    
    AppState app_state = {0};
    app_state.build_path_stack.arena = ArenaAlloc(Gigabytes(1));
    BuildPathPush(&app_state.build_path_stack, S8("C:"));
    
    G_WindowHooks window_hooks =
    {
        WindowHookOpen,
        WindowHookDraw,
        WindowHookClose,
    };
    G_WindowOpen(S8("Near"), V2I(1024, 768), window_hooks, &app_state);
    
    G_MainLoop();
    
    OS_SemaphoreSignal(fi.u2w.sem);
    OS_SemaphoreSignal(db.u2w.sem);
    
    OS_ThreadJoin(fi.thread);
    OS_ThreadJoin(db.thread);
    
    IMG_Cleanup();
    LINE_Cleanup();
    R2D_Cleanup();
    G_Cleanup();
    OS_Cleanup();
}

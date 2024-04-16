
//~NOTE(tbt): thread context

Function void
OS_ThreadContextAlloc(OS_ThreadContext *tctx, I32 logical_thread_index)
{
    tctx->permanent_arena = ArenaAlloc(Gigabytes(2));
    tctx->errors_arena = ArenaAlloc(Megabytes(500));
    ArenaTemp errors_checkpoint = ArenaTempBegin(tctx->errors_arena);
    tctx->errors = PushArray(tctx->errors_arena, ErrorScope, 1);
    tctx->errors->checkpoint = errors_checkpoint;
    ScratchPoolAlloc(&tctx->scratch_pool);
}

Function void
OS_ThreadContextRelease(OS_ThreadContext *tctx)
{
    ArenaRelease(tctx->permanent_arena);
    ScratchPoolRelease(&tctx->scratch_pool);
}

// NOTE(tbt): wrappers

Function ArenaTemp
OS_TCtxScratchBegin(Arena *non_conflict[], U64 non_conflict_count)
{
    ArenaTemp result;
    ScratchPool *pool = &OS_TCtxGet()->scratch_pool;
    result = ScratchBegin(pool, non_conflict, non_conflict_count);
    return result;
}

Function void
OS_TCtxErrorScopePush(void)
{
    OS_ThreadContext *tctx = OS_TCtxGet();
    ArenaTemp checkpoint = ArenaTempBegin(tctx->errors_arena);
    ErrorScope *scope = PushArray(tctx->errors_arena, ErrorScope, 1);
    scope->checkpoint = checkpoint;
    scope->next = tctx->errors;
    tctx->errors = scope;
}

Function ErrorScope
OS_TCtxErrorScopePop(Arena *arena)
{
    ErrorScope result = {0};
    
    OS_ThreadContext *tctx = OS_TCtxGet();
    
    ErrorScope *scope = tctx->errors;
    if(0 != scope)
    {
        tctx->errors = tctx->errors->next;
    }
    
    for(ErrorMsg *e = scope->last; 0 != e && 0 != arena; e = e->prev)
    {
        ErrorPush(&result, arena, e->timestamp, e->string);
    }
    
    ArenaTempEnd(scope->checkpoint);
    
    return result;
}

Function U64
OS_TCtxErrorScopePropogate(void)
{
    U64 result = 0;
    
    OS_ThreadContext *tctx = OS_TCtxGet();
    
    ErrorScope *scope = tctx->errors;
    if(0 != scope)
    {
        tctx->errors = tctx->errors->next;
        if(0 != tctx->errors && 0 != scope->last)
        {
            scope->last->next = tctx->errors->first;
            tctx->errors->first = scope->first;
            if(0 == tctx->errors->last)
            {
                tctx->errors->last = tctx->errors->first;
            }
            tctx->errors->count += scope->count;
            result = scope->count;
        }
    }
    
    return result;
}

Function void
OS_TCtxErrorScopeClear(void)
{
    OS_ThreadContext *tctx = OS_TCtxGet();
    tctx->errors->first = 0;
    tctx->errors->last = 0;
    tctx->errors->count = 0;
    ArenaTempEnd(tctx->errors->checkpoint);
}

Function void
OS_TCtxErrorScopeMerge(ErrorScope *scope)
{
    OS_ThreadContext *tctx = OS_TCtxGet();
    
    for(ErrorMsg *e = scope->last; 0 != e; e = e->prev)
    {
        ErrorPush(tctx->errors, tctx->errors_arena, e->timestamp, e->string);
    }
}

Function void
OS_TCtxErrorPush(S8 string)
{
    OS_ThreadContext *tctx = OS_TCtxGet();
    ErrorPush(tctx->errors, tctx->errors_arena, OS_TimeInMicroseconds(), string);
}

Function void
OS_TCtxErrorPushCond(B32 success, S8 string)
{
    OS_ThreadContext *tctx = OS_TCtxGet();
    ErrorPushCond(tctx->errors, tctx->errors_arena, success, OS_TimeInMicroseconds(), string);
}

Function void
OS_TCtxErrorPushFmtV(const char *fmt, va_list args)
{
    OS_ThreadContext *tctx = OS_TCtxGet();
    ErrorPushFmtV(tctx->errors, tctx->errors_arena, OS_TimeInMicroseconds(), fmt, args);
}

Function void
OS_TCtxErrorPushFmtCondV(B32 success, const char *fmt, va_list args)
{
    OS_ThreadContext *tctx = OS_TCtxGet();
    ErrorPushFmtCondV(tctx->errors, tctx->errors_arena, success, OS_TimeInMicroseconds(), fmt, args);
}

Function void
OS_TCtxErrorPushFmt(const char *fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    OS_TCtxErrorPushFmtV(fmt, args);
    va_end(args);
}

Function void
OS_TCtxErrorPushFmtCond(B32 success, const char *fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    OS_TCtxErrorPushFmtCondV(success, fmt, args);
    va_end(args);
}

Function ErrorMsg *
OS_TCtxErrorFirst(void)
{
    OS_ThreadContext *tctx = OS_TCtxGet();
    ErrorMsg *result = 0;
    if(0 != tctx->errors)
    {
        result = tctx->errors->first;
    }
    return result;
}

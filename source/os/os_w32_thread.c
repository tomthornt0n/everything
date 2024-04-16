
//~NOTE(tbt): thread creation

Function OpaqueHandle
OS_ThreadStart(OS_ThreadInitHook *init, void *user_data)
{
    W32_ThreadProcArgs *args = LocalAlloc(0, sizeof(*args));
    MemorySet(args, 0, sizeof(*args));
    args->init = init;
    args->user_data = user_data;
    void *thread = CreateThread(0, 0, W32_ThreadProc, args, 0, 0);
    OpaqueHandle result = { .p = IntFromPtr(thread), };
    return result;
}

Function void
OS_ThreadJoin(OpaqueHandle thread)
{
    void *t = PtrFromInt(thread.p);
    WaitForSingleObject(t, INFINITE);
    CloseHandle(t);
}

Function void
OS_ThreadStop(OpaqueHandle thread)
{
    void *t = PtrFromInt(thread.p);
    TerminateThread(t, 0);
    CloseHandle(t);
}

//~NOTE(tbt): synchronisation primitives

Function OpaqueHandle
OS_SemaphoreAlloc(I32 initial)
{
    HANDLE sem = CreateSemaphoreW(0, initial, INT_MAX, 0);
    OpaqueHandle result = { .p = IntFromPtr(sem), };
    return result;
}

Function void
OS_SemaphoreSignal(OpaqueHandle sem)
{
    void *s = PtrFromInt(sem.p);
    ReleaseSemaphore(s, 1, 0);
}

Function void
OS_SemaphoreWait(OpaqueHandle sem)
{
    void *s = PtrFromInt(sem.p);
    WaitForSingleObject(s, INFINITE);
}

Function void
OS_SemaphoreRelease(OpaqueHandle sem)
{
    void *s = PtrFromInt(sem.p);
    CloseHandle(s);
}

Function OpaqueHandle
OS_SRWAlloc(void)
{
    OpaqueHandle result = {0};
    
    W32_SRWLock *lock = w32_srw_locks_free_list;
    if(0 == lock)
    {
        OS_TCtxErrorPush(S8("No more free SRW locks!"));
    }
    else
    {
        w32_srw_locks_free_list = lock->next_free;
        InitializeSRWLock(&lock->srw_lock);
        lock->generation += 1;
        result.p = lock - w32_srw_locks;
        result.g = lock->generation;
    }
    
    return result;
}

Function void
OS_SRWLockRead(OpaqueHandle lock)
{
    W32_SRWLock *l = &w32_srw_locks[lock.p];
    if(l->generation != lock.g)
    {
        OS_TCtxErrorPush(S8("SRW lock generation did not match"));
    }
    else
    {
        AcquireSRWLockShared(&l->srw_lock);
    }
}

Function void
OS_SRWLockWrite(OpaqueHandle lock)
{
    W32_SRWLock *l = &w32_srw_locks[lock.p];
    if(l->generation != lock.g)
    {
        OS_TCtxErrorPush(S8("SRW lock generation did not match"));
    }
    else
    {
        AcquireSRWLockExclusive(&l->srw_lock);
    }
}

Function void
OS_SRWUnlockRead(OpaqueHandle lock)
{
    W32_SRWLock *l = &w32_srw_locks[lock.p];
    if(l->generation != lock.g)
    {
        OS_TCtxErrorPush(S8("SRW lock generation did not match"));
    }
    else
    {
        ReleaseSRWLockShared(&l->srw_lock);
    }
}

Function void
OS_SRWUnlockWrite(OpaqueHandle lock)
{
    W32_SRWLock *l = &w32_srw_locks[lock.p];
    if(l->generation != lock.g)
    {
        OS_TCtxErrorPush(S8("SRW lock generation did not match"));
    }
    else
    {
        ReleaseSRWLockExclusive(&l->srw_lock);
    }
}

Function void
OS_SRWRelease(OpaqueHandle lock)
{
    W32_SRWLock *l = &w32_srw_locks[lock.p];
    if(l->generation != lock.g)
    {
        OS_TCtxErrorPush(S8("SRW lock generation did not match"));
    }
    else
    {
        l->next_free = w32_srw_locks_free_list;
        w32_srw_locks_free_list = l;
    }
}

//~NOTE(tbt): interlocked operations

Function I32
OS_InterlockedCompareExchange1I(I32 volatile *a, I32 b, I32 comperand)
{
    return InterlockedCompareExchange(a, b, comperand);
}

Function I32
OS_InterlockedExchange1I(I32 volatile *a, I32 b)
{
    return InterlockedExchange(a, b);
}

Function I32
OS_InterlockedIncrement1I(I32 volatile *a)
{
    return InterlockedIncrement(a);
}

Function I32
OS_InterlockedDecrement1I(I32 volatile *a)
{
    return InterlockedDecrement(a);
}

Function U64
OS_InterlockedCompareExchange1U64(U64 volatile *a, U64 b, U64 comperand)
{
    return InterlockedCompareExchange64(a, b, comperand);
}

Function U64
OS_InterlockedExchange1U64(U64 volatile *a, U64 b)
{
    return InterlockedExchange64(a, b);
}

Function U64
OS_InterlockedIncrement1U64(U64 volatile *a)
{
    return InterlockedIncrement64(a);
}

Function U64
OS_InterlockedDecrement1U64(U64 volatile *a)
{
    return InterlockedDecrement64(a);
}

Function void *
OS_InterlockedCompareExchange1Ptr(void *volatile *a, void *b, void *comperand)
{
    return InterlockedCompareExchangePointer(a, b, comperand);
}

Function void *
OS_InterlockedExchange1Ptr(void *volatile *a, void *b)
{
    return InterlockedExchangePointer(a, b);
}

//~NOTE(tbt): thread context

Function void
OS_ThreadNameSet(S8 name)
{
    ArenaTemp scratch = OS_TCtxScratchBegin(0, 0);
    
    {
        S16 name_16 = S16FromS8(scratch.arena, name);
        HRESULT hr = SetThreadDescription(GetCurrentThread(), name_16.buffer);
    }
    
    {
        S8 name_copy = S8Clone(scratch.arena, name);
        
#pragma pack(push,8)
        typedef struct THREADNAME_INFO THREADNAME_INFO;
        struct THREADNAME_INFO
        {
            U32 dwType;  // Must be 0x1000
            const char *szName; // Pointer to name (in user addr space)
            U32 dwThreadID; // Thread ID 
            U32 dwFlags; // Reserved for future use, must be zero
        };
#pragma pack(pop)
        
        THREADNAME_INFO info;
        info.dwType = 0x1000;
        info.szName = name_copy.buffer;
        info.dwThreadID = GetThreadId(0);
        info.dwFlags = 0;
        
#pragma warning(push)
#pragma warning(disable: 6320 6322)
        __try
        {
            RaiseException(0x406D1388, 0, sizeof(info) / sizeof(void *), (const ULONG_PTR *)&info);
        }
        __except (EXCEPTION_EXECUTE_HANDLER)
        {
        }
#pragma warning(pop)
    }
    
    ArenaTempEnd(scratch);
}

Function OS_ThreadContext *
OS_TCtxGet(void)
{
    OS_ThreadContext *result = TlsGetValue(w32_tc_id);
    return result;
}

Function void
OS_TCtxSet(OS_ThreadContext *tctx)
{
    void *ptr = tctx;
    TlsSetValue(w32_tc_id, ptr);
}


//~NOTE(tbt): thread control

typedef void OS_ThreadInitHook(void *user_data);

#if BuildModeDebug
# define OS_AssertMainThread() Statement( AssertWithMessage(OS_TCtxGet()->logical_thread_index == 0, "should only be called from the main thread\n"); )
#else
# define OS_AssertMainThread() 
#endif

Function OpaqueHandle OS_ThreadStart (OS_ThreadInitHook *init, void *user_data);
Function void OS_ThreadJoin (OpaqueHandle thread);
Function void OS_ThreadStop (OpaqueHandle thread);

//~NOTE(tbt): synchronisation primitives

Function OpaqueHandle OS_SemaphoreAlloc (I32 initial);
Function void OS_SemaphoreSignal (OpaqueHandle sem);
Function void OS_SemaphoreWait (OpaqueHandle sem);
Function void OS_SemaphoreRelease (OpaqueHandle sem);

Function OpaqueHandle OS_SRWAlloc (void);
Function void OS_SRWLockRead (OpaqueHandle lock);
Function void OS_SRWLockWrite (OpaqueHandle lock);
Function void OS_SRWUnlockRead (OpaqueHandle lock);
Function void OS_SRWUnlockWrite (OpaqueHandle lock);
Function void OS_SRWRelease (OpaqueHandle lock);

#define OS_CriticalSectionShared(L) DeferLoop(OS_SRWLockRead(L), OS_SRWUnlockRead(L))
#define OS_CriticalSectionExclusive(L) DeferLoop(OS_SRWLockWrite(L), OS_SRWUnlockWrite(L))

//~NOTE(tbt): interlocked operations

// TODO(tbt): these should be in base layer instead
// should not depend on OS, depends on arch and compiler

#if BuildCompilerMSVC
# include <intrin.h>
# define OS_WriteBarrier Statement( _WriteBarrier(); _mm_sfence(); )
#elif BuildCompilerGCC || BuildCompilerClang
# define OS_WriteBarrier Statement( __sync_synchronize(); )
#else
# error OS_WriteBarrier not implemented for this platform
#endif

Function I32 OS_InterlockedCompareExchange1I (I32 volatile *a, I32 ex, I32 comperand);
Function I32 OS_InterlockedExchange1I (I32 volatile *a, I32 ex);
Function I32 OS_InterlockedIncrement1I (I32 volatile *a);
Function I32 OS_InterlockedDecrement1I (I32 volatile *a);
Function U64 OS_InterlockedCompareExchange1U64 (U64 volatile *a, U64 ex, U64 comperand);
Function U64 OS_InterlockedExchange1U64 (U64 volatile *a, U64 ex);
Function U64 OS_InterlockedIncrement1U64 (U64 volatile *a);
Function U64 OS_InterlockedDecrement1U64 (U64 volatile *a);
Function void *OS_InterlockedCompareExchange1Ptr (void *volatile *a, void *ex, void *comperand);
Function void *OS_InterlockedExchange1Ptr (void *volatile *a, void *ex);

//~NOTE(tbt): thread context

typedef struct OS_ThreadContext OS_ThreadContext;
struct OS_ThreadContext
{
    I32 logical_thread_index;
    
    ScratchPool scratch_pool;
    Arena *permanent_arena;
    
    S8 exe_path;
    
    Arena *errors_arena;
    ErrorScope *errors;
};

Function void OS_ThreadNameSet (S8 name);

Function void OS_ThreadContextAlloc (OS_ThreadContext *tctx, I32 logical_thread_index);
Function void OS_ThreadContextRelease (OS_ThreadContext *tctx);

Function OS_ThreadContext *OS_TCtxGet (void); // NOTE(tbt): gets the thread local context pointer
Function void OS_TCtxSet (OS_ThreadContext *ptr); // NOTE(tbt): sets the thread local context pointer

// NOTE(tbt): wrappers

Function ArenaTemp OS_TCtxScratchBegin (Arena *non_conflict[], U64 non_conflict_count);

Function void OS_TCtxErrorScopePush (void);
Function void OS_TCtxErrorScopeMerge (ErrorScope *scope); // NOTE(tbt): merges scope into the top scope in the threads context
Function ErrorScope OS_TCtxErrorScopePop (Arena *arena); // NOTE(tbt): can pass 0 as arena if you don't want the scope returned
Function U64 OS_TCtxErrorScopePropogate (void); // NOTE(tbt): merges the top two error scopes - passes the errors "up the stack"
Function void OS_TCtxErrorScopeClear (void); // NOTE(tbt): clears all errors from the top scope

Function void OS_TCtxErrorPush (S8 string);
Function void OS_TCtxErrorPushCond (B32 success, S8 string);
Function void OS_TCtxErrorPushFmtV (const char *fmt, va_list args);
Function void OS_TCtxErrorPushFmtCondV (B32 success, const char *fmt, va_list args);
Function void OS_TCtxErrorPushFmt (const char *fmt, ...);
Function void OS_TCtxErrorPushFmtCond (B32 success, const char *fmt, ...);
Function ErrorMsg *OS_TCtxErrorFirst (void);

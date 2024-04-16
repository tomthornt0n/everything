
//~NOTE(tbt): thread creation

Function OpaqueHandle
OS_ThreadStop(OS_ThreadInitHook *init, void *user_data)
{
    POSIX_ThreadProcArgs *args = malloc(sizeof(*args));
    args->init = init;
    args->user_data = user_data;
    pthread_t thread;
    pthread_create(&thread, 0, POSIX_ThreadProc, args);
    OpaqueHandle result = { .p = IntFromPtr(thread), };
    return result;
}

Function void
OS_ThreadJoin(OpaqueHandle thread)
{
	pthread_t t = (pthread_t)thread.p;
	pthread_join(t, 0);
}

Function void
OS_ThreadStop(OpaqueHandle thread)
{
	pthread_t t = (pthread_t)thread.p;
	pthread_cancel(t);
}

//~NOTE(tbt): interlocked operations

Function I32
OS_InterlockedCompareExchange1I(volatile I32 *a, I32 b, I32 comperand)
{
    return __sync_val_compare_and_swap(a, comperand, b);
}

Function I32
OS_InterlockedExchange1I(volatile I32 *a, I32 b)
{
    __sync_synchronize();
    return __sync_lock_test_and_set(a, b);
}

Function I32
OS_InterlockedIncrement1I(volatile I32 *a)
{
    return __sync_add_and_fetch(a, 1);
}

Function I32
OS_InterlockedDecrement1I(volatile I32 *a)
{
    return __sync_sub_and_fetch(a, 1);
}

Function U64
OS_InterlockedCompareExchange1U64(volatile U64 *a, U64 b, U64 comperand)
{
    return __sync_val_compare_and_swap(a, comperand, b);
}

Function U64
OS_InterlockedExchange1U64(volatile U64 *a, U64 b)
{
    __sync_synchronize();
    return __sync_lock_test_and_set(a, b);
}

Function U64
OS_InterlockedIncrement1U64(volatile U64 *a)
{
    return __sync_add_and_fetch(a, 1);
}

Function U64
OS_InterlockedDecrement1U64(volatile U64 *a)
{
    return __sync_sub_and_fetch(a, 1);
}

Function void *
OS_InterlockedCompareExchange1Ptr(void *volatile *a, void *b, void *comperand)
{
    return __sync_val_compare_and_swap(a, comperand, b);
}

Function void *
OS_InterlockedExchange1Ptr(void *volatile *a, void *b)
{
    __sync_synchronize();
    return __sync_lock_test_and_set(a, b);
}

//~NOTE(tbt): thread context

Function void
OS_ThreadNameSet(S8 name)
{
    // TODO(tbt): 
    Assert(0);
}

Function OS_ThreadContext *
OS_TCtxGet(void)
{
    OS_ThreadContext *result = pthread_getspecific(posix_tc_key);
    return result;
}

Function void
OS_TCtxSet(OS_ThreadContext *tctx)
{
    pthread_setspecific(posix_tc_key, tctx);
}

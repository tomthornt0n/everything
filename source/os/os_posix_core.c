
//~NOTE(tbt): initialisation and cleanup

#if BuildOSMac
Function void MAC_Init (void);
#endif

Function void
OS_Init(void)
{
    pthread_key_create(&posix_tc_key, 0);
    Persist OS_ThreadContext main_thread_context;
    OS_ThreadContextAlloc(&main_thread_context, 0);
    OS_TCtxSet(&main_thread_context);
    
#if BuildOSMac
    MAC_Init();
#endif
}

Function void
OS_Cleanup(void)
{
#if BuildModeDebug
    OS_ConsoleOutputLine(S8("The main thread reported the following errors:"));
    for(ErrorMsg *error = OS_TCtxErrorFirst(); 0 != error; error = error->next)
    {
        OS_ConsoleOutputFmt("%.*s - %.*s", FmtS8(error->friendly), FmtS8(error->detail));
    }
#endif
}

//~NOTE(tbt): console

Function void
OS_ConsoleOutputS8(S8 string)
{
	write(2, string.buffer, string.len);
}

Function void
OS_ConsoleOutputS16(S16 string)
{
	ArenaTemp scratch = OS_TCtxScratchBegin(0, 0);
    S8 s8 = S8FromS16(scratch.arena, string);
    write(2, s8.buffer, s8.len);
    ArenaTempEnd(scratch);
}

//~NOTE(tbt): entropy

Function void
OS_WriteEntropy(void *buffer, U64 size)
{
	getentropy(buffer, size);
}

Function U64
OS_U64FromEntropy(void)
{
    U64 result;
    OS_WriteEntropy(&result, sizeof(result));
    return result;
}

//~NOTE(tbt): memory

Function void *
OS_MemoryReserve(U64 size)
{
    return mmap(0, size, PROT_NONE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
}

Function void
OS_MemoryRelease(void *memory, U64 size)
{
    int rc = munmap(memory, size);
}

Function void
OS_MemoryCommit(void *memory, U64 size)
{
    // NOTE(tbt): round down to page boundary
    uintptr_t p = IntFromPtr(memory);
    long page_size = sysconf(_SC_PAGESIZE);
    p = (p / page_size)*page_size;
    size += IntFromPtr(memory) - p;
    memory = PtrFromInt(p);
    
    int rc = mprotect(memory, size, PROT_READ | PROT_WRITE);
}

Function void
OS_MemoryDecommit(void *memory, U64 size)
{
    int rc = mprotect(memory, size, PROT_NONE);
}

//~NOTE(tbt): shared libraries

Function OpaqueHandle
OS_SharedLibOpen(S8 filename)
{
	OpaqueHandle result = {0};
	void *dl = dlopen(filename.buffer, RTLD_NOW | RTLD_GLOBAL);
	result.p = IntFromPtr(dl);
    return result;
}

Function void *
OS_SharedLibSymbolLookup(OpaqueHandle lib, char *symbol)
{
	Assert(0 != lib.p);
	void *dl = PtrFromInt(lib.p);
    void *result = dlsym(dl, symbol);
    return result;
}

Function void
OS_SharedLibClose(OpaqueHandle lib)
{
	Assert(0 != lib.p);
	void *dl = PtrFromInt(lib.p);
    dlclose(dl);
}

//~NOTE(tbt): time

Function DateTime
OS_NowUTC(void)
{
    time_t t = time(0);
    TM tm;
    gmtime_r(&t, &tm);
    DateTime result = POSIX_DateTimeFromTm(tm);
    return result;
}

Function DateTime
OS_NowLTC(void)
{
    time_t t = time(0);
    TM tm;
    localtime_r(&t, &tm);
    DateTime result = POSIX_DateTimeFromTm(tm);
    return result;
}

Function DateTime
OS_LTCFromUTC(DateTime universal_time)
{
    TM tm = POSIX_TmFromDateTime(universal_time);
    time_t t = mktime(&tm);
    localtime_r(&t, &tm);
    DateTime result = POSIX_DateTimeFromTm(tm);
    return result;
}

Function DateTime
OS_UTCFromLTC(DateTime local_time)
{
    TM tm = POSIX_TmFromDateTime(local_time);
    time_t t = mktime(&tm);
    gmtime_r(&t, &tm);
    DateTime result = POSIX_DateTimeFromTm(tm);
    return result;
}

Function void
OS_Sleep(U64 milliseconds)
{
    struct timespec ts =
    {
        .tv_sec = milliseconds / 1000,
        .tv_nsec = (milliseconds % 1000)*1000000,
    };
    int r;
    do
    {
        r = nanosleep(&ts, &ts);
    } while(r && errno == EINTR);
}

Function U64
OS_TimeInMicroseconds(void)
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC_RAW, &ts);
    U64 result = (ts.tv_sec*1000000000 + ts.tv_nsec)/1000;
    return result;
}

//~NOTE(tbt): system queries

Function size_t
OS_ProcessorsCount(void)
{
    size_t result = sysconf(_SC_NPROCESSORS_ONLN);
    return result;
}



typedef struct tm TM;

Function DateTime
POSIX_DateTimeFromTm(TM tm)
{
    DateTime result;
    result.year = tm.tm_year + 1900;
    result.mon = tm.tm_mon;
    result.day = tm.tm_mday - 1;
    result.hour = tm.tm_hour;
    result.min = tm.tm_min;
    result.sec = tm.tm_sec;
    result.msec = 0; // TODO(tbt): get this somehow
    return result;
}

Function TM
POSIX_TmFromDateTime(DateTime t)
{
    TM result;
    result.tm_year = t.year - 1900;
    result.tm_mon = t.mon;
    result.tm_mday = t.day + 1;
    result.tm_hour = t.hour;
    result.tm_min = t.min;
    result.tm_sec = t.sec;
    result.tm_isdst = -1;
    mktime(&result);
    return result;
}

Function DenseTime
POSIX_DenseTimeFromSeconds(time_t t)
{
    TM tm;
    localtime_r((time_t[1]){0}, &tm);
    tm.tm_sec += t;
    mktime(&tm);
    DateTime date_time = POSIX_DateTimeFromTm(tm);
    DenseTime result = DenseTimeFromDateTime(date_time);
    return result;
}

typedef struct POSIX_ThreadProcArgs POSIX_ThreadProcArgs;
struct POSIX_ThreadProcArgs
{
    OS_ThreadInitHook *init;
    void *user_data;
    OS_ThreadContext tctx;
};

Global pthread_key_t posix_tc_key;

Global U64 posix_thread_count = 0;

Function void *
POSIX_ThreadProc(void *param)
{
    POSIX_ThreadProcArgs *args = param;
    
    U64 logical_thread_index = OS_InterlockedIncrement1U64(&posix_thread_count);
    OS_ThreadContextAlloc(&args->tctx, logical_thread_index);
    OS_TCtxSet(&args->tctx);
    
    args->init(args->user_data);
    
    OS_ThreadContextRelease(&args->tctx);
    free(param);
    OS_InterlockedDecrement1U64(&posix_thread_count);
    
    return 0;
}

Function size_t
POSIX_FileSizeGet(int fd)
{
    off_t current = lseek(fd, 0, SEEK_CUR);
    size_t result = lseek(fd, 0, SEEK_END);
    lseek(fd, current, SEEK_SET);
    return result;
}

typedef struct POSIX_FileIterator POSIX_FileIterator;
struct POSIX_FileIterator
{
    OS_FileIterator _;
    DIR *d;
    struct dirent *dir;
    B32 is_done;
};



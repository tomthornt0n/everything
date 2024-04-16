
typedef struct W32_ThreadProcArgs W32_ThreadProcArgs;
struct W32_ThreadProcArgs
{
    OS_ThreadInitHook *init;
    void *user_data;
    OS_ThreadContext tctx;
};

typedef struct W32_FileIterator W32_FileIterator;
struct W32_FileIterator
{
    OS_FileIterator _;
    HANDLE handle;
    WIN32_FIND_DATAW find_data;
    B32 is_done;
};

typedef struct W32_SRWLock W32_SRWLock;
struct W32_SRWLock
{
    W32_SRWLock *next_free;
    SRWLOCK srw_lock;
    U64 generation;
};

ThreadLocalVar Global W32_SRWLock w32_srw_locks[256];
ThreadLocalVar Global W32_SRWLock *w32_srw_locks_free_list;

Global HANDLE w32_threads_count = 0;

Global U64 w32_t_perf_counter_ticks_per_second = 0;
Global B32 w32_t_is_granular = False;

Global DWORD w32_tc_id = 0;

Global UINT w32_clip_string_list_format = 0;

Global HANDLE w32_console_handle = INVALID_HANDLE_VALUE;

Function DWORD WINAPI W32_ThreadProc (LPVOID param);

Function S16 W32_FormatLastError (void);
Function void W32_TCtxPushLastErrorFormattedCond (B32 success);

Function U64 W32_SizeFromFileHandle (HANDLE file_handle);

Function void W32_AssertLogCallback (char *title, char *msg);

Function U64 W32_U64FromHiAndLoWords (DWORD hi, DWORD lo);

Function S8List W32_S8ListFromWin32StringList (Arena *arena, wchar_t *string_list);
Function S16 W32_Win32StringListFromS8List (Arena *arena, S8List list);
Function S16 W32_FileOpListFromS8List      (Arena *arena, S8List list);

Function OS_FilePropertiesFlags W32_FilePropertiesFlagsFromFileAttribs (DWORD attribs);
Function DataAccessFlags W32_DataAccessFlagsFromFileAttribs (DWORD attribs);

Function SYSTEMTIME W32_SystemTimeFromDateTime (DateTime date_time);
Function DenseTime W32_DenseTimeFromFileTime (FILETIME file_time);
Function DateTime W32_DateTimeFromSystemTime (SYSTEMTIME system_time);

Function void W32_SRWLockAllocatorInit (void);
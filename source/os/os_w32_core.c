
//~NOTE(tbt): initialisation and cleanup

Function void
OS_Init(void)
{
    //-NOTE(tbt): weird COM stuff
    CoInitializeEx(0, COINIT_APARTMENTTHREADED/* | COINIT_DISABLE_OLE1DDE*/);
    OleInitialize(0);
    
    // NOTE(tbt): commit at page granularity
    SYSTEM_INFO system_info = {0};
    GetSystemInfo(&system_info);
    arena_commit_chunk_size = system_info.dwPageSize;
    
    //-NOTE(tbt): initialise time stuff
    LARGE_INTEGER perf_counter_frequency;
    if(QueryPerformanceFrequency(&perf_counter_frequency))
    {
        w32_t_perf_counter_ticks_per_second = perf_counter_frequency.QuadPart;
    }
    if(TIMERR_NOERROR == timeBeginPeriod(1))
    {
        w32_t_is_granular = True;
    }
    
    //-NOTE(tbt): initialise thread context
    w32_tc_id = TlsAlloc();
    Persist OS_ThreadContext main_thread_context;
    OS_ThreadContextAlloc(&main_thread_context, 0);
    OS_TCtxSet(&main_thread_context);
    w32_threads_count = CreateSemaphoreW(0, 0, INT_MAX, 0);
    
    //-NOTE(tbt): initialise console
#if BuildModeDebug
    FreeConsole();
    AllocConsole();
    SetConsoleCP(CP_UTF8);
    w32_console_handle = GetStdHandle(STD_OUTPUT_HANDLE);
    // TODO(tbt): setup stdout handle to use ANSI escape sequences
#endif
    
    assert_log_hook = W32_AssertLogCallback;
    
    //-NOTE(tbt): initialise clipboard stuff
    w32_clip_string_list_format = RegisterClipboardFormatW(L"OS_S8List");
}

Function void
OS_Cleanup(void)
{
    if(w32_t_is_granular)
    {
        timeEndPeriod(1);
    }
    
#if BuildModeDebug
    OS_ConsoleOutputLine(S8("The main thread reported the following errors:"));
    for(ErrorMsg *error = OS_TCtxErrorFirst(); 0 != error; error = error->next)
    {
        OS_ConsoleOutputLine(error->string);
    }
#endif
}

//~NOTE(tbt): clipboard

Function void
OS_ClipboardTextSet(S8 string)
{
    ArenaTemp scratch = OS_TCtxScratchBegin(0, 0);
    
    S16 s16 = S16FromS8(scratch.arena, string);
    OpenClipboard(0);
    EmptyClipboard();
    HGLOBAL mem_handle = GlobalAlloc(GMEM_ZEROINIT, (s16.len + 1)*sizeof(wchar_t));
    void *mem = GlobalLock(mem_handle);
    MemoryCopy(mem, s16.buffer, s16.len*sizeof(wchar_t));
    GlobalUnlock(mem_handle);
    SetClipboardData(CF_UNICODETEXT, mem_handle);
    CloseClipboard();
    
    ArenaTempEnd(scratch);
}

Function void
OS_ClipboardFilesSet(S8List filenames)
{
    ArenaTemp scratch = OS_TCtxScratchBegin(0, 0);
    
    OpenClipboard(0);
    EmptyClipboard();
    S16 file_list = W32_FileOpListFromS8List(scratch.arena, filenames);
    HGLOBAL drop_files_handle = GlobalAlloc(GHND, sizeof(DROPFILES) + (file_list.len + 2)*sizeof(wchar_t));
    DROPFILES *drop_files = GlobalLock(drop_files_handle);
    drop_files->pFiles = sizeof(DROPFILES);
    drop_files->fWide = True;
    void *files_ptr = PtrFromInt(IntFromPtr(drop_files) + drop_files->pFiles);
    MemoryCopy(files_ptr, file_list.buffer, file_list.len*sizeof(wchar_t));
    GlobalUnlock(drop_files_handle);
    SetClipboardData(CF_HDROP, drop_files_handle);
    CloseClipboard();
    
    ArenaTempEnd(scratch);
}

Function void
OS_ClipboardStringListSet(S8List strings)
{
    ArenaTemp scratch = OS_TCtxScratchBegin(0, 0);
    
    if(!OpenClipboard(0))
    {
        W32_TCtxPushLastErrorFormattedCond(False);
    }
    EmptyClipboard();
    S16 string_list = W32_Win32StringListFromS8List(scratch.arena, strings);
    HGLOBAL mem_handle = GlobalAlloc(GHND, (string_list.len + 2)*sizeof(wchar_t));
    wchar_t *mem = GlobalLock(mem_handle);
    MemoryCopy(mem, string_list.buffer, string_list.len*sizeof(wchar_t));
    GlobalUnlock(mem_handle);
    SetClipboardData(w32_clip_string_list_format, mem_handle);
    SetClipboardData(CF_UNICODETEXT, mem_handle);
    CloseClipboard();
    
    ArenaTempEnd(scratch);
}

Function S8
OS_ClipboardTextGet(Arena *arena)
{
    S8 result = {0};
    
    OpenClipboard(0);
    HGLOBAL mem_handle = GetClipboardData(CF_UNICODETEXT);
    if(0 != mem_handle)
    {
        void *mem = GlobalLock(mem_handle);
        if(0 != mem)
        {
            result = S8FromS16(arena, CStringAsS16(mem));
        }
        GlobalUnlock(mem_handle);
    }
    CloseClipboard();
    
    return result;
}

Function S8List
OS_ClipboardFilesGet(Arena *arena)
{
    S8List result = {0};
    
    OpenClipboard(0);
    HGLOBAL mem_handle = GetClipboardData(CF_HDROP);
    if(0 != mem_handle)
    {
        DROPFILES *mem = GlobalLock(mem_handle);
        if(0 != mem)
        {
            wchar_t *string_list = PtrFromInt(IntFromPtr(mem) + mem->pFiles);
            result = W32_S8ListFromWin32StringList(arena, string_list);
        }
        GlobalUnlock(mem_handle);
    }
    CloseClipboard();
    
    return result;
}

Function S8List
OS_ClipboardStringListGet(Arena *arena)
{
    S8List result = {0};
    
    if(!OpenClipboard(0))
    {
        W32_TCtxPushLastErrorFormattedCond(False);
    }
    HGLOBAL mem_handle = GetClipboardData(w32_clip_string_list_format);
    if(0 != mem_handle)
    {
        wchar_t *mem = GlobalLock(mem_handle);
        if(0 != mem)
        {
            result = W32_S8ListFromWin32StringList(arena, mem);
        }
        GlobalUnlock(mem_handle);
    }
    CloseClipboard();
    
    return result;
}

//~NOTE(tbt): console

Function void
OS_ConsoleOutputS8(S8 string)
{
    ArenaTemp scratch = OS_TCtxScratchBegin(0, 0);
    S16 string_s16 = S16FromS8(scratch.arena, string);
    if(INVALID_HANDLE_VALUE != w32_console_handle)
    {
        WriteConsoleW(w32_console_handle, string_s16.buffer, string_s16.len, 0, 0);
    }
    else
    {
        OS_TCtxErrorPush(S8("Logging error - w32_console_handle == INVALID_HANDLE_VALUE"));
    }
    OutputDebugStringW(string_s16.buffer);
    ArenaTempEnd(scratch);
}

Function void
OS_ConsoleOutputS16(S16 string)
{
    if(INVALID_HANDLE_VALUE != w32_console_handle)
    {
        WriteConsoleW(w32_console_handle, string.buffer, string.len, 0, 0);
        OutputDebugStringW(string.buffer);
    }
}

Function S8List
OS_CmdLineGet(Arena *arena)
{
    S8List result = {0};
    
    I32 arguments_count;
    LPWSTR *arguments = CommandLineToArgvW(GetCommandLineW(), &arguments_count);
    
    for(U64 argument_index = 0;
        argument_index < arguments_count;
        argument_index += 1)
    {
        S8Node *node = ArenaPushNoZero(arena, sizeof(S8Node), _Alignof(S8Node));
        node->string = S8FromS16(arena, CStringAsS16(arguments[argument_index]));
        S8ListAppendExplicit(&result, node);
    }
    
    LocalFree(arguments);
    
    return result;
}

//~NOTE(tbt): entropy

Function void
OS_WriteEntropy(void *buffer, U64 size)
{
    HCRYPTPROV ctx = 0;
    CryptAcquireContextW(&ctx, 0, 0, PROV_DSS, CRYPT_VERIFYCONTEXT);
    CryptGenRandom(ctx, size, buffer);
    CryptReleaseContext(ctx, 0);
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
    return VirtualAlloc(0, size, MEM_RESERVE, PAGE_NOACCESS);
}

Function void
OS_MemoryRelease(void *memory, U64 size)
{
    VirtualFree(memory, 0, MEM_RELEASE);
}

Function void
OS_MemoryCommit(void *memory, U64 size)
{
    VirtualAlloc(memory, size, MEM_COMMIT, PAGE_READWRITE);
}

Function void
OS_MemoryDecommit(void *memory, U64 size)
{
    VirtualFree(memory, size, MEM_DECOMMIT);
}

Function void
OS_MemoryProtect(void *memory, U64 size, DataAccessFlags flags)
{
    DWORD c = PAGE_NOACCESS;
    if((DataAccessFlags_Read | DataAccessFlags_Write | DataAccessFlags_Execute) == flags)
    {
        c = PAGE_EXECUTE_READWRITE;
    }
    else if((DataAccessFlags_Read | DataAccessFlags_Execute) == flags)
    {
        c = PAGE_EXECUTE_READ;
    }
    else if((DataAccessFlags_Execute) == flags)
    {
        c = PAGE_EXECUTE;
    }
    else if((DataAccessFlags_Read | DataAccessFlags_Write) == flags)
    {
        c = PAGE_READWRITE;
    }
    else if((DataAccessFlags_Read) == flags)
    {
        c = PAGE_READONLY;
    }
    else if(flags != 0)
    {
        OS_TCtxErrorPush(S8("Invalid combination of DataAccessFlags specified"));
    }
    
    DWORD old_protect = 0;
    VirtualProtect(memory, size, c, &old_protect);
}

//~NOTE(tbt): shared libraries

Function OpaqueHandle
OS_SharedLibOpen(S8 filename)
{
    OpaqueHandle result = {0};
    ArenaTemp scratch = OS_TCtxScratchBegin(0, 0);
    S16 filename_s16 = S16FromS8(scratch.arena, filename);
    HMODULE module = LoadLibraryW(filename_s16.buffer);
    result.p = IntFromPtr(module);
    ArenaTempEnd(scratch);
    return result;
}

Function void *
OS_SharedLibSymbolLookup(OpaqueHandle lib, char *symbol)
{
    Assert(0 != lib.p);
    void *result = 0;
    HMODULE module = PtrFromInt(lib.p);
    FARPROC fn = GetProcAddress(module, symbol);
    result = (void *)fn;
    return result;
}

Function void
OS_SharedLibClose(OpaqueHandle lib)
{
    Assert(0 != lib.p);
    HMODULE module = PtrFromInt(lib.p);
    FreeLibrary(module);
}

//~NOTE(tbt): time

Function DateTime
OS_NowUTC(void)
{
    DateTime result;
    SYSTEMTIME system_time;
    GetSystemTime(&system_time);
    result = W32_DateTimeFromSystemTime(system_time);
    return result;
}

Function DateTime
OS_NowLTC(void)
{
    DateTime universal_time = OS_NowUTC();
    DateTime result = OS_LTCFromUTC(universal_time);
    return result;
}

Function DateTime
OS_LTCFromUTC(DateTime universal_time)
{
    DateTime result;
    
    SYSTEMTIME ut_system_time, lt_system_time;
    FILETIME ut_file_time, lt_file_time;
    ut_system_time = W32_SystemTimeFromDateTime(universal_time);
    SystemTimeToFileTime(&ut_system_time, &ut_file_time);
    FileTimeToLocalFileTime(&ut_file_time, &lt_file_time);
    FileTimeToSystemTime(&lt_file_time, &lt_system_time);
    result = W32_DateTimeFromSystemTime(lt_system_time);
    
    return result;
}

Function DateTime
OS_UTCFromLTC(DateTime local_time)
{
    DateTime result;
    
    SYSTEMTIME ut_system_time, lt_system_time;
    FILETIME ut_file_time, lt_file_time;
    lt_system_time = W32_SystemTimeFromDateTime(local_time);
    SystemTimeToFileTime(&lt_system_time, &lt_file_time);
    LocalFileTimeToFileTime(&lt_file_time, &ut_file_time);
    FileTimeToSystemTime(&ut_file_time, &ut_system_time);
    result = W32_DateTimeFromSystemTime(ut_system_time);
    
    return result;
}

Function void
OS_Sleep(U64 milliseconds)
{
    Sleep(milliseconds);
}

Function U64
OS_TimeInMicroseconds(void)
{
    U64 result = 0;
    LARGE_INTEGER perf_counter;
    if(QueryPerformanceCounter(&perf_counter))
    {
        result = (perf_counter.QuadPart * 1000000llu) / w32_t_perf_counter_ticks_per_second;
    }
    return result;
}

//~NOTE(tbt): system dialogues

Function S8
OS_FilenameFromSaveDialogue(Arena *arena, S8 title, S8List extensions)
{
    ArenaTemp scratch = OS_TCtxScratchBegin(&arena, 1);
    
    S8 result = {0};
    
    S8List filters_list = {0};
    for(S8ListForEach(extensions, s))
    {
        S8ListAppendFmt(scratch.arena, &filters_list, "%.*s files (*.%.*s)", FmtS8(s->string), FmtS8(s->string));
        S8ListAppendFmt(scratch.arena, &filters_list, "*.%.*s", FmtS8(s->string));
    }
    S16 filters = W32_Win32StringListFromS8List(scratch.arena, filters_list);
    
    wchar_t file[MAX_PATH] = {0};
    OPENFILENAMEW open_file_name =
    {
        .lStructSize = sizeof(open_file_name),
        .hwndOwner = 0,
        .lpstrFilter = filters.buffer,
        .lpstrFile = file,
        .nMaxFile = ArrayCount(file),
        .lpstrInitialDir = (LPCWSTR)0,
        .lpstrTitle = S16FromS8(scratch.arena, title).buffer,
        .Flags = 0,
    };
    
    if(GetSaveFileNameW(&open_file_name))
    {
        S8 filename = S8FromS16(scratch.arena, CStringAsS16(open_file_name.lpstrFile));
        
        S8 extension = S8ListS8FromIndex(extensions, open_file_name.nFilterIndex - 1);
        if(S8Match(ExtensionFromFilename(filename), extension, MatchFlags_CaseInsensitive))
        {
            result = S8Clone(arena, filename);
        }
        else
        {
            result = S8FromFmt(arena, "%.*s.%.*s", FmtS8(filename), FmtS8(extension));
        }
    }
    
    ArenaTempEnd(scratch);
    
    return result;
}

Function S8
OS_FilenameFromOpenDialogue(Arena *arena, S8 title, S8List extensions)
{
    ArenaTemp scratch = OS_TCtxScratchBegin(&arena, 1);
    
    S8 result = {0};
    
    S8List filters_list = {0};
    for(S8ListForEach(extensions, s))
    {
        S8ListAppendFmt(scratch.arena, &filters_list, "%.*s files (*.%.*s)", FmtS8(s->string), FmtS8(s->string));
        S8ListAppendFmt(scratch.arena, &filters_list, "*.%.*s", FmtS8(s->string));
    }
    S16 filters = W32_Win32StringListFromS8List(scratch.arena, filters_list);
    
    wchar_t file[MAX_PATH] = {0};
    OPENFILENAMEW open_file_name =
    {
        .lStructSize = sizeof(open_file_name),
        .hwndOwner = 0,
        .lpstrFilter = filters.buffer,
        .lpstrFile = file,
        .nMaxFile = ArrayCount(file),
        .lpstrInitialDir = (LPCWSTR)0,
        .lpstrTitle = S16FromS8(scratch.arena, title).buffer,
        .Flags = 0,
    };
    
    if(GetOpenFileNameW(&open_file_name))
    {
        result = S8FromS16(arena, CStringAsS16(open_file_name.lpstrFile));
    }
    
    ArenaTempEnd(scratch);
    
    return result;
}

//~NOTE(tbt): system queries

Function U64
OS_ProcessorsCount(void)
{
    SYSTEM_INFO system_info;
    GetSystemInfo(&system_info);
    return system_info.dwNumberOfProcessors;
}

Function U64 OS_PageSize(void)
{
    SYSTEM_INFO system_info;
    GetSystemInfo(&system_info);
    return system_info.dwPageSize;
}
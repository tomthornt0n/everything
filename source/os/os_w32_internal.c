
Function U64
W32_SizeFromFileHandle(HANDLE file_handle)
{
    DWORD hi_size, lo_size;
    lo_size = GetFileSize(file_handle, &hi_size);
    U64 result = W32_U64FromHiAndLoWords(hi_size, lo_size);
    return result;
}

Function void
W32_TCtxPushLastErrorFormattedCond(B32 success)
{
    if(!success)
    {
        S16 err = W32_FormatLastError();
        ArenaTemp scratch = OS_TCtxScratchBegin(0, 0);
        OS_TCtxErrorPush(S8FromS16(scratch.arena, err));
        ArenaTempEnd(scratch);
        LocalFree((void *)err.buffer);
    }
}

Function void
W32_AssertLogCallback(char *title, char *msg)
{
    OS_ConsoleOutputFmt("%s\n", msg);
    MessageBoxA(0, msg, title, MB_ICONERROR | MB_OK);
}

Function U64
W32_U64FromHiAndLoWords(DWORD hi,
                        DWORD lo)
{
    U64 result = 0;
    result |= ((U64)hi) << 32;
    result |= ((U64)lo) <<  0;
    return result;
}

Function S16
W32_Win32StringListFromS8List(Arena *arena, S8List list)
{
    ArenaTemp scratch = OS_TCtxScratchBegin(&arena, 1);
    
    S8List builder_list = {0};
    for(S8ListForEach(list, s))
    {
        S8 _s = S8Clone(scratch.arena, s->string);
        _s.len += 1;
        S8ListAppend(scratch.arena, &builder_list, _s);
    }
    S8ListAppend(scratch.arena, &builder_list, S8("\0"));
    
    S8 joined_list = S8ListJoin(scratch.arena, builder_list);
    S16 result = S16FromS8(arena, joined_list);
    
    ArenaTempEnd(scratch);
    return result;
}

Function S16
W32_FileOpListFromS8List(Arena *arena, S8List list)
{
    ArenaTemp scratch = OS_TCtxScratchBegin(&arena, 1);
    
    S8List _list = {0};
    for(S8ListForEach(list, s))
    {
        S8 _s = OS_AbsolutePathFromRelativePath(scratch.arena, s->string);
        _s.len += 1;
        _s = S8Clone(scratch.arena, _s);
        ((char *)_s.buffer)[_s.len - 1] = '\0';
        S8ListAppend(scratch.arena, &_list, _s);
    }
    S8ListAppend(scratch.arena, &_list, S8("\0"));
    
    S8 joined_list = S8ListJoin(scratch.arena, _list);
    S16 result = S16FromS8(arena, joined_list);
    
    ArenaTempEnd(scratch);
    return result;
}

Function S8List
W32_S8ListFromWin32StringList(Arena *arena,
                              wchar_t *string_list)
{
    S8List result = {0};
    for(;;)
    {
        S16 str = CStringAsS16(string_list);
        string_list += str.len + 1;
        if(0 == str.len)
        {
            break;
        }
        else
        {
            S8Node *node = PushArray(arena, S8Node, 1);
            node->string = S8FromS16(arena, str);
            S8ListAppendExplicit(&result, node);
        }
    }
    return result;
}

Function S16
W32_FormatLastError(void)
{
    DWORD dw = GetLastError(); 
    
    void *buffer;
    FormatMessage(FORMAT_MESSAGE_ALLOCATE_BUFFER | 
                  FORMAT_MESSAGE_FROM_SYSTEM |
                  FORMAT_MESSAGE_IGNORE_INSERTS,
                  NULL,
                  dw,
                  MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
                  (LPTSTR) &buffer,
                  0, NULL );
    
    S16 result = CStringAsS16(buffer);
    return result;
}

Function OS_FilePropertiesFlags
W32_FilePropertiesFlagsFromFileAttribs(DWORD attribs)
{
    OS_FilePropertiesFlags result = OS_FilePropertiesFlags_Exists;
    if(attribs & FILE_ATTRIBUTE_DIRECTORY)
    {
        result |= OS_FilePropertiesFlags_IsDirectory;
    }
    if(attribs & FILE_ATTRIBUTE_HIDDEN)
    {
        result |= OS_FilePropertiesFlags_Hidden;
    }
    return result;
}

Function DataAccessFlags
W32_DataAccessFlagsFromFileAttribs(DWORD attribs)
{
    DataAccessFlags result = DataAccessFlags_Read | DataAccessFlags_Execute;
    if(!(attribs & FILE_ATTRIBUTE_READONLY))
    {
        result |= DataAccessFlags_Write;
    }
    return result;
}

Function DWORD CALLBACK
W32_ThreadProc(LPVOID param)
{
    W32_ThreadProcArgs *args = param;
    
    LONG logical_thread_index;
    ReleaseSemaphore(w32_threads_count, 1, &logical_thread_index);
    OS_ThreadContextAlloc(&args->tctx, logical_thread_index + 1);
    OS_TCtxSet(&args->tctx);
    CoInitializeEx(0, COINIT_MULTITHREADED | COINIT_DISABLE_OLE1DDE);
    SetThreadDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);
    
    args->init(args->user_data);
    
#if BuildModeDebug
    OS_ConsoleOutputFmt("Thread %ld reported the following errors:\n", logical_thread_index);
    for(ErrorMsg *error = OS_TCtxErrorFirst(); 0 != error; error = error->next)
    {
        OS_ConsoleOutputLine(error->string);
    }
#endif
    
    OS_ThreadContextRelease(&args->tctx);
    LocalFree(param);
    WaitForSingleObject(w32_threads_count, 0);
    
    return 0;
}

Function DateTime
W32_DateTimeFromSystemTime(SYSTEMTIME system_time)
{
    DateTime result;
    result.year = system_time.wYear;
    result.mon = system_time.wMonth - 1;
    result.day = system_time.wDay - 1;
    result.hour = system_time.wHour;
    result.min = system_time.wMinute;
    result.sec = system_time.wSecond;
    result.msec = system_time.wMilliseconds;
    return result;
}

Function SYSTEMTIME
W32_SystemTimeFromDateTime(DateTime date_time)
{
    SYSTEMTIME result = {0};
    result.wYear = date_time.year;
    result.wMonth = date_time.mon + 1;
    result.wDay = date_time.day + 1;
    result.wHour = date_time.hour;
    result.wMinute = date_time.min;
    result.wSecond = date_time.sec;
    result.wMilliseconds = date_time.msec;
    return result;
}

Function DenseTime
W32_DenseTimeFromFileTime(FILETIME file_time)
{
    DenseTime result;
    SYSTEMTIME system_time = {0};
    FileTimeToSystemTime(&file_time, &system_time);
    DateTime date_time = W32_DateTimeFromSystemTime(system_time);
    result = DenseTimeFromDateTime(date_time);
    return result;
}

Function void
W32_SRWLockAllocatorInit (void)
{
    for(U64 i = 0; i < ArrayCount(w32_srw_locks) - 1; i += 1)
    {
        w32_srw_locks[i].next_free = &w32_srw_locks[i + 1];
    }
    w32_srw_locks[ArrayCount(w32_srw_locks) - 1].next_free = 0;
    w32_srw_locks_free_list = & w32_srw_locks[0];
}

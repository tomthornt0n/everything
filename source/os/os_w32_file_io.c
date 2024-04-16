
Function S8
OS_FileReadEntire(Arena *arena, S8 filename)
{
    S8 result = {0};
    
    ArenaTemp scratch = OS_TCtxScratchBegin(&arena, 1);
    S16 filename_s16 = S16FromS8(scratch.arena, filename);
    HANDLE file_handle = CreateFileW(filename_s16.buffer,
                                     GENERIC_READ,
                                     0, 0,
                                     OPEN_EXISTING,
                                     FILE_ATTRIBUTE_NORMAL,
                                     0);
    if(INVALID_HANDLE_VALUE != file_handle)
    {
        U64 n_total_bytes_to_read = W32_SizeFromFileHandle(file_handle);
        
        ArenaTemp checkpoint = ArenaTempBegin(arena);
        
        char *buffer = ArenaPushNoZero(arena, n_total_bytes_to_read + 1, _Alignof(char));
        
        unsigned char *cursor = buffer;
        unsigned char *end = buffer + n_total_bytes_to_read;
        
        B32 success = True;
        while(cursor < end && success)
        {
            DWORD n_bytes_to_read = Min(UINT_MAX, end - cursor);
            
            DWORD n_bytes_read;
            if(ReadFile(file_handle, cursor, n_bytes_to_read, &n_bytes_read, 0))
            {
                cursor += n_bytes_read;
            }
            else
            {
                success = False;
                W32_TCtxPushLastErrorFormattedCond(success);
            }
        }
        
        if(success)
        {
            result.len = n_total_bytes_to_read;
            result.buffer = buffer;
        }
        else
        {
            ArenaTempEnd(checkpoint);
        }
        
        CloseHandle(file_handle);
    }
    else
    {
        W32_TCtxPushLastErrorFormattedCond(False);
    }
    
    ArenaTempEnd(scratch);
    return result;
}

Function S8
OS_FileReadTextEntire(Arena *arena, S8 filename)
{
    ArenaTemp scratch = OS_TCtxScratchBegin(&arena, 1);
    S8 raw = OS_FileReadEntire(scratch.arena, filename);
    S8 result = S8LFFromCRLF(arena, raw);
    ArenaTempEnd(scratch);
    
    return result;
}

Function B32
OS_FileWriteEntire(S8 filename, S8 data)
{
    B32 success = True;
    
    ArenaTemp scratch = OS_TCtxScratchBegin(0, 0);
    S16 filename_s16 = S16FromS8(scratch.arena, filename);
    HANDLE file_handle = CreateFileW(filename_s16.buffer,
                                     GENERIC_WRITE,
                                     0, 0,
                                     CREATE_ALWAYS,
                                     FILE_ATTRIBUTE_NORMAL,
                                     0);
    if(INVALID_HANDLE_VALUE != file_handle)
    {
        const unsigned char *cursor = data.buffer;
        const unsigned char *end = data.buffer + data.len;
        
        while(cursor < end && success)
        {
            U64 _n_bytes_to_write = end - cursor;
            DWORD n_bytes_to_write = Min(UINT_MAX, _n_bytes_to_write);
            
            DWORD n_bytes_written;
            if(WriteFile(file_handle, cursor, n_bytes_to_write, &n_bytes_written, 0))
            {
                cursor += n_bytes_written;
            }
            else
            {
                success = False;
                W32_TCtxPushLastErrorFormattedCond(success);
            }
        }
        CloseHandle(file_handle);
    }
    else
    {
        success = False;
        W32_TCtxPushLastErrorFormattedCond(success);
    }
    
    ArenaTempEnd(scratch);
    return success;
}

Function OS_FileProperties
OS_FilePropertiesFromFilename(S8 filename)
{
    OS_FileProperties result = {0};
    
    ArenaTemp scratch = OS_TCtxScratchBegin(0, 0);
    
    S16 filename_s16 = S16FromS8(scratch.arena, filename);
    
    WIN32_FILE_ATTRIBUTE_DATA attribs = {0};
    if(GetFileAttributesExW(filename_s16.buffer, GetFileExInfoStandard, &attribs))
    {
        result.flags = W32_FilePropertiesFlagsFromFileAttribs(attribs.dwFileAttributes);
        result.size = W32_U64FromHiAndLoWords(attribs.nFileSizeHigh, attribs.nFileSizeLow);
        result.access_flags = W32_DataAccessFlagsFromFileAttribs(attribs.dwFileAttributes);
        result.creation_time = W32_DenseTimeFromFileTime(attribs.ftCreationTime);
        result.access_time = W32_DenseTimeFromFileTime(attribs.ftLastAccessTime);
        result.write_time = W32_DenseTimeFromFileTime(attribs.ftLastWriteTime);
    }
    ArenaTempEnd(scratch);
    return result;
}

Function B32
OS_FileDelete(S8 filename)
{
    B32 success;
    ArenaTemp scratch = OS_TCtxScratchBegin(0, 0);
    S16 filename_s16 = S16FromS8(scratch.arena, filename);
    success = DeleteFileW(filename_s16.buffer);
    ArenaTempEnd(scratch);
    W32_TCtxPushLastErrorFormattedCond(success);
    return success;
}

Function B32
OS_FileMove(S8 filename, S8 new_filename)
{
    B32 success;
    ArenaTemp scratch = OS_TCtxScratchBegin(0, 0);
    S16 filename_s16 = S16FromS8(scratch.arena, filename);
    S16 new_filename_s16 = S16FromS8(scratch.arena, new_filename);
    success = MoveFileExW(filename_s16.buffer, new_filename_s16.buffer, MOVEFILE_COPY_ALLOWED);
    ArenaTempEnd(scratch);
    W32_TCtxPushLastErrorFormattedCond(success);
    return success;
}

Function B32
OS_DirectoryMake(S8 filename)
{
    B32 success;
    ArenaTemp scratch = OS_TCtxScratchBegin(0, 0);
    S16 filename_s16 = S16FromS8(scratch.arena, filename);
    success = CreateDirectoryW(filename_s16.buffer, 0);
    ArenaTempEnd(scratch);
    W32_TCtxPushLastErrorFormattedCond(success);
    return success;
}

Function B32
OS_DirectoryDelete(S8 filename)
{
    B32 success;
    ArenaTemp scratch = OS_TCtxScratchBegin(0, 0);
    S16 filename_s16 = S16FromS8(scratch.arena, filename);
    success = RemoveDirectoryW(filename_s16.buffer);
    ArenaTempEnd(scratch);
    W32_TCtxPushLastErrorFormattedCond(success);
    return success;
}

Function OS_FileIterator *
OS_FileIteratorBegin(Arena *arena, S8 directory)
{
    W32_FileIterator *result = PushArray(arena, W32_FileIterator, 1);
    result->_.directory = S8Clone(arena, directory);
    
    ArenaTemp scratch = OS_TCtxScratchBegin(&arena, 1);
    S16 directory_s16 = S16FromS8(scratch.arena, S8FromFmt(scratch.arena, "%.*s\\*", FmtS8(directory)));
    result->handle = FindFirstFileW(directory_s16.buffer, &result->find_data);
    ArenaTempEnd(scratch);
    
    return (OS_FileIterator *)result;
}

Function B32
OS_FileIteratorNext(Arena *arena, OS_FileIterator *iter)
{
    B32 result = False;
    
    ArenaTemp scratch = OS_TCtxScratchBegin(&arena, 1);
    
    W32_FileIterator *_iter = (W32_FileIterator *)iter;
    if(_iter->handle != 0 &&
       _iter->handle != INVALID_HANDLE_VALUE)
    {
        while(!_iter->is_done)
        {
            wchar_t *file_name = _iter->find_data.cFileName;
            //NOTE(lucas): Windows fails to get icons for reparse points at a 50% hit rate so right now lets ignore them...
            //maybe we could keep them and give them our own icon?
            B32 reparse_point = (_iter->find_data.dwFileAttributes & FILE_ATTRIBUTE_REPARSE_POINT) != 0;
            // NOTE(tbt): ommit the directories '.' and '..'
            B32 should_ommit = (file_name[0] == '.' && (file_name[1] == '\0' || (file_name[1] == '.' && file_name[2] == '\0'))) || reparse_point;
            
            if(!should_ommit)
            {
                iter->current_name = S8FromS16(arena, CStringAsS16(file_name));
                iter->current_full_path = OS_AbsolutePathFromRelativePath(arena, FilenamePush(scratch.arena, iter->directory, iter->current_name));
                iter->current_properties.size = W32_U64FromHiAndLoWords(_iter->find_data.nFileSizeHigh, _iter->find_data.nFileSizeLow);
                iter->current_properties.flags = W32_FilePropertiesFlagsFromFileAttribs(_iter->find_data.dwFileAttributes);
                iter->current_properties.access_flags = W32_DataAccessFlagsFromFileAttribs(_iter->find_data.dwFileAttributes);
                iter->current_properties.creation_time = W32_DenseTimeFromFileTime(_iter->find_data.ftCreationTime);
                iter->current_properties.access_time = W32_DenseTimeFromFileTime(_iter->find_data.ftLastAccessTime);
                iter->current_properties.write_time = W32_DenseTimeFromFileTime(_iter->find_data.ftLastWriteTime);
                result = True;
            }
            
            if(!FindNextFileW(_iter->handle, &_iter->find_data))
            {
                _iter->is_done = True;
            }
            
            if(!should_ommit)
            {
                break;
            }
        }
    }
    
    ArenaTempEnd(scratch);
    
    return result;
}

Function void
OS_FileIteratorEnd(OS_FileIterator *iter)
{
    W32_FileIterator *_iter = (W32_FileIterator *)iter;
    if(_iter->handle != 0 &&
       _iter->handle != INVALID_HANDLE_VALUE)
    {
        FindClose(_iter->handle);
        _iter->handle = 0;
    }
}

Function S8
OS_S8FromStdPath(Arena *arena, OS_StdPath path)
{
    S8 result = {0};
    
    ArenaTemp scratch = OS_TCtxScratchBegin(&arena, 1);
    switch(path)
    {
        case(OS_StdPath_CWD):
        {
            DWORD required_chars = GetCurrentDirectoryW(0, 0);
            wchar_t *buffer = ArenaPushNoZero(arena, required_chars*sizeof(buffer[0]), _Alignof(wchar_t));
            GetCurrentDirectoryW(required_chars, buffer);
            result = S8FromS16(arena, CStringAsS16(buffer));
        } break;
        
        case(OS_StdPath_ExecutableFile):
        case(OS_StdPath_ExecutableDir):
        {
            if(0 == OS_TCtxGet()->exe_path.buffer)
            {
                // NOTE(tbt): will fail if the length of the path to the exe > cap * grow_factor ^ grow_max
                
                U64 cap = 2048;
                I32 grow_max = 4;
                I32 grow_factor = 4;
                
                ArenaTemp before = ArenaTempBegin(scratch.arena);
                for(U64 r = 0; r < 4; r += 1, cap *= 4)
                {
                    wchar_t *buffer = ArenaPushNoZero(scratch.arena, cap*sizeof(*buffer), _Alignof(wchar_t));
                    DWORD size = GetModuleFileNameW(0, buffer, cap);
                    if(size == cap && GetLastError() == ERROR_INSUFFICIENT_BUFFER)
                    {
                        ArenaTempEnd(before);
                    }
                    else
                    {
                        OS_TCtxGet()->exe_path = S8FromS16(OS_TCtxGet()->permanent_arena, CStringAsS16(buffer));
                        break;
                    }
                }
            }
            
            if(0 != OS_TCtxGet()->exe_path.buffer)
            {
                result = OS_TCtxGet()->exe_path;
                if(OS_StdPath_ExecutableDir == path)
                {
                    result = FilenamePop(result);
                }
                result = S8Clone(arena, result);
            }
        } break;
        
        case(OS_StdPath_Home):
        {
            HANDLE token_handle = GetCurrentProcessToken();
            DWORD size = 0;
            GetUserProfileDirectoryW(token_handle, 0, &size);
            wchar_t *buffer = ArenaPushNoZero(scratch.arena, size*sizeof(*buffer), _Alignof(wchar_t));
            if(GetUserProfileDirectoryW(token_handle, buffer, &size))
            {
                result = S8FromS16(arena, CStringAsS16(buffer));
            }
        } break;
        
        case(OS_StdPath_Config):
        {
            wchar_t *buffer;
            SHGetKnownFolderPath(&FOLDERID_LocalAppData, 0, 0, &buffer);
            result = S8FromS16(arena, CStringAsS16(buffer));
            CoTaskMemFree(buffer);
        } break;
        
        case(OS_StdPath_Temp):
        {
            DWORD size = GetTempPathW(0, 0);
            wchar_t *buffer = ArenaPushNoZero(scratch.arena, size*sizeof(*buffer), _Alignof(wchar_t));
            GetTempPathW(size, buffer);
            result = S8FromS16(arena, CStringAsS16(buffer));
            result.len -= 1; // NOTE(tbt): remove trailing '\\'
        } break;
    }
    ArenaTempEnd(scratch);
    
    return result;
}

Function void
OS_FileExec(S8 filename, OS_FileExecVerb verb)
{
    ArenaTemp scratch = OS_TCtxScratchBegin(0, 0);
    
    const wchar_t *verb_table[OS_FileExecVerb_MAX] =
    {
        [OS_FileExecVerb_Default] = 0,
        [OS_FileExecVerb_Open] = L"open",
        [OS_FileExecVerb_Edit] = L"edit",
        [OS_FileExecVerb_Print] = L"print",
    };
    S16 filename_s16 = S16FromS8(scratch.arena, filename);
    ShellExecuteW(0, verb_table[verb], filename_s16.buffer, 0, 0, SW_NORMAL);
    
    ArenaTempEnd(scratch);
}

Function void
OS_CwdSet(S8 cwd)
{
    ArenaTemp scratch = OS_TCtxScratchBegin(0, 0);
    S16 cwd_s16 = S16FromS8(scratch.arena, cwd);
    SetCurrentDirectoryW(cwd_s16.buffer);
    ArenaTempEnd(scratch);
}

Function S8
OS_AbsolutePathFromRelativePath(Arena *arena, S8 path)
{
    S8 result;
    
    B32 is_drive_id = ((2 == path.len) && CharIsLetter(path.buffer[0]) && (':' == path.buffer[1]));
    if(is_drive_id)
    {
        result = S8FromFmt(arena, "%.*s\\", FmtS8(path));
    }
    else
    {
        ArenaTemp scratch = OS_TCtxScratchBegin(&arena, 1);
        S16 rel_s16 = S16FromS8(scratch.arena, path);
        U64 buf_size = GetFullPathNameW(rel_s16.buffer, 0, 0, 0)*sizeof(wchar_t);
        wchar_t *abs_s16 = ArenaPushNoZero(scratch.arena, buf_size, _Alignof(wchar_t));
        GetFullPathNameW(rel_s16.buffer, buf_size, abs_s16, 0);
        result = S8FromS16(arena, CStringAsS16(abs_s16));
        ArenaTempEnd(scratch);
    }
    
    return result;
}

Function OpaqueHandle
OS_FileWatchStart(S8 filename)
{
    OpaqueHandle result = {0};
    
    ArenaTemp scratch = OS_TCtxScratchBegin(0, 0);
    
    S16 filename_s16 = S16FromS8(scratch.arena, OS_AbsolutePathFromRelativePath(scratch.arena, filename));
    HANDLE file_watch_handle = FindFirstChangeNotificationW(filename_s16.buffer, False, FILE_NOTIFY_CHANGE_DIR_NAME | FILE_NOTIFY_CHANGE_FILE_NAME);
    Assert(INVALID_HANDLE_VALUE != file_watch_handle);
    result.p = IntFromPtr(file_watch_handle);
    
    ArenaTempEnd(scratch);
    return result;
}

Function B32
OS_FileWatchWait(OpaqueHandle watch, U64 timeout_milliseconds)
{
    B32 result = False;
    
    HANDLE handle = PtrFromInt(watch.p);
    
    if(0 != handle)
    {
        if(OS_FileWatchTimeoutInfinite == timeout_milliseconds)
        {
            timeout_milliseconds = INFINITE;
        }
        result = (WAIT_OBJECT_0 == WaitForSingleObject(handle, timeout_milliseconds));
        if(result)
        {
            while(WAIT_OBJECT_0 == WaitForSingleObject(handle, 0))
            {
                FindNextChangeNotification(handle);
            }
        }
    }
    return result;
}

Function void
OS_FileWatchStop(OpaqueHandle watch)
{
    HANDLE handle = PtrFromInt(watch.p);
    if(0 != handle)
    {
        FindCloseChangeNotification(handle);
    }
}

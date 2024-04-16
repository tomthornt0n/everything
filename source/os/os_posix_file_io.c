
Function OS_FileProperties
OS_FilePropertiesFromFilename(S8 filename)
{
    OS_FileProperties result = {0};
    struct stat sb;
    if(-1 != stat(filename.buffer, &sb))
    {
        result.flags = OS_FilePropertiesFlags_Exists;
        if(S_IFDIR == (sb.st_mode & S_IFMT))
        {
            result.flags |= OS_FilePropertiesFlags_IsDirectory;
        }
        if('.' == FilenameLast(filename).buffer[0])
        {
            result.flags |= OS_FilePropertiesFlags_Hidden;
        }
        
        if(0 == access(filename.buffer, R_OK))
        {
            result.access_flags |= DataAccessFlags_Read;
        }
        if(0 == access(filename.buffer, W_OK))
        {
            result.access_flags |= DataAccessFlags_Write;
        }
        if(0 == access(filename.buffer, X_OK))
        {
            result.access_flags |= DataAccessFlags_Execute;
        }
        
        result.size = sb.st_size;
#if BuildOSMac
        result.creation_time = POSIX_DenseTimeFromSeconds(sb.st_birthtimespec.tv_sec);
        result.access_time = POSIX_DenseTimeFromSeconds(sb.st_atimespec.tv_sec);
        result.write_time = POSIX_DenseTimeFromSeconds(sb.st_mtimespec.tv_sec);
#else
        result.creation_time = 0; // TODO(tbt): statx or something
        result.access_time = POSIX_DenseTimeFromSeconds(sb.st_atim.tv_sec);
        result.write_time = POSIX_DenseTimeFromSeconds(sb.st_mtim.tv_sec);
#endif
    }
    return result;
}

Function S8
OS_FileReadEntire(Arena *arena, S8 filename)
{
    S8 result = {0};
    
    int fd = open(filename.buffer, O_RDONLY);
    if(fd > 0)
    {
        size_t n_total_bytes_to_read = POSIX_FileSizeGet(fd);
        
        ArenaTemp checkpoint = ArenaTempBegin(arena);
        
        char *buffer = ArenaPush(arena, n_total_bytes_to_read + 1);
        
        char *cursor = buffer;
        const char *end = buffer + n_total_bytes_to_read;
        
        B32 success = True;
        while(cursor < end && success)
        {
            size_t n_bytes_to_read = end - cursor;
            ssize_t n_bytes_read = read(fd, cursor, n_bytes_to_read);
            if(n_bytes_read > 0)
            {
                cursor += n_bytes_read;
            }
            else
            {
                success = False;
            }
        }
        
        if(success)
        {
            result.buffer = buffer;
            result.len = n_total_bytes_to_read;
        }
        else
        {
            OS_TCtxErrorPush(S8("Could not read file."), CStringAsS8(strerror(errno)));
            ArenaTempEnd(checkpoint);
        }
        
        close(fd);
    }
    else
    {
        OS_TCtxErrorPush(S8("Could not read file."), CStringAsS8(strerror(errno)));
    }
    
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
    
    int fd = creat(filename.buffer, S_IRUSR | S_IWUSR | S_IRGRP);
    if(fd > 0)
    {
        const char *cursor = data.buffer;
        const char *end = data.buffer + data.len;
        
        while(cursor < end && success)
        {
            size_t n_bytes_to_write = end - cursor;
            ssize_t n_bytes_written = write(fd, cursor, n_bytes_to_write);
            if(n_bytes_written > 0)
            {
                cursor += n_bytes_written;
            }
            else
            {
                OS_TCtxErrorPush(S8("Could not write file."), CStringAsS8(strerror(errno)));
                success = False;
            }
        }
        
        close(fd);
    }
    else
    {
        OS_TCtxErrorPush(S8("Could not write file."), CStringAsS8(strerror(errno)));
        success = False;
    }
    
    return success;
}

Function B32
OS_FileDelete(S8 filename)
{
    B32 success = (0 == unlink(filename.buffer));
    return success;
}

Function B32
OS_FileMove(S8 filename, S8 new_filename)
{
    B32 success = (0 == rename(filename.buffer, new_filename.buffer));
    return success;
}

Function B32
OS_DirectoryMake(S8 filename)
{
    B32 success = (0 == mkdir(filename.buffer, S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH));
    return success;
}

Function B32
OS_DirectoryDelete(S8 filename)
{
    B32 success = (0 == rmdir(filename.buffer));
    return success;
}


Function OS_FileIterator *
OS_FileIteratorBegin(Arena *arena, S8 directory)
{
    POSIX_FileIterator *result = ArenaPush(arena, sizeof(*result));
    result->_.directory = S8Clone(arena, directory);
    result->d = opendir(directory.buffer);
    return (OS_FileIterator *)result;
}

Function B32
OS_FileIteratorNext(Arena *arena, OS_FileIterator *iter)
{
    B32 result = False;
    ArenaTemp scratch = OS_TCtxScratchBegin(&arena, 1);
    
    POSIX_FileIterator *_iter = (POSIX_FileIterator *)iter;
    if(_iter->d != 0)
    {
        while(!_iter->is_done)
        {
            _iter->dir = readdir(_iter->d);
            if(0 == _iter->dir)
            {
                _iter->is_done = True;
            }
            else
            {
                S8 filename = CStringAsS8(_iter->dir->d_name);
                
                B32 should_ommit = ((filename.len >= 2) &&
                                    (S8Match(filename, S8("."), MatchFlags_RightSideSloppy)) ||
                                    (S8Match(filename, S8(".."), MatchFlags_RightSideSloppy)));
                
                if(!should_ommit)
                {
                    S8 full_path = FilenamePush(scratch.arena, iter->directory, filename);
                    _iter->_.current_name = S8Clone(arena, filename);
                    _iter->_.current_full_path = OS_AbsolutePathFromRelativePath(arena, full_path);
                    _iter->_.current_properties = OS_FilePropertiesFromFilename(_iter->_.current_full_path);
                    result = True;
                    break;
                }
            }
        }
    }
    
    ArenaTempEnd(scratch);
    return result;
}

Function void
OS_FileIteratorEnd(OS_FileIterator *iter)
{
    POSIX_FileIterator *_iter = (POSIX_FileIterator *)iter;
    if(0 != _iter->d)
    {
        closedir(_iter->d);
    }
}

Function S8
OS_S8FromStdPath(Arena *arena, OS_StdPath path)
{
    S8 result = {0};
    
    ArenaTemp scratch = OS_TCtxScratchBegin(&arena, 1);
    switch(path)
    {
        default:
        {
            OS_TCtxErrorPush(S8("Error getting path."), S8("Invalid OS_StdPath passed in"));
        } break;
        
        case(OS_StdPath_CWD):
        {
            // NOTE(tbt): will fail if the length of the cwd > cap * grow_factor ^ grow_max
            
            size_t cap = 2048;
            int grow_max = 4;
            int grow_factor = 4;
            
            ArenaTemp before = ArenaTempBegin(scratch.arena);
            for(size_t r = 0; r < grow_max; r += 1, cap *= grow_factor)
            {
                char *buffer = ArenaPush(scratch.arena, cap);
                if(0 == getcwd(buffer, cap) && ERANGE == errno)
                {
                    ArenaTempEnd(before);
                }
                else
                {
                    result = S8Clone(arena, CStringAsS8(buffer));
                    break;
                }
            }
        } break;
        
        case(OS_StdPath_ExecutableFile):
        case(OS_StdPath_ExecutableDir):
        {
            if(0 == OS_TCtxGet()->exe_path.buffer)
            {
                // NOTE(tbt): will fail if the length of the path to the exe > cap * grow_factor ^ grow_max
                
                size_t cap = 2048;
                int grow_max = 4;
                int grow_factor = 4;
                
                ArenaTemp before = ArenaTempBegin(scratch.arena);
                for(size_t r = 0; r < grow_max; r += 1, cap *= grow_factor)
                {
                    char *buffer = ArenaPush(scratch.arena, cap);
                    if(cap == readlink("/proc/self/exe", buffer, cap))
                    {
                        ArenaTempEnd(before);
                    }
                    else
                    {
                        OS_TCtxGet()->exe_path = S8Clone(OS_TCtxGet()->permanent_arena, CStringAsS8(buffer));
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
            char *home = getenv("HOME");
            if(0 != home)
            {
                result = S8Clone(arena, CStringAsS8(home));
            }
        } break;
        
        case(OS_StdPath_Config):
        {
            char *config = getenv("XDG_CONFIG_HOME");
            if(0 == config)
            {
                result = FilenamePush(arena, OS_StdPathGet(scratch.arena, OS_StdPath_Home), S8(".config"));
            }
            else
            {
                result = S8Clone(arena, CStringAsS8(config));
            }
        } break;
        
        case(OS_StdPath_Temp):
        {
            result = S8("/tmp");
        } break;
    }
    ArenaTempEnd(scratch);
    
    return result;
}

Function void
OS_CwdSet(S8 cwd)
{
    ArenaTemp scratch = OS_TCtxScratchBegin(0, 0);
    S8 s = S8Clone(scratch.arena, cwd);
    chdir(s.buffer);
    ArenaTempEnd(scratch);
}

Function S8
OS_AbsolutePathFromRelativePath(Arena *arena, S8 path)
{
    ArenaTemp scratch = OS_TCtxScratchBegin(0, 0);
    S8 s = S8Clone(scratch.arena, path);
    char *absolute_path = realpath(s.buffer, ArenaPush(arena, PATH_MAX));
    S8 result = CStringAsS8(absolute_path);
    ArenaTempEnd(scratch);
    return result;
}

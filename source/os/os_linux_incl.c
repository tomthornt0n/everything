
#include <sys/inotify.h>

//~NOTE(tbt): clipboard

Function void
OS_ClipboardTextSet(S8 string)
{
    Assert(0);
}

Function void
OS_ClipboardFilesSet(S8List filenames)
{
    Assert(0);
}

Function void
OS_ClipboardStringListSet(S8List strings)
{
    Assert(0);
}

Function S8
OS_ClipboardTextGet(Arena *arena)
{
    Assert(0);
}

Function S8List
OS_ClipboardFilesGet(Arena *arena)
{
    Assert(0);
}

Function S8List
OS_ClipboardStringListGet(Arena *arena)
{
    Assert(0);
}

//~NOTE(tbt): console

Function S8List
OS_CmdLineGet(Arena *arena)
{
	S8List result = {0};
    
    ArenaTemp scratch = OS_TCtxScratchBegin(&arena, 1);
    int cmdline_fd = open("/proc/self/cmdline", O_RDONLY);
    
    char buf[4096] = {0};
    if(read(cmdline_fd, buf, sizeof(buf)) > 0)
    {
        
        char *cur = buf;
        while('\0' != *cur && cur < buf + sizeof(buf))
        {
            S8 str = CStringAsS8(cur);
            cur += str.len + 1;
            S8ListAppend(arena, &result, str);
        }
    }
    
    close(cmdline_fd);
    ArenaTempEnd(scratch);
    
    return result;
}

//~NOTE(tbt): synchronisation primitives

Function OpaqueHandle
OS_SemaphoreAlloc(I32 initial)
{
    sem_t *sem = malloc(sizeof(*sem));
    sem_init(sem, False, initial);
    OpaqueHandle result = { .p = IntFromPtr(sem), };
    return result;
}

Function void
OS_SemaphoreSignal(OpaqueHandle sem)
{
	sem_t *s = PtrFromInt(sem.p);
    sem_post(s);
}

Function void
OS_SemaphoreWait(OpaqueHandle sem)
{
	sem_t *s = PtrFromInt(sem.p);
	int r;
    do {
        r = sem_wait(s);
    } while (r == -1 && errno == EINTR);
}

Function void
OS_SemaphoreRelease(OpaqueHandle sem)
{
	sem_t *s = PtrFromInt(sem.p);
    sem_destroy(s);
    free(s);
}

//~NOTE(tbt): opening/executing files

Function void
OS_FileExec(S8 filename, OS_FileExecVerb verb)
{
    ArenaTemp scratch = TC_ScratchGet(0, 0);
    
    switch(verb)
    {
        case(OS_FileExecVerb_Default):
        case(OS_FileExecVerb_Open):
        case(OS_FileExecVerb_Edit):
        {
            S8 cmd = S8FromFmt(scratch.arena, "xdg-open %.*s & disown", FmtS8(filename));
            system(cmd.buffer);
        } break;
        
        case(OS_FileExecVerb_Print):
        {
            // TODO(tbt): i'm not really sure what the best option is here?
            S8 cmd = S8FromFmt(scratch.arena, "lp %.*s & disown", FmtS8(filename));
            system(cmd.buffer);
        } break;
    }
    
    ArenaTempEnd(scratch);
}

Function S8
OS_FilenameFromSaveDialogue (Arena *arena, S8 title, S8List extensions)
{
    Assert(0);
}

Function S8
OS_FilenameFromOpenDialogue (Arena *arena, S8 title, S8List extensions)
{
    Assert(0);
}

//~NOTE(tbt): directory change notifications

Function OpaqueHandle
OS_FileWatchStart(S8 filename)
{
    uint32_t mask = IN_CREATE | IN_DELETE | IN_DELETE_SELF | IN_MOVE_SELF | IN_MOVED_TO;
    
    int fd = inotify_init1(IN_NONBLOCK),
    inotify_add_watch(fd, filename.buffer, mask);
    
    OpaqueHandle result = { .p = EncodeU64FromI64(fd), };
    return result;
}

Function B32
OS_FileWatchWait(OpaqueHandle watch)
{
    B32 result = True;
    
    int fd = DecodeI64FromU64(watch.p);
    
    char buffer[sizeof(struct inotify_event) + NAME_MAX + 1];
    if(read(fd, buffer, sizeof(buffer)) < 0)
    {
        result = False;
    }
    
    return result;
}

Function void
OS_FileWatchStop(OpaqueHandle watch)
{
    int fd = DecodeI64FromU64(watch.p);
    close(fd);
}


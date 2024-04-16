
#import <Foundation/Foundation.h>
#import <Cocoa/Cocoa.h>
#import <UniformTypeIdentifiers/UniformTypeIdentifiers.h>

#include <dirent.h>
#include <dlfcn.h>
#include <errno.h>
#include <fcntl.h>
#include <pthread.h>
#include <semaphore.h>
#include <stdlib.h>
#include <sys/event.h>
#include <sys/mman.h>
#include <sys/random.h>
#include <sys/stat.h>
#include <sys/syscall.h>
#include <time.h>
#include <unistd.h>
#include <wordexp.h>

Function void
MAC_Init()
{
    [[NSApplication sharedApplication] setActivationPolicy:NSApplicationActivationPolicyRegular];
}

Function S8List
OS_CmdLineGet(Arena *arena)
{
    S8List result = {0};
    NSArray *arguments = [[NSProcessInfo processInfo] arguments];
    for(NSString *string in arguments)
    {
        S8Node *node = ArenaPush(arena, sizeof(*node));
        U64 len = [string lengthOfBytesUsingEncoding:NSUTF8StringEncoding];
        node->string.buffer = ArenaPush(arena, len);
        NSUInteger used_length = 0;
        [string getBytes:(void * _Nullable)node->string.buffer maxLength:len usedLength:&used_length encoding:NSUTF8StringEncoding options:0 range:NSMakeRange(0, [string length]) remainingRange:NULL];
        node->string.len = used_length;
        S8ListAppendExplicit(&result, node);
    }
    return result;
}

Function void
OS_ClipboardTextSet(S8 string)
{
    NSPasteboard *pasteboard = [NSPasteboard generalPasteboard];
    NSInteger change_count = [pasteboard clearContents];
    NSString *text = [[NSString alloc] initWithBytes: string.buffer length: string.len encoding:NSUTF8StringEncoding];
    NSArray *objects_to_copy = [[NSArray alloc] initWithObjects:text, nil];
    BOOL ok = [pasteboard writeObjects:objects_to_copy];
    [text release];
    [objects_to_copy release];
}

Function void
OS_ClipboardFilesSet(S8List filenames)
{
    NSPasteboard *pasteboard = [NSPasteboard generalPasteboard];
    NSInteger change_count = [pasteboard clearContents];
    NSMutableArray *objects_to_copy = [NSMutableArray arrayWithCapacity:filenames.count];
    for(S8ListForEach(filenames, s))
    {
        OS_FileProperties props = OS_FilePropertiesGet(s->string);
        B32 is_directory = (props.flags & OS_FilePropertiesFlags_IsDirectory);

        NSString *text = [[NSString alloc] initWithBytes: s->string.buffer length: s->string.len encoding:NSUTF8StringEncoding];
        NSURL *url = [NSURL fileURLWithPath:text isDirectory:(is_directory ? YES : NO)];
        [objects_to_copy addObject:url];
        [text release];
    }
    BOOL ok = [pasteboard writeObjects:objects_to_copy];
}

Function void
OS_ClipboardStringListSet(S8List strings)
{
    NSPasteboard *pasteboard = [NSPasteboard generalPasteboard];
    NSInteger change_count = [pasteboard clearContents];
    NSMutableArray *objects_to_copy = [NSMutableArray arrayWithCapacity:strings.count];
    for(S8ListForEach(strings, s))
    {
        NSString *text = [[NSString alloc] initWithBytes: s->string.buffer length: s->string.len encoding:NSUTF8StringEncoding];
        [objects_to_copy addObject:text];
        [text release];
    }
    BOOL ok = [pasteboard writeObjects:objects_to_copy];
}

Function S8
OS_ClipboardTextGet(Arena *arena)
{
    S8 result = {0};

    NSPasteboard *pasteboard = [NSPasteboard generalPasteboard]; 
    NSArray *classes = [[NSArray alloc] initWithObjects:[NSString class], nil];
    NSDictionary *options = [NSDictionary dictionary];
    NSArray *copied_items = [pasteboard readObjectsForClasses:classes options:options];
    if(copied_items != nil)
    {
        NSString *text = [copied_items objectAtIndex:0];
        U64 len = [text lengthOfBytesUsingEncoding:NSUTF8StringEncoding];
        result.buffer = ArenaPush(arena, len);
        NSUInteger used_length = 0;
        [text getBytes:(void * _Nullable)result.buffer maxLength:len usedLength:&used_length encoding:NSUTF8StringEncoding options:0 range:NSMakeRange(0, [text length]) remainingRange:NULL];
        result.len = used_length;
    }
    [classes release];

    return result;
}

Function S8List
OS_ClipboardFilesGet(Arena *arena)
{
    S8List result = {0};

    NSPasteboard *pasteboard = [NSPasteboard generalPasteboard]; 
    NSArray *classes = [[NSArray alloc] initWithObjects:[NSURL class], nil];
    NSDictionary *options = [NSDictionary dictionary];
    NSArray *copied_items = [pasteboard readObjectsForClasses:classes options:options];
    for(NSURL *url in copied_items)
    {
        S8Node *s = ArenaPush(arena, sizeof(*s));
        NSString *text = url.path;
        U64 len = [text lengthOfBytesUsingEncoding:NSUTF8StringEncoding];
        s->string.buffer = ArenaPush(arena, len);
        NSUInteger used_length = 0;
        [text getBytes:(void * _Nullable)s->string.buffer maxLength:len usedLength:&used_length encoding:NSUTF8StringEncoding options:0 range:NSMakeRange(0, [text length]) remainingRange:NULL];
        s->string.len = used_length;
        S8ListAppendExplicit(&result, s);
    }
    [classes release];

    return result;
}

Function S8List
OS_ClipboardStringListGet(Arena *arena)
{
    S8List result = {0};

    NSPasteboard *pasteboard = [NSPasteboard generalPasteboard]; 
    NSArray *classes = [[NSArray alloc] initWithObjects:[NSString class], nil];
    NSDictionary *options = [NSDictionary dictionary];
    NSArray *copied_items = [pasteboard readObjectsForClasses:classes options:options];
    for(NSString *text in copied_items)
    {
        S8Node *s = ArenaPush(arena, sizeof(*s));
        U64 len = [text lengthOfBytesUsingEncoding:NSUTF8StringEncoding];
        s->string.buffer = ArenaPush(arena, len);
        NSUInteger used_length = 0;
        [text getBytes:(void * _Nullable)s->string.buffer maxLength:len usedLength:&used_length encoding:NSUTF8StringEncoding options:0 range:NSMakeRange(0, [text length]) remainingRange:NULL];
        s->string.len = used_length;
        S8ListAppendExplicit(&result, s);
    }
    [classes release];

    return result;
}

Function S8
OS_FilenameFromSaveDialogue(Arena *arena, S8 title, S8List extensions)
{
    S8 result = {0};

    NSSavePanel *panel = [NSSavePanel savePanel];
    
    panel.title = [[NSString alloc] initWithBytes: title.buffer length: title.len encoding:NSUTF8StringEncoding];
    
    NSMutableArray *allowed_types = [NSMutableArray arrayWithCapacity:extensions.count];
    for(S8ListForEach(extensions, s))
    {
        NSString *text = [[NSString alloc] initWithBytes: s->string.buffer length: s->string.len encoding:NSUTF8StringEncoding];
        UTType *type = [UTType typeWithFilenameExtension:text];
        [allowed_types addObject:type];
        [text release];
    }
    panel.allowedContentTypes = allowed_types;

    [panel runModal];

    NSString *text = panel.URL.path;
    U64 len = [text lengthOfBytesUsingEncoding:NSUTF8StringEncoding];
    result.buffer = ArenaPush(arena, len);
    NSUInteger used_length = 0;
    [text getBytes:(void * _Nullable)result.buffer maxLength:len usedLength:&used_length encoding:NSUTF8StringEncoding options:0 range:NSMakeRange(0, [text length]) remainingRange:NULL];
    result.len = used_length;

    return result;
}

Function S8
OS_FilenameFromOpenDialogue(Arena *arena, S8 title, S8List extensions)
{
    S8 result = {0};

    NSOpenPanel *panel = [NSOpenPanel openPanel];

    panel.title = [[NSString alloc] initWithBytes: title.buffer length: title.len encoding:NSUTF8StringEncoding];
    
    NSMutableArray *allowed_types = [NSMutableArray arrayWithCapacity:extensions.count];
    for(S8ListForEach(extensions, s))
    {
        NSString *text = [[NSString alloc] initWithBytes: s->string.buffer length: s->string.len encoding:NSUTF8StringEncoding];
        UTType *type = [UTType typeWithFilenameExtension:text];
        [allowed_types addObject:type];
        [text release];
    }
    panel.allowedContentTypes = allowed_types;

    [panel runModal];

    NSString *text = panel.URL.path;
    U64 len = [text lengthOfBytesUsingEncoding:NSUTF8StringEncoding];
    result.buffer = ArenaPush(arena, len);
    NSUInteger used_length = 0;
    [text getBytes:(void * _Nullable)result.buffer maxLength:len usedLength:&used_length encoding:NSUTF8StringEncoding options:0 range:NSMakeRange(0, [text length]) remainingRange:NULL];
    result.len = used_length;

    return result;
}

Function OpaqueHandle
OS_SemaphoreMake(I32 initial)
{
    dispatch_semaphore_t sem = dispatch_semaphore_create(initial);
    OpaqueHandle result = { .p = IntFromPtr(sem), };
    return result;
}

Function void
OS_SemaphoreSignal(OpaqueHandle sem)
{
	dispatch_semaphore_t s = PtrFromInt(sem.p);
    dispatch_semaphore_signal(s);
}

Function void
OS_SemaphoreWait(OpaqueHandle sem)
{
    dispatch_semaphore_t s = PtrFromInt(sem.p);
	dispatch_semaphore_wait(s, DISPATCH_TIME_FOREVER);
}

Function void
OS_SemaphoreDestroy(OpaqueHandle sem)
{
	dispatch_object_t s = PtrFromInt(sem.p);
    dispatch_release(s);
}

Function void
OS_FileExec(S8 filename, OS_FileExecVerb verb)
{
    switch(verb)
    {
        default:
        {
            // TODO(tbt): implement printing
            Assert(0);
        } break;

        case(OS_FileExecVerb_Default):
        case(OS_FileExecVerb_Open):
        case(OS_FileExecVerb_Edit):
        {
            OS_FileProperties props = OS_FilePropertiesGet(filename);
            B32 is_directory = (props.flags & OS_FilePropertiesFlags_IsDirectory);

            NSString *text = [[NSString alloc] initWithBytes: filename.buffer length: filename.len encoding:NSUTF8StringEncoding];
            NSURL *url = [NSURL fileURLWithPath:text isDirectory:(is_directory ? YES : NO)];

            NSWorkspace *workspace = [NSWorkspace sharedWorkspace];
            [workspace openURL:url];

            [text release];
        } break;
    }
}


Function OpaqueHandle
OS_FileWatchMake(S8 filename)
{
    ArenaTemp scratch = OS_TCtxScratchGet(0, 0);

    int kq = kqueue();

    S8 s = S8Clone(scratch.arena, filename);
    int dir_fd = open(filename.buffer, O_RDONLY);

    struct kevent dir_event;
    EV_SET(&dir_event,
           dir_fd,
           EVFILT_VNODE, EV_ADD | EV_CLEAR | EV_ENABLE,
           NOTE_WRITE,
           0, 0);
    kevent(kq, &dir_event, 1, 0, 0, 0);

    OpaqueHandle result = { .p = EncodeU64FromI64(kq), };
    return result;
}

Function B32
OS_FileWatchWait(OpaqueHandle handle, U64 timeout_milliseconds)
{
    B32 result = False;

    int kq = DecodeI64FromU64(handle.p);

    time_t seconds = timeout_milliseconds / 1000;
    int64_t nanoseconds = (timeout_milliseconds - seconds*1000)*1000000;
    struct timespec ts = { .tv_sec = seconds, .tv_nsec = nanoseconds, };

    struct timespec *timeout = 0;
    if(OS_FileWatchTimeoutInfinite != timeout_milliseconds)
    {
        timeout = &ts;
    }

    struct kevent change;
    if(kevent(kq, NULL, 0, &change, 1, timeout) >= 0)
    {
        result = True;
    }

   return result;
}

Function void
OS_FileWatchDestroy(OpaqueHandle handle)
{
    int kq = DecodeI64FromU64(handle.p);
    close(kq);
}


//~NOTE(tbt): file properties

typedef U64 OS_FilePropertiesFlags;
enum OS_FilePropertiesFlags
{
    OS_FilePropertiesFlags_Exists      = Bit(0),
    OS_FilePropertiesFlags_IsDirectory = Bit(1),
    OS_FilePropertiesFlags_Hidden      = Bit(2),
};

typedef struct OS_FileProperties OS_FileProperties;
struct OS_FileProperties
{
    OS_FilePropertiesFlags flags;
    U64 size;
    DataAccessFlags access_flags;
    DenseTime creation_time;
    DenseTime access_time;
    DenseTime write_time;
};

Function OS_FileProperties OS_FilePropertiesFromFilename (S8 filename);

//~NOTE(tbt): basic file IO operations

Function S8  OS_FileReadEntire     (Arena *arena, S8 filename);
Function S8  OS_FileReadTextEntire (Arena *arena, S8 filename);
Function B32 OS_FileWriteEntire    (S8 filename, S8 data);

//~NOTE(tbt): file management

Function B32 OS_FileDelete       (S8 filename);                  // NOTE(tbt): deletes a single file
Function B32 OS_FileMove         (S8 filename, S8 new_filename); // NOTE(tbt): renames or moves a file or directory
Function B32 OS_DirectoryMake    (S8 filename);                  // NOTE(tbt): creates an empty directory
Function B32 OS_DirectoryDelete  (S8 filename);                  // NOTE(tbt): deletes an empty directory

//~NOTE(tbt): directory iteration

typedef struct OS_FileIterator OS_FileIterator;
struct OS_FileIterator
{
    S8 directory;
    S8 current_name;
    S8 current_full_path;
    OS_FileProperties current_properties;
};

Function OS_FileIterator *OS_FileIteratorBegin (Arena *arena, S8 filename);
Function B32              OS_FileIteratorNext  (Arena *arena, OS_FileIterator *iter);
Function void             OS_FileIteratorEnd   (OS_FileIterator *iter);

//~NOTE(tbt): standard paths

typedef enum OS_StdPath OS_StdPath;
enum OS_StdPath
{
    OS_StdPath_CWD,
    OS_StdPath_ExecutableFile,
    OS_StdPath_ExecutableDir,
    OS_StdPath_Home,
    OS_StdPath_Config,
    OS_StdPath_Temp,
    
    OS_StdPath_MAX,
};

Function S8 OS_S8FromStdPath (Arena *arena, OS_StdPath path);

//~NOTE(tbt): opening/executing files

typedef enum OS_FileExecVerb OS_FileExecVerb;
enum OS_FileExecVerb
{
    OS_FileExecVerb_Default,
    OS_FileExecVerb_Open,
    OS_FileExecVerb_Edit,
    OS_FileExecVerb_Print,
    OS_FileExecVerb_MAX,
};
Function void OS_FileExec (S8 filename, OS_FileExecVerb verb);

//~NOTE(tbt): path utils

Function void OS_CwdSet (S8 cwd);
Function S8   OS_AbsolutePathFromRelativePath (Arena *arena, S8 path);

//~NOTE(tbt): directory change notifications

enum { OS_FileWatchTimeoutInfinite = ~((U64)0), };

Function OpaqueHandle OS_FileWatchStart (S8 filename);
Function B32          OS_FileWatchWait  (OpaqueHandle watch, U64 timeout_milliseconds);
Function void         OS_FileWatchStop  (OpaqueHandle watch);

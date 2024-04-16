
//~NOTE(tbt): initialisation and cleanup

Function void OS_Init    (void);
Function void OS_Cleanup (void);

//~NOTE(tbt): clipboard

Function void OS_ClipboardTextSet       (S8 string);
Function void OS_ClipboardFilesSet      (S8List filenames);
Function void OS_ClipboardStringListSet (S8List strings);
Function S8 OS_ClipboardTextGet       (Arena *arena);
Function S8List OS_ClipboardFilesGet      (Arena *arena);
Function S8List OS_ClipboardStringListGet (Arena *arena);

//~NOTE(tbt): console

// TODO(tbt): console input

// TODO(tbt): helpers for escape sequences`

Function void OS_ConsoleOutputS8 (S8 string);
Function void OS_ConsoleOutputS16 (S16 string);
Function void OS_ConsoleOutputFmtV (char *fmt, va_list args);
Function void OS_ConsoleOutputFmt (char *fmt, ...);
Function void OS_ConsoleOutputLine (S8 string);

Function S8List OS_CmdLineGet (Arena *arena);

//~NOTE(tbt): entropy

Function void OS_WriteEntropy (void *buffer, U64 size);
Function U64 OS_U64FromEntropy (void);

//~NOTE(tbt): memory

Function void *OS_MemoryReserve (U64 size);
Function void  OS_MemoryRelease (void *memory, U64 size);
Function void  OS_MemoryCommit (void *memory, U64 size);
Function void  OS_MemoryDecommit (void *memory, U64 size);

#if !defined(MemoryReserve)
# define MemoryReserve(S) OS_MemoryReserve(S)
#endif
#if !defined(MemoryRelease)
# define MemoryRelease(P, S) OS_MemoryRelease(P, S)
#endif
#if !defined(MemoryCommit)
# define MemoryCommit(P, S) OS_MemoryCommit(P, S)
#endif
#if !defined(MemoryDecommit)
# define MemoryDecommit(P, S) OS_MemoryDecommit(P, S)
#endif

//~NOTE(tbt): shared libraries

#if BuildCompilerMSVC
# define SharedLibExport __declspec(dllexport)
#else
// TODO(tbt): look into whether this is necessary on other platforms
# define SharedLibExport 
#endif

Function OpaqueHandle OS_SharedLibOpen (S8 filename);
Function void *OS_SharedLibSymbolLookup (OpaqueHandle lib, char *symbol);
Function void OS_SharedLibClose (OpaqueHandle lib);

//~NOTE(tbt): time

// NOTE(tbt): real world time
Function DateTime OS_NowUTC (void);	// NOTE(tbt): current date-time in universal time coordinates
Function DateTime OS_NowLTC (void);	// NOTE(tbt): current date-time in local time coordinates
Function DateTime OS_LTCFromUTC (DateTime universal_time);	// NOTE(tbt): convert from universal time coordinates to local time coordinates
Function DateTime OS_UTCFromLTC (DateTime local_time);	// NOTE(tbt): convert from local time coordinates to universal time coordinates

// NOTE(tbt): precision time
Function void OS_Sleep (U64 milliseconds);
Function U64 OS_TimeInMicroseconds (void); // NOTE(tbt): query the performance counter and convert microseconds
Function F64 OS_TimeInSeconds (void); // NOTE(tbt): query the performance counter and convert to seconds

// NOTE(tbt): measures the time taken to complete a block and adds it to R
#define OS_InstrumentBlockCumulative(R) U64 R ## _begin__; DeferLoop(Glue(R, _begin__) = OS_TimeInMicroseconds(), R += OS_TimeInMicroseconds() - Glue(R, _begin__))

//~NOTE(tbt): system dialogues

Function S8 OS_FilenameFromSaveDialogue (Arena *arena, S8 title, S8List extensions);
Function S8 OS_FilenameFromOpenDialogue (Arena *arena, S8 title, S8List extensions);

//~NOTE(tbt): system queries

Function U64 OS_ProcessorsCount (void);
Function U64 OS_PageSize (void);

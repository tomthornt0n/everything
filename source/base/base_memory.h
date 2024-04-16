
//~NOTE(tbt): utilities

Function void *MemorySet (void *dest, U8 value, U64 bytes);
Function void *MemoryCopy (void *restrict dest, const void *restrict src, U64 bytes);
Function void *MemoryMove (void *dest, const void *src, U64 bytes);
Function B32 MemoryMatch (const void *a, const void *b, U64 bytes);

Function U64 AlignForward (U64 ptr, U64 align);

//~NOTE(tbt): arenas

Global U64 arena_commit_chunk_size = Kilobytes(16);

typedef struct Arena Arena;
struct Arena
{
    U64 cap;
    U64 alloc_position;
    U64 commit_position;
};

Function Arena *ArenaAlloc (U64 reserve_size);
Function void ArenaRelease (Arena *arena);

Function void *ArenaPush (Arena *arena, U64 size, U64 align);
Function void *ArenaPushNoZero (Arena *arena, U64 size, U64 align);
Function void ArenaPop (Arena *arena, U64 size);
Function void ArenaPopTo (Arena *arena, U64 alloc_position);
Function B32 ArenaHas (Arena *arena, const void *ptr);
Function void ArenaClear (Arena *arena);

#define PushArray(A, T, C) ArenaPush(A, sizeof(T)*(C), _Alignof(T))

//~NOTE(tbt): temporary memory

typedef struct ArenaTemp ArenaTemp;
struct ArenaTemp
{
    Arena *arena;
    U64 alloc_position;
};

Function ArenaTemp ArenaTempBegin (Arena *arena);
Function void ArenaTempEnd (ArenaTemp temp);

//~NOTE(tbt): scratch pool

typedef struct ScratchPool ScratchPool;
struct ScratchPool
{
    Arena *arenas[4];
};

Function void ScratchPoolAlloc (ScratchPool *pool);
Function ArenaTemp ScratchBegin (ScratchPool *pool, Arena *non_conflict[], U64 non_conflict_count);
Function void ScratchPoolRelease (ScratchPool *pool);

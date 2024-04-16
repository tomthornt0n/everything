
#if !defined(MemoryReserve)
# error "missing implementation for macro 'MemoryReserve'"
#endif
#if !defined(MemoryRelease)
# error "missing implementation for macro 'MemoryRelease'"
#endif
#if !defined(MemoryCommit)
# error "missing implementation for macro 'MemoryCommit'"
#endif
#if !defined(MemoryDecommit)
# error "missing implementation for macro 'MemoryDecommit'"
#endif

//~NOTE(tbt): utilities

Function void *
MemorySet(void *memory, U8 value, U64 bytes)
{
    U8 *data = memory;
    for(U64 i = 0;
        i < bytes;
        i += 1)
    {
        data[i] = value;
    }
    return memory;
}

Function void *
MemoryCopy(void *restrict dest, const void *restrict src, U64 bytes)
{
    const U8 *from = src;
    U8 *to = dest;
    for(U64 i = 0;
        i < bytes;
        i += 1)
    {
        to[i] = from[i];
    }
    return dest;
}

Function void *
MemoryMove(void *dest, const void *src, U64 bytes)
{
    U8 *d = dest;
    const U8 *s = src;
    
    void *result = 0;
    
    if (d == s)
    {
        result = dest;
    }
    else if((U64)s - (U64)d - bytes <= -2*bytes)
    {
        result = MemoryCopy(d, s, bytes);
    }
    else if(d < s)
    {
        if((U64)s % sizeof(U64) == (U64)d % sizeof(U64))
        {
            while((U64)d % sizeof(U64))
            {
                if (0 == bytes)
                {
                    return dest;
                }
                bytes -= 1;
                *d = *s;
                d += 1;
                s += 1;
            }
            while(bytes >= sizeof(U64))
            {
                *(U64 *)d = *(U64 *)s;
                bytes -= sizeof(U64);
                d += sizeof(U64);
                s += sizeof(U64);
            }
        }
        while(0 != bytes)
        {
            *d = *s;
            d += 1;
            s += 1;
            bytes -= 1;
        }
    }
    else
    {
        if((U64)s % sizeof(U64) == (U64)d % sizeof(U64))
        {
            while((U64)(d + bytes) % sizeof(U64))
            {
                if(0 == bytes)
                {
                    return dest;
                }
                bytes -= 1;
                d[bytes] = s[bytes];
            }
            while(bytes >= sizeof(U64))
            {
                bytes -= sizeof(U64);
                *(U64 *)(d + bytes) = *(U64 *)(s + bytes);
            }
        }
        while(0 != bytes)
        {
            bytes -= 1;
            d[bytes] = s[bytes];
        }
    }
    
    return dest;
}

Function B32
MemoryMatch(const void *a, const void *b, U64 bytes)
{
    const U8 *one = a;
    const U8 *two = b;
    for(U64 i = 0;
        i < bytes;
        i += 1)
    {
        if(one[i] != two[i])
        {
            return False;
        }
    }
    return True;
}

Function U64
AlignForward(U64 ptr, U64 align)
{
    U64 modulo = ptr % align;
    U64 result = (0 == modulo) ? ptr : ptr + align - modulo;
    return result;
}

//~NOTE(tbt): arenas

Function Arena *
ArenaAlloc(U64 reserve_size)
{
    void *memory = MemoryReserve(reserve_size);
    MemoryCommit(memory, arena_commit_chunk_size);
    
    Arena *result = memory;
    result->cap = reserve_size;
    result->alloc_position = sizeof(Arena);
    result->commit_position = arena_commit_chunk_size;
    
    return result;
}

Function void *
ArenaPushNoZero(Arena *arena, U64 size, U64 align)
{
    void *memory = 0;
    
    U64 base = AlignForward(arena->alloc_position, align);
    U64 new_alloc_position = base + size;
    if(new_alloc_position < arena->cap)
    {
        if(new_alloc_position > arena->commit_position)
        {
            U64 commit_size = size;
            commit_size += arena_commit_chunk_size - 1;
            commit_size -= commit_size % arena_commit_chunk_size;
            MemoryCommit((U8 *)arena + arena->commit_position, commit_size);
            arena->commit_position += commit_size;
        }
        
        memory = PtrFromInt(IntFromPtr(arena) + base);
        arena->alloc_position = new_alloc_position;
    }
    
    Assert(0 != memory);
    
    return memory;
}

Function void *
ArenaPush(Arena *arena, U64 size, U64 align)
{
    void *memory = ArenaPushNoZero(arena, size, align);
    MemorySet(memory, 0, size);
    return memory;
}

Function void
ArenaPop(Arena *arena, U64 size)
{
    ArenaPopTo(arena, arena->alloc_position - size);
}

Function void
ArenaPopTo(Arena *arena, U64 alloc_position)
{
    U64 excess_committed = (arena->commit_position - alloc_position);
    if(excess_committed >= arena_commit_chunk_size)
    {
        U64 chunks_to_decommit = excess_committed / arena_commit_chunk_size;
        U64 size_to_decommit = chunks_to_decommit*arena_commit_chunk_size;
        arena->commit_position -= size_to_decommit;
        MemoryDecommit(PtrFromInt(IntFromPtr(arena) + arena->commit_position), size_to_decommit);
    }
    arena->alloc_position = alloc_position;
}

Function B32
ArenaHas(Arena *arena, const void *ptr)
{
    B32 result = (IntFromPtr(ptr) > IntFromPtr(arena) && IntFromPtr(ptr) - IntFromPtr(arena) < arena->commit_position);
    return result;
}

Function void
ArenaClear(Arena *arena)
{
    ArenaPopTo(arena, sizeof(Arena));
}

Function void
ArenaRelease(Arena *arena)
{
    U64 reserve_size = arena->cap;
    U64 commit_position = arena->commit_position;
    MemoryDecommit(arena, commit_position);
    MemoryRelease(arena, reserve_size);
}

//~NOTE(tbt): temporary memory

Function ArenaTemp
ArenaTempBegin(Arena *arena)
{
    ArenaTemp result =
    {
        .arena = arena,
        .alloc_position = arena->alloc_position,
    };
    return result;
}

Function void
ArenaTempEnd(ArenaTemp temp)
{
    ArenaPopTo(temp.arena, temp.alloc_position);
}

//~NOTE(tbt): scratch pool

Function void
ScratchPoolAlloc(ScratchPool *pool)
{
    for(U64 arena_index = 0;
        arena_index < ArrayCount(pool->arenas);
        arena_index += 1)
    {
        pool->arenas[arena_index] = ArenaAlloc(Gigabytes(2));
    }
}

Function ArenaTemp
ScratchBegin(ScratchPool *pool,
             Arena *non_conflict_array[],
             U64 non_conflict_count)
{
    ArenaTemp result = {0};
    
    Arena *scratch = 0;
    for(U64 arena_index = 0;
        arena_index < ArrayCount(pool->arenas);
        arena_index += 1)
    {
        scratch = pool->arenas[arena_index];
        B32 is_conflicting = False;
        for(U64 conflict_index = 0;
            conflict_index < non_conflict_count;
            conflict_index += 1)
        {
            Arena *non_conflict = non_conflict_array[conflict_index];
            if(non_conflict == scratch)
            {
                is_conflicting = True;
                break;
            }
        }
        if(!is_conflicting)
        {
            result = ArenaTempBegin(scratch);
            break;
        }
    }
    
    Assert(0 != result.arena);
    return result;
}

Function void
ScratchPoolRelease(ScratchPool *pool)
{
    for(U64 arena_index = 0;
        arena_index < ArrayCount(pool->arenas);
        arena_index += 1)
    {
        Arena *arena = pool->arenas[arena_index];
        ArenaRelease(arena);
    }
}

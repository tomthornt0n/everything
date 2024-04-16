
Function void
PTBD_SelectionSet(Arena *arena, PTBD_Selection *selection, I64 id, B32 is_selected)
{
    if(id > 0)
    {
        B32 is_in_selection = PTBD_SelectionHas(selection, id);
        
        if(!is_in_selection && is_selected)
        {
            for(PTBD_Selection *chunk = selection; 0 != chunk; chunk = chunk->next)
            {
                for(U64 i = 0; i < chunk->cap; i += 1)
                {
                    if(chunk->ids[i] <= 0)
                    {
                        chunk->ids[i] = id;
                        goto break_all_1;
                    }
                }
                
                if(0 == chunk->next)
                {
                    U64 cap = 32;
                    chunk->next = ArenaPush(arena, sizeof(*chunk->next) + cap*sizeof(chunk->ids[0]), _Alignof(PTBD_Selection));
                    chunk->next->cap = cap;
                }
            }
            break_all_1:;
        }
        else if(is_in_selection && !is_selected)
        {
            for(PTBD_Selection *chunk = selection; 0 != chunk; chunk = chunk->next)
            {
                for(U64 i = 0; i < chunk->cap; i += 1)
                {
                    if(chunk->ids[i] == id)
                    {
                        chunk->ids[i] = 0;
                        goto break_all_2;
                    }
                }
            }
            break_all_2:;
        }
    }
}

Function B32
PTBD_SelectionHas(PTBD_Selection *selection, I64 id)
{
    B32 result = False;
    
    for(PTBD_Selection *chunk = selection; 0 != chunk && id > 0; chunk = chunk->next)
    {
        for(U64 i = 0; i < chunk->cap; i += 1)
        {
            if(chunk->ids[i] == id)
            {
                result = True;
                goto break_all;
            }
        }
    }
    break_all:;
    
    return result;
}

Function void
PTBD_SelectionClear(PTBD_Selection *selection)
{
    for(PTBD_Selection *chunk = selection; 0 != chunk; chunk = chunk->next)
    {
        MemorySet(chunk->ids, 0, sizeof(chunk->ids[0])*chunk->cap);
    }
}


Function VD_Bitmap
VD_PushBitmap(Arena *arena, I32 width, I32 height)
{
    VD_Bitmap result =
    {
        .width = width,
        .height = height,
        .pixels = PushArray(arena, U8, width*height),
    };
    return result;
};

Function U8
VD_BitmapPixelAt(VD_Bitmap *bitmap, I32 x, I32 y)
{
    U8 result = 255;
    if(0 <= x && x < bitmap->width &&
       0 <= y && y < bitmap->height)
    {
        result = bitmap->pixels[x + y*bitmap->width];
    }
    return result;
}

Function void
VD_BitmapSetPixel(VD_Bitmap *bitmap, I32 x, I32 y, U8 value)
{
    if(0 <= x && x < bitmap->width &&
       0 <= y && y < bitmap->height)
    {
        bitmap->pixels[x + y*bitmap->width] = value;
    }
}

Function void
VD_StackPush(Arena *arena, VD_Stack *stack, VD_StackFrame frame)
{
    if(0 == stack->top || stack->top->count >= stack->top->cap)
    {
        VD_StackChunk *new_chunk = stack->free_list;
        if(0 == new_chunk)
        {
            new_chunk = PushArray(arena, VD_StackChunk, 1);
        }
        else
        {
            stack->free_list = stack->free_list->next;
        }
        //new_chunk->cap = (0 == stack->top) ? 1024 : Min(stack->top->cap*2, 32768);
        new_chunk->cap = 1;
        new_chunk->frames = PushArray(arena, VD_StackFrame, new_chunk->cap);
        new_chunk->next = stack->top;
        stack->top = new_chunk;
    }
    
    VD_StackChunk *chunk = stack->top;
    
    chunk->frames[chunk->count] = frame;
    chunk->count += 1;
}

Function B32
VD_StackPop(VD_Stack *stack, VD_StackFrame *result)
{
    B32 success = False;
    
    while(0 != stack->top && 0 == stack->top->count)
    {
        VD_StackChunk *to_free = stack->top;
        stack->top = stack->top->next;
        to_free->next = stack->free_list;
        stack->free_list = to_free;
    }
    
    VD_StackChunk *chunk = stack->top;
    
    if(0 != chunk && chunk->count > 0)
    {
        chunk->count -= 1;
        MemoryCopy(result, &chunk->frames[chunk->count], sizeof(VD_StackFrame));
        success = True;
    }
    
    return success;
}

Function VD_Node *
VD_TreeFromBitmapInternal(Arena *arena, VD_Bitmap *bitmap, VD_Bitmap *already_visited, V2I root)
{
    ArenaTemp scratch = OS_TCtxScratchBegin(&arena, 1);
    
    VD_Node *result = PushArray(arena, VD_Node, 1);
    
    U8 original = VD_BitmapPixelAt(bitmap, root.x, root.y);
    
    result->value = original;
    result->bounds = Range2F(U2F(Infinity), U2F(NegativeInfinity));
    
    VD_Stack stack = {0};
    VD_StackPush(scratch.arena, &stack, VD_StackFrame(root));
    
    VD_Stack queued_objects = {0};
    
    VD_StackFrame frame = {0};
    while(VD_StackPop(&stack, &frame))
    {
        if(!VD_BitmapPixelAt(already_visited, frame.pos.x, frame.pos.y))
        {
            VD_BitmapSetPixel(already_visited, frame.pos.x, frame.pos.y, 255);
            result->bounds.min = Mins2F(result->bounds.min, V2F(frame.pos.x, frame.pos.y));
            result->bounds.max = Maxs2F(result->bounds.max, V2F(frame.pos.x, frame.pos.y));
            
#define X(X, Y) VD_StackPush(scratch.arena, (VD_BitmapPixelAt(bitmap, X, Y) == original) ? &stack : &queued_objects, VD_StackFrame(X, Y))
            X(frame.pos.x, frame.pos.y - 1);
            X(frame.pos.x, frame.pos.y + 1);
            X(frame.pos.x - 1, frame.pos.y);
            X(frame.pos.x + 1, frame.pos.y);
#undef X
        }
    }
    
    VD_StackFrame queued_object;
    while(VD_StackPop(&queued_objects, &queued_object))
    {
        if(!VD_BitmapPixelAt(already_visited, queued_object.pos.x, queued_object.pos.y))
        {
            VD_Node *child = VD_TreeFromBitmapInternal(arena, bitmap, already_visited, queued_object.pos);
            child->prev = result->last;
            if(0 == result->last)
            {
                result->first = child;
            }
            else
            {
                result->last->next = child;
            }
            result->last = child;
            result->children_count += 1;
        }
    }
    
    ArenaTempEnd(scratch);
    
    return result;
}

Function VD_Node *
VD_TreeFromBitmap(Arena *arena, VD_Bitmap *bitmap, V2I root)
{
    ArenaTemp scratch = OS_TCtxScratchBegin(&arena, 1);
    VD_Bitmap already_visited = VD_PushBitmap(scratch.arena, bitmap->width, bitmap->height);
    VD_Node *result = VD_TreeFromBitmapInternal(arena, bitmap, &already_visited, root);
    ArenaTempEnd(scratch);
    return result;
}

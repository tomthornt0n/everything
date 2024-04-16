
//~NOTE(tbt): rendering

Function FONT_PreparedText *
UI_PreparedTextFromS8(OpaqueHandle font_provider, I32 font_size, S8 string)
{
    if(0 == string.len)
    {
        string = S8(" ");
    }
    
    UI_PreparedTextCacheNode *result = 0;
    
    UI_PreparedTextCache *cache = &ui->pt_cache;
    
    U64 key[2];
    MurmurHash3_x64_128(string.buffer, string.len, 0, key);
    U64 bucket_index = key[0] % ArrayCount(cache->buckets);
    
    for(UI_PreparedTextCacheNode *node = cache->buckets[bucket_index]; 0 != node; node = node->next)
    {
        if(MemoryMatch(node->string_hash, key, sizeof(key)) &&
           OpaqueHandleMatch(node->pt.provider, font_provider) &&
           node->pt.size == font_size)
        {
            result = node;
            break;
        }
    }
    
    if(0 == result)
    {
        Arena *arena = G_WindowArenaGet(ui->window);
        result = cache->free_list;
        if(0 == result)
        {
            result = PushArray(arena, UI_PreparedTextCacheNode, 1);
        }
        else
        {
            cache->free_list = cache->free_list->next;
        }
        MemoryCopy(result->string_hash, key, sizeof(key));
        result->pt = FONT_PrepareS8(arena, &ui->pt_free_list, font_provider, font_size, string);
        result->next = cache->buckets[bucket_index];
        cache->buckets[bucket_index] = result;
    }
    
    result->last_touched_frame_index = ui->frame_index;
    
    return &result->pt;
}

//~NOTE(tbt): utilities

//-NOTE(tbt): rect cut layouts

Function Range2F
UI_CutLeft(Range2F *rect, F32 f)
{
    Range2F result =
    {
        .min = V2F(rect->min.x, rect->min.y),
        .max = V2F(rect->min.x + f, rect->max.y),
    };
    rect->min.x += f;
    return result;
}

Function Range2F
UI_CutRight(Range2F *rect, F32 f)
{
    Range2F result =
    {
        .min = V2F(rect->max.x - f, rect->min.y),
        .max = V2F(rect->max.x, rect->max.y),
    };
    rect->max.x -= f;
    return result;
}

Function Range2F
UI_CutTop(Range2F *rect, F32 f)
{
    Range2F result =
    {
        .min = V2F(rect->min.x, rect->min.y),
        .max = V2F(rect->max.x, rect->min.y + f),
    };
    rect->min.y += f;
    return result;
}

Function Range2F
UI_CutBottom (Range2F *rect, F32 f)
{
    Range2F result =
    {
        .min = V2F(rect->min.x, rect->max.y - f),
        .max = V2F(rect->max.x, rect->max.y),
    };
    rect->max.y -= f;
    return result;
}

Function Range2F
UI_GetLeft(Range2F rect, F32 f)
{
    Range2F result =
    {
        .min = V2F(rect.min.x, rect.min.y),
        .max = V2F(rect.min.x + f, rect.max.y),
    };
    return result;
}

Function Range2F
UI_GetRight(Range2F rect, F32 f)
{
    Range2F result =
    {
        .min = V2F(rect.max.x - f, rect.min.y),
        .max = V2F(rect.max.x, rect.max.y),
    };
    return result;
}

Function Range2F
UI_GetTop(Range2F rect, F32 f)
{
    Range2F result =
    {
        .min = V2F(rect.min.x, rect.min.y),
        .max = V2F(rect.max.x, rect.min.y + f),
    };
    return result;
}

Function Range2F
UI_GetBottom (Range2F rect, F32 f)
{
    Range2F result =
    {
        .min = V2F(rect.min.x, rect.max.y - f),
        .max = V2F(rect.max.x, rect.max.y),
    };
    return result;
}

//-NOTE(tbt): text editing

Function S8
UI_EditTextFilterHookDefault(Arena *arena, S8 entered, void *user_data)
{
    return entered;
}

Function S8
UI_EditTextFilterHookLineEdit(Arena *arena, S8 entered, void *user_data)
{
    S8 result = S8Replace(arena, entered, S8("\n"), S8(""), 0);
    return result;
}

Function S8
UI_EditTextFilterHookNumeric(Arena *arena, S8 entered, void *user_data)
{
    S8Builder builder = { .buffer = PushArray(arena, char, entered.len), .cap = entered.len, .len = 0, };
    UTFConsume consume = {0};
    for(U64 character_index = 0; character_index < entered.len; character_index += consume.advance)
    {
        consume = CodepointFromUTF8(entered, character_index);
        if(('0' <= consume.codepoint && consume.codepoint <= '9') || consume.codepoint == '.')
        {
            S8 s = { .buffer = &entered.buffer[character_index], .len = consume.advance, };
            S8BuilderAppend(&builder, s);
        }
    }
    S8 result = S8FromBuilder(builder);
    return result;
}

Function S8
UI_EditTextFilterHookDate(Arena *arena, S8 entered, void *user_data)
{
    S8Builder builder = { .buffer = PushArray(arena, char, entered.len), .cap = entered.len, .len = 0, };
    UTFConsume consume = {0};
    for(U64 character_index = 0; character_index < entered.len; character_index += consume.advance)
    {
        consume = CodepointFromUTF8(entered, character_index);
        if(('0' <= consume.codepoint && consume.codepoint <= '9') || consume.codepoint == '/')
        {
            S8 s = { .buffer = &entered.buffer[character_index], .len = consume.advance, };
            S8BuilderAppend(&builder, s);
        }
    }
    S8 result = S8FromBuilder(builder);
    return result;
}

Function B32
UI_EditTextActionFromEvent(G_Event *e, UI_EditTextAction *result)
{
    B32 is_action = False;
    
    MemorySet(result, 0, sizeof(*result));
    
    if(G_EventKind_Key == e->kind && e->is_down)
    {
        for(U64 i = 0; i < ArrayCount(ui_edit_text_key_bindings); i += 1)
        {
            if(ui_edit_text_key_bindings[i].key == e->key &&
               ui_edit_text_key_bindings[i].modifiers == e->modifiers)
            {
                result->flags = ui_edit_text_key_bindings[i].flags;
                result->offset = ui_edit_text_key_bindings[i].offset;
                is_action = True;
                break;
            }
        }
    }
    else if(G_EventKind_Char == e->kind)
    {
        //result->offset = +1;
        result->codepoint = e->codepoint;
        is_action = True;
    }
    
    return is_action;
}

Function UI_EditTextOp
UI_EditTextOpFromAction(Arena *arena,
                        UI_EditTextAction action,
                        S8 string,
                        Range1U64 selection,
                        UI_EditTextFilterHook *filter_hook,
                        void *filter_hook_user_data)
{
    UI_EditTextOp result = {0};
    
    result.next_selection = selection;
    
    if(action.flags & UI_EditTextActionFlags_SelectAll)
    {
        result.next_selection.mark = 0;
        result.next_selection.cursor = string.len;
    }
    
    if(action.flags & UI_EditTextActionFlags_Copy)
    {
        Range1U64 range =
        {
            .min = Min1U64(selection.cursor, selection.mark),
            .max = Max1U64(selection.cursor, selection.mark),
        };
        S8 selected_string =
        {
            .buffer = &string.buffer[range.min],
            .len = range.max - range.min
        };
        OS_ClipboardTextSet(selected_string);
    }
    
    if(action.flags & UI_EditTextActionFlags_Paste)
    {
        S8 raw_input = OS_ClipboardTextGet(arena);
        S8 filtered_input = filter_hook(arena, raw_input, filter_hook_user_data);
        if(0 != filtered_input.len || 0 == raw_input.len)
        {
            result.replace.min = Min1U64(result.next_selection.cursor, result.next_selection.mark);
            result.replace.max = Max1U64(result.next_selection.cursor, result.next_selection.mark);
            result.with = filtered_input;
        }
    }
    
    if(0 != action.codepoint)
    {
        S8 raw_input = UTF8FromCodepoint(arena, action.codepoint);
        S8 filtered_input = filter_hook(arena, raw_input, filter_hook_user_data);
        if(0 != filtered_input.len || 0 == raw_input.len)
        {
            result.replace.min = Min1U64(result.next_selection.cursor, result.next_selection.mark);
            result.replace.max = Max1U64(result.next_selection.cursor, result.next_selection.mark);
            result.with = filtered_input;
        }
    }
    
    I32 dir = Normalise1I(action.offset);
    if(0 != action.offset &&
       !((action.flags & UI_EditTextActionFlags_LineLevel) && S8IsLineBoundary(string, result.next_selection.cursor + dir)))
    {
        if(result.next_selection.cursor == result.next_selection.mark ||
           (action.flags & UI_EditTextActionFlags_StickMark))
        {
            U64 new_len = string.len - result.replace.max + result.replace.min + result.with.len;
            
            for(I32 i = action.offset; 0 != i; i -= dir)
            {
                do
                {
                    if((dir < 0 && result.next_selection.cursor == 0) ||
                       (dir > 0 && result.next_selection.cursor >= new_len))
                    {
                        goto break_all;
                    }
                    result.next_selection.cursor += dir;
                } while(UTF8IsContinuationByte(string, result.next_selection.cursor) ||
                        ((action.flags & UI_EditTextActionFlags_WordLevel) && !S8IsWordBoundary(string, result.next_selection.cursor)) ||
                        ((action.flags & UI_EditTextActionFlags_LineLevel) && !S8IsLineBoundary(string, result.next_selection.cursor)));
            }
            break_all:;
        }
    }
    
    if(action.flags & UI_EditTextActionFlags_Delete)
    {
        result.replace.min = Min1U64(result.next_selection.cursor, result.next_selection.mark);
        result.replace.max = Max1U64(result.next_selection.cursor, result.next_selection.mark);
        result.with = S8("");
    }
    
    if(result.next_selection.cursor >= result.replace.max)
    {
        I64 difference = (I64)result.with.len - (I64)result.replace.max + (I64)result.replace.min;
        result.next_selection.cursor += difference;
    }
    if(result.next_selection.mark >= result.replace.max)
    {
        I64 difference = (I64)result.with.len - (I64)result.replace.max + (I64)result.replace.min;
        result.next_selection.mark += difference;
    }
    
    if(!(action.flags & UI_EditTextActionFlags_StickMark))
    {
        if(action.offset > 0 || result.with.len > 0)
        {
            result.next_selection.cursor = Max1U64(result.next_selection.cursor, result.next_selection.mark);
            result.next_selection.mark = result.next_selection.cursor;
        }
        else if(action.offset < 0)
        {
            result.next_selection.cursor = Min1U64(result.next_selection.cursor, result.next_selection.mark);
            result.next_selection.mark = result.next_selection.cursor;
        }
        else
        {
            result.next_selection.mark = result.next_selection.cursor;
        }
    }
    
    U64 new_len = string.len - result.replace.max + result.replace.min + result.with.len;
    result.next_selection.min = Clamp1U64(result.next_selection.min, 0, new_len);
    result.next_selection.max = Clamp1U64(result.next_selection.max, 0, new_len);
    
    return result;
}

Function UI_EditTextStatus
UI_EditS8Builder(G_Event *e, Range1U64 *selection, S8Builder *text, UI_EditTextFilterHook *filter_hook, void *filter_hook_user_data)
{
    UI_EditTextStatus result = 0;
    
    ArenaTemp scratch = OS_TCtxScratchBegin(0, 0);
    
    S8 string = S8FromBuilder(*text);
    
    UI_EditTextAction action = {0};
    if(UI_EditTextActionFromEvent(e, &action))
    {
        UI_EditTextOp op = UI_EditTextOpFromAction(scratch.arena, action, string, *selection, filter_hook, filter_hook_user_data);
        
        if(text->len + op.with.len - op.replace.max + op.replace.min <= text->cap)
        {
            B32 is_edited = S8BuilderReplaceRange(text, op.replace, op.with);
            
            result = (UI_EditTextStatus_Edited*is_edited |
                      UI_EditTextStatus_CursorMoved*(selection->cursor != op.next_selection.cursor) |
                      UI_EditTextStatus_MarkMoved*(selection->mark != op.next_selection.mark));
            
            *selection = op.next_selection;
            
            if(result)
            {
                G_EventConsume(e);
            }
        }
    }
    
    ArenaTempEnd(scratch);
    
    return result;
}

//-NOTE(tbt): animation

Function void
UI_Animate1F(F32 *a, F32 b, F32 speed)
{
    if(Abs1F(*a - b) > UI_AnimateSlop)
    {
        F64 half_life = UI_AnimateSpeedScale / speed;
        
        F64 dt = 1.0 / 60.0;
        if(0 != ui && 0 != ui->window.p)
        {
            dt = G_WindowFrameTimeGet(ui->window) / 1000000.0;
            
            if(0 != G_EventQueueFirstEventFromKind(G_WindowEventQueueGet(ui->window), G_EventKind_WindowSize, False))
            {
                // NOTE(tbt): animations get janked up while the window is resizing, so just skip
                *a = b;
                return;
            }
            
        }
        F64 x = (0.69314718056*dt) / (half_life + 1e-5);
        F64 negexp = 1.0 / (1.0 + x + 0.48*x*x + 0.235*x*x*x);
        *a = InterpolateLinear1F(*a, b, 1.0f - negexp);
        G_DoNotWaitForEventsUntil(OS_TimeInMicroseconds() + 500000ULL);
    }
}

Function void
UI_AnimateF(F32 a[], F32 b[], U64 n, F32 speed)
{
    for(U64 i = 0; i < n; i += 1)
    {
        UI_Animate1F(&a[i], b[i], speed);
    }
}

//-NOTE(tbt): misc. text rendering helpers

Function V2F
UI_TextPositionFromRect(FONT_PreparedText *pt, Range2F rect)
{
    ArenaTemp scratch = OS_TCtxScratchBegin(0, 0);
    
    V2F result = V2F(0.0f, 0.0f);
    
    if(2 == pt->glyphs_count && 1 == pt->lines_count)
    {
        FONT_PreparedTextGlyph *g = pt->lines->glyphs;
        result = Add2F(rect.min, Sub2F(Scale2F(Sub2F(Measure2F(rect), Measure2F(g->bounds)), 0.5f), g->bounds.min));
    }
    else
    {
        F32 line_height = FONT_ProviderVerticalAdvanceFromSize(pt->provider, pt->size);
        F32 width = pt->bounds.max.x - pt->bounds.min.x;
        F32 height = Max1U64(pt->lines_count, 1)*line_height;
        result.x = 0.5f*(rect.max.x - rect.min.x - width) - pt->bounds.min.x;
        result.y = 0.5f*(rect.max.y - rect.min.y - height) + 0.8f*line_height;
        result = Add2F(rect.min, result);
        result.x = Max1F(result.x, rect.min.x);
    }
    
    ArenaTempEnd(scratch);
    
    return Round2F(result);
}

// NOTE(tbt): assumes single line, LTR
//            returns an empty string if no truncation is necessary
Function S8
UI_TruncateWithSuffix(Arena *arena, S8 string, F32 width, FONT_PreparedText *pt, F32 padding, S8 suffix)
{
    S8 result = {0};
    
    Assert(1 == pt->lines_count);
    
    FONT_PreparedTextLine *line = pt->lines;
    
    if(line->bounds.max.x - line->bounds.min.x > width)
    {
        ArenaTemp scratch = OS_TCtxScratchBegin(&arena, 1);
        
        U64 character_index = 0;
        for(FONT_PreparedTextGlyph *glyph = line->glyphs; 0 != glyph; glyph = glyph->next)
        {
            if(glyph->bounds.max.x - line->bounds.min.x + padding >= width)
            {
                break;
            }
            character_index += 1;
        }
        
        U64 len = S8ByteIndexFromCharIndex(string, character_index);
        result = S8FromFmt(arena, "%.*s%.*s", (int)len, string.buffer, FmtS8(suffix));
        
        ArenaTempEnd(scratch);
    }
    
    return result;
}

//~NOTE(tbt): core

//-NOTE(tbt): node cache keying

// NOTE(tbt): stack of extra identifiers used when computing a key for a node

Function void
UI_SaltPush(S8 id)
{
    Arena *arena = G_WindowFrameArenaGet(ui->window);
    S8ListPush(arena, &ui->salt_stack, id);
}

Function void
UI_SaltPushFmtV(const char *fmt, va_list args)
{
    Arena *arena = G_WindowFrameArenaGet(ui->window);
    S8ListPushFmtV(arena, &ui->salt_stack, fmt, args);
}

Function void
UI_SaltPushFmt(const char *fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    UI_SaltPushFmtV(fmt, args);
    va_end(args);
}

Function void
UI_SaltPop(void)
{
    S8ListPop(&ui->salt_stack);
}

// NOTE(tbt): key utilities

Function UI_Key
UI_KeyNil(void)
{
    UI_Key result = {0};
    return result;
}

Function B32
UI_KeyIsNil(UI_Key *key)
{
    B32 result = (0 == key->id_len);
    return result;
}

Function UI_Key
UI_KeyFromIdAndSaltStack(S8 id, S8List salt_stack)
{
    UI_Key result = UI_KeyNil();
    if(id.len > 0)
    {
        ArenaTemp scratch = OS_TCtxScratchBegin(0, 0);
        
        S8List ids = salt_stack;
        S8ListPush(scratch.arena, &ids, id);
        
        S8 to_hash = S8ListJoin(scratch.arena, ids);
        
        MurmurHash3_x64_128(to_hash.buffer, to_hash.len, 0, result.hash);
        
        MemoryCopy(result.id_buffer, to_hash.buffer, Min1U64(to_hash.len, sizeof(result.id_buffer)));
        result.id_len = to_hash.len;
        
        ArenaTempEnd(scratch);
    }
    return result;
}

Function B32
UI_KeyMatch(UI_Key *a, UI_Key *b)
{
    S8 str_a = { .buffer = a->id_buffer, .len = Min1U64(a->id_len, sizeof(b->id_buffer)), };
    S8 str_b = { .buffer = b->id_buffer, .len = Min1U64(b->id_len, sizeof(b->id_buffer)), };
    B32 result = (MemoryMatch(a->hash, b->hash, sizeof(a->hash)) && a->id_len == b->id_len && S8Match(str_a, str_b, 0));
    return result;
}

Function B32
UI_KeyMatchOrNil(UI_Key *a, UI_Key *b)
{
    B32 result = (UI_KeyIsNil(a) || UI_KeyIsNil(b) || UI_KeyMatch(a, b));
    return result;
}

//-NOTE(tbt): control stacks

#define UI_ControlStackDef(NAME_L, NAME_U, TYPE, DEFAULT) \
Function TYPE                                                            \
Glue(UI_, Glue(NAME_U, Peek)) (void)                                     \
{                                                                        \
TYPE result = (DEFAULT);                                                \
if(ui->Glue(NAME_L, _stack_count) > 0)                                  \
{                                                                       \
result = ui->Glue(NAME_L, _stack)[ui->Glue(NAME_L, _stack_count) - 1]; \
}                                                                       \
return result;                                                          \
}                                                                        \
\
Function TYPE                                                            \
Glue(UI_, Glue(NAME_U, Pop)) (void)                                      \
{                                                                        \
TYPE result = (DEFAULT);                                                \
if(ui->Glue(NAME_L, _stack_count) > 0)                                  \
{                                                                       \
ui->Glue(NAME_L, _stack_count) -= 1;                                   \
result = ui->Glue(NAME_L, _stack)[ui->Glue(NAME_L, _stack_count)];     \
}                                                                       \
return result;                                                          \
}                                                                        \
\
Function void                                                            \
Glue(UI_, Glue(NAME_U, Push)) (TYPE v)                                   \
{                                                                        \
if(ui->Glue(NAME_L, _stack_count) < UI_ControlStackCap)                 \
{                                                                       \
ui->Glue(NAME_L, _stack)[ui->Glue(NAME_L, _stack_count)] = v;          \
ui->Glue(NAME_L, _stack_count) += 1;                                   \
ui->Glue(NAME_L, _set_next_flag) = False;                              \
}                                                                       \
} \
\
Function void                                                                \
Glue(UI_, Glue(SetNext, NAME_U)) (TYPE v)                                    \
{                                                                            \
if(ui->Glue(NAME_L, _stack_count) < UI_ControlStackCap)                     \
{                                                                           \
if(ui->Glue(NAME_L, _set_next_flag) && ui->Glue(NAME_L, _stack_count) > 0) \
{                                                                          \
ui->Glue(NAME_L, _stack)[ui->Glue(NAME_L, _stack_count) - 1] = v;         \
}                                                                          \
else                                                                       \
{                                                                          \
ui->Glue(NAME_L, _stack)[ui->Glue(NAME_L, _stack_count)] = v;             \
ui->Glue(NAME_L, _stack_count) += 1;                                      \
ui->Glue(NAME_L, _set_next_flag) = True;                                  \
}                                                                          \
}                                                                           \
}
UI_ControlStackList
#undef UI_ControlStackDef

//-NOTE(tbt): tree utilities

Function UI_Node *
UI_NodeLookup(UI_Key *key)
{
    UI_Node *result = UI_NodeLookupIncludingUnused(key);
    if(0 != result && result->last_touched_frame_index != ui->frame_index)
    {
        result = 0;
    }
    return result;
}

Function UI_Node *
UI_NodeLookupIncludingUnused(UI_Key *key)
{
    UI_Node *result = 0;
    if(!UI_KeyIsNil(key))
    {
        U64 index = key->hash[0] % ArrayCount(ui->buckets);
        result = &ui->buckets[index];
        while(0 != result && !UI_KeyMatch(key, &result->key))
        {
            result = result->next_hash;
        }
    }
    return result;
}

Function B32
UI_NodeIsChildOf(UI_Node *child, UI_Key *parent)
{
    B32 result = False;
    for(UI_Node *node = child; 0 != node; node = node->parent)
    {
        if(UI_KeyMatch(&node->key, parent))
        {
            result = True;
            break;
        }
    }
    return result;
}

//-NOTE(tbt): low-level node creation API

Function UI_NodeStrings
UI_TextAndIdFromString(S8 string)
{
    S8 id = string;
    S8 text = string;
    
    if(S8HasSubstring(string, S8("###"), 0))
    {
        S8Split split_on_triple = S8SplitMake(string, S8("###"), 0);
        S8SplitNext(&split_on_triple);
        text = split_on_triple.current;
        id = split_on_triple.string;
    }
    else
    {
        S8Split split_on_double = S8SplitMake(string, S8("##"), 0);
        S8SplitNext(&split_on_double);
        if(split_on_double.string.len > 0)
        {
            text = split_on_double.current;
        }
    }
    
    UI_NodeStrings result = { id, text, };
    return result;
}

Function UI_Node *
UI_NodeFromKey(UI_Key *key)
{
    UI_Node *result = 0;
    
    U64 bucket_index = key->hash[0] % ArrayCount(ui->buckets);
    
    UI_Node *bucket = &ui->buckets[bucket_index];
    
    result = bucket;
    while(0 != result && !UI_KeyMatchOrNil(&result->key, key))
    {
        result = result->next_hash;
    }
    
    if(0 == result)
    {
        result = ui->free_list;
        if(0 == result)
        {
            result = PushArray(G_WindowArenaGet(ui->window), UI_Node, 1);
        }
        else
        {
            ui->free_list = ui->free_list->next_hash;
        }
        result->next_hash = bucket->next_hash;
        bucket->next_hash = result;
    }
    
    if(UI_KeyIsNil(&result->key))
    {
        result->key = *key;
        result->is_new = True;
    }
    else
    {
        result->is_new = False;
    }
    
    Assert(result->last_touched_frame_index != ui->frame_index);
    
    result->last_touched_frame_index = ui->frame_index;
    
    return result;
}

Function void
UI_NodeUpdate(UI_Node *node, UI_Flags flags, S8 text)
{
    UI_Flags default_flags = UI_DefaultFlagsPeek();
    node->flags = (flags | default_flags);
    node->text = text;
    node->text.len = Min1U64(node->text.len, 1024);
    node->size[Axis2_X] = UI_WidthPeek();
    node->size[Axis2_Y] = UI_HeightPeek();
    node->bg = UI_BgColPeek();
    node->fg = UI_FgColPeek();
    node->growth = UI_GrowthPeek();
    node->stroke_width = UI_StrokeWidthPeek();
    node->corner_radius = UI_CornerRadiusPeek();
    node->shadow_radius = UI_ShadowRadiusPeek();
    node->hot_bg = UI_HotBgColPeek();
    node->hot_fg = UI_HotFgColPeek();
    node->hot_growth = UI_HotGrowthPeek();
    node->hot_stroke_width = UI_HotStrokeWidthPeek();
    node->hot_corner_radius = UI_HotCornerRadiusPeek();
    node->hot_shadow_radius = UI_HotShadowRadiusPeek();
    node->hot_cursor_kind = UI_HotCursorKindPeek();
    node->active_bg = UI_ActiveBgColPeek();
    node->active_fg = UI_ActiveFgColPeek();
    node->active_growth = UI_ActiveGrowthPeek();
    node->active_stroke_width = UI_ActiveStrokeWidthPeek();
    node->active_corner_radius = UI_ActiveCornerRadiusPeek();
    node->active_shadow_radius = UI_ActiveShadowRadiusPeek();
    node->active_cursor_kind = UI_ActiveCursorKindPeek();
    node->font_provider = UI_FontPeek();
    node->scale = UI_ScalePeek();
    node->font_size = UI_FontSizePeek()*node->scale;
    node->children_layout_axis = UI_ChildrenLayoutPeek();
    if(node->flags & UI_Flags_Floating)
    {
        node->target_relative_rect.min = Scale2F(UI_FloatingPositionPeek(), node->scale);
    }
    node->texture = UI_TexturePeek();
    node->texture_region = UI_TextureRegionPeek();
    node->nav_context = UI_NavPeek();
    node->text_trunc_suffix = UI_TextTruncSuffixPeek();
    node->parent = 0;
    node->next = 0;
    node->prev = 0;
    node->first = 0;
    node->last = 0;
    
    // TODO(tbt):  control stacks just thread local globals, not part of UI_State
    
#define UI_ControlStackDef(NAME_L, NAME_U, TYPE, DEFAULT) \
if(ui->Glue(NAME_L, _set_next_flag)) \
{ \
Glue(UI_, Glue(NAME_U, Pop)) ();\
ui->Glue(NAME_L, _set_next_flag) = False;\
}
    UI_ControlStackList
#undef UI_ControlStackDef
    
    if(UI_NavInterfaceStackMatchTop(&node->nav_context.key))
    {
        node->nav_context.hook(node->nav_context.user_data, node);
    }
}

Function void
UI_NodeInsert(UI_Node *node, UI_Node *parent)
{
    Assert(0 == node->parent);
    Assert(0 == node->prev);
    Assert(0 == node->next);
    
    node->parent = parent;
    if(0 == parent->last)
    {
        parent->first = node;
    }
    else
    {
        parent->last->next = node;
    }
    node->prev = parent->last;
    parent->last = node;
}

//-NOTE(tbt): higher level wrappers over the above

Function UI_Node *
UI_NodeMake(UI_Flags flags, S8 string)
{
    UI_Node *result = 0;
    
    UI_Node *parent = UI_ParentPeek();
    
    UI_NodeStrings strings = UI_TextAndIdFromString(string);
    
    UI_Key key = UI_KeyFromIdAndSaltStack(strings.id, ui->salt_stack);
    
    if(UI_KeyIsNil(&key))
    {
        result = PushArray(G_WindowFrameArenaGet(ui->window), UI_Node, 1);
    }
    else
    {
        result = UI_NodeFromKey(&key);
    }
    
    UI_NodeUpdate(result, flags, S8Clone(G_WindowFrameArenaGet(ui->window), strings.text));
    UI_NodeInsert(result, parent);
    
    return result;
}

Function UI_Node *
UI_NodeMakeFromFmtV(UI_Flags flags, const char *fmt, va_list args)
{
    ArenaTemp scratch = OS_TCtxScratchBegin(0, 0);
    S8 string = S8FromFmtV(scratch.arena, fmt, args);
    UI_Node *result = UI_NodeMake(flags, string);
    ArenaTempEnd(scratch);
    return result;
}

Function UI_Node *
UI_NodeMakeFromFmt(UI_Flags flags, const char *fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    UI_Node *result = UI_NodeMakeFromFmtV(flags, fmt, args);
    va_end(args);
    return result;
}

Function UI_Node *
UI_OverlayFromString(S8 string)
{
    UI_Node *result = 0;
    
    UI_NodeStrings strings = UI_TextAndIdFromString(string);
    
    UI_Key key = UI_KeyFromIdAndSaltStack(strings.id, ui->salt_stack);
    
    if(UI_KeyIsNil(&key))
    {
        result = PushArray(G_WindowFrameArenaGet(ui->window), UI_Node, 1);
    }
    else
    {
        result = UI_NodeFromKey(&key);
    }
    
    Assert(0 == result->parent);
    
    UI_NodeUpdate(result, 0, S8Clone(G_WindowFrameArenaGet(ui->window), strings.text));
    
    if(0 == result->next && 0 == result->prev)
    {
        result->next = ui->root.next;
        result->prev = &ui->root;
        if(0 != ui->root.next)
        {
            ui->root.next->prev = result;
        }
        ui->root.next = result;
    }
    
    result->size[Axis2_X] = UI_SizeNone();
    result->size[Axis2_Y] = UI_SizeNone();
    result->rect = G_WindowClientRectGet(ui->window);
    result->clipped_rect = ui->root.rect;
    result->computed_size = ui->root.rect.max;
    
    return result;
}

//-NOTE(tbt): navigation

Function B32
UI_NavInterfaceStackPush(UI_Key *key)
{
    B32 result = False;
    if(ui->interface_stack_count < ArrayCount(ui->interface_stack) && !UI_NavInterfaceStackMatchTop(key))
    {
        MemoryCopy(&ui->interface_stack[ui->interface_stack_count], key, sizeof(*key));
        ui->interface_stack_count += 1;
        result = True;
    }
    return result;
}

Function UI_Key
UI_NavInterfaceStackPop(void)
{
    UI_Key result = UI_KeyNil();
    if(ui->interface_stack_count > 0)
    {
        ui->interface_stack_count -= 1;
        result = ui->interface_stack[ui->interface_stack_count];
    }
    return result;
}

Function UI_Key
UI_NavInterfaceStackPeek(void)
{
    UI_Key result = UI_KeyNil();
    if(ui->interface_stack_count > 0)
    {
        result = ui->interface_stack[ui->interface_stack_count - 1];
    }
    return result;
}

Function B32
UI_NavInterfaceStackMatchTop(UI_Key *key)
{
    UI_Key top = UI_NavInterfaceStackPeek();
    B32 result = (UI_KeyIsNil(key) && UI_KeyIsNil(&top)) || UI_KeyMatch(&top, key);
    return result;
}

Function UI_Key
UI_NavInterfaceStackPopCond(UI_Key *match_top)
{
    UI_Key result = {0};
    if(UI_NavInterfaceStackMatchTop(match_top))
    {
        result = UI_NavInterfaceStackPop();
    }
    return result;
}

Function void
UI_NavDefault(void *user_data, UI_Node *node)
{
    UI_Node *state = user_data;
    
    if(UI_KeyMatch(&state->nav_default_keyboard_focus, &node->key))
    {
        node->flags |= UI_Flags_KeyboardFocused;
    }
}

Function UI_Node *
UI_NavDefaultNext(UI_Node *node)
{
    UI_Node *result = 0;
    
    UI_Node *current = UI_NodeLookup(&node->nav_default_keyboard_focus);
    if(0 == current)
    {
        current = node;
    }
    
    while(0 != current)
    {
        UI_Node *next = current->first;
        if(0 == next)
        {
            next = current->next;
        }
        if(0 == next)
        {
            UI_Node *parent = current->parent;
            while(0 != parent && 0 == parent->next)
            {
                parent = parent->parent;
            }
            if(0 != parent)
            {
                next = parent->next;
            }
        }
        current = next;
        
        if(0 != current &&
           (current->flags & UI_Flags_Pressable) &&
           !(current->flags & UI_Flags_NavDefaultFilter) &&
           current->nav_context.hook == &UI_NavDefault &&
           current->nav_context.user_data == node)
        {
            node->nav_default_keyboard_focus = current->key;
            result = current;
            break;
        }
    }
    
    if(!UI_NodeIsChildOf(current, &node->key))
    {
        current = node;
    }
    
    ui->was_last_interaction_keyboard = True;
    
    return result;
}

Function UI_Node *
UI_NavDefaultPrev(UI_Node *node)
{
    UI_Node *result = 0;
    
    UI_Node *current = UI_NodeLookup(&node->nav_default_keyboard_focus);
    if(0 == current)
    {
        current = node;
    }
    
    while(0 != current)
    {
        UI_Node *next = current->last;
        if(0 == next)
        {
            next = current->prev;
        }
        if(0 == next)
        {
            UI_Node *parent = current->parent;
            while(0 != parent && 0 == parent->prev)
            {
                parent = parent->parent;
            }
            if(0 != parent)
            {
                next = parent->prev;
            }
        }
        current = next;
        
        if(0 != current &&
           (current->flags & UI_Flags_Pressable) &&
           !(current->flags & UI_Flags_NavDefaultFilter) &&
           current->nav_context.hook == &UI_NavDefault &&
           current->nav_context.user_data == node)
        {
            node->nav_default_keyboard_focus = current->key;
            result = current;
            break;
        }
    }
    
    if(!UI_NodeIsChildOf(current, &node->key))
    {
        current = node;
    }
    
    ui->was_last_interaction_keyboard = True;
    
    return result;
}

//-NOTE(tbt): interaction

Function UI_Use
UI_UseFromNode(UI_Node *node)
{
    UI_Use result = {0};
    
    result.node = node;
    
    G_EventQueue *events = G_WindowEventQueueGet(ui->window);
    V2F mouse_position = G_WindowMousePositionGet(ui->window);
    V2F mouse_delta = G_EventQueueSumDelta(events, G_EventKind_MouseMove, False);
    F32 s = node->scale;
    
    B32 is_mouse_over = HasPoint2F(node->clipped_rect, mouse_position);
    
    result.is_hovering = (is_mouse_over &&
                          UI_KeyMatchOrNil(&ui->hot, &node->key) &&
                          UI_KeyMatchOrNil(&ui->active, &node->key));
    
    if(result.is_hovering || UI_KeyMatch(&ui->active, &node->key))
    {
        result.is_left_down = G_EventQueueHasKeyDown(events, G_Key_MouseButtonLeft, G_ModifierKeys_Any, False);
        result.is_left_up = G_EventQueueHasKeyUp(events, G_Key_MouseButtonLeft, G_ModifierKeys_Any, False);
        result.is_right_down = G_EventQueueHasKeyDown(events, G_Key_MouseButtonRight, G_ModifierKeys_Any, False);
        result.is_right_up = G_EventQueueHasKeyUp(events, G_Key_MouseButtonRight, G_ModifierKeys_Any, False);
        result.is_double_clicked = G_EventQueueHasKeyDown(events, G_Key_MouseButtonDoubleClick, G_ModifierKeys_Any, False);
    }
    
    if(result.is_hovering && (node->flags & UI_Flags_Pressable))
    {
        ui->hot = node->key;
        if(result.is_left_down)
        {
            G_EventQueueHasKeyDown(events, G_Key_MouseButtonLeft, G_ModifierKeys_Any, True); // NOTE(tbt): consume event
            
            ui->last_initial_mouse_position = mouse_position;
            ui->last_initial_mouse_relative_position = Sub2F(mouse_position, node->rect.min);
            
            ui->active = node->key;
        }
    }
    
    if(is_mouse_over && (node->flags & UI_Flags_Scrollable))
    {
        result.scroll_delta = G_EventQueueSumDelta(events, G_EventKind_MouseScroll, True);
    }
    
    B32 is_active = UI_KeyMatch(&ui->active, &node->key);
    B32 is_hot = UI_KeyMatch(&ui->hot, &node->key);
    
    if(is_active)
    {
        node->is_dragging = (node->is_dragging || Abs1F(mouse_delta.x) > 2.0f || Abs1F(mouse_delta.y) > 2.0f);
        result.is_pressed = (is_hot && !G_WindowKeyStateGet(ui->window, G_Key_MouseButtonLeft));
        result.is_dragging = node->is_dragging;
        
        result.initial_mouse_position = ui->last_initial_mouse_position;
        result.initial_mouse_relative_position = ui->last_initial_mouse_relative_position;
    }
    else
    {
        node->is_dragging = False;
    }
    
    if(result.is_dragging)
    {
        result.drag_delta = mouse_delta;
        G_EventQueueSumDelta(events, G_EventKind_MouseMove, True);
    }
    
    if(result.is_pressed && &UI_NavDefault == node->nav_context.hook)
    {
        UI_Node *state = node->nav_context.user_data;
        state->nav_default_keyboard_focus = node->key;
    }
    
    if(node->flags & UI_Flags_KeyboardFocused)
    {
        if(node->flags & UI_Flags_Pressable)
        {
            if(G_EventQueueHasKeyDown(events, G_Key_Enter, G_ModifierKeys_Any, False))
            {
                result.is_pressed = True;
                ui->was_last_interaction_keyboard = True;
            }
        }
        if(node->flags & UI_Flags_Scrollable)
        {
            if(G_EventQueueHasKeyDown(events, G_Key_Up, G_ModifierKeys_Any, False))
            {
                result.scroll_delta.y += 1.0f;
            }
            if(G_EventQueueHasKeyDown(events, G_Key_Down, G_ModifierKeys_Any, False))
            {
                result.scroll_delta.y -= 1.0f;
            }
            if(G_EventQueueHasKeyDown(events, G_Key_Left, G_ModifierKeys_Any, False))
            {
                result.scroll_delta.x -= 1.0f;
            }
            if(G_EventQueueHasKeyDown(events, G_Key_Right, G_ModifierKeys_Any, False))
            {
                result.scroll_delta.x += 1.0f;
            }
        }
    }
    
    if(node->flags & UI_Flags_ScrollChildrenX)
    {
        if(!(node->flags & UI_Flags_ScrollChildrenY))
        {
            node->target_view_offset.x += result.scroll_delta.y*UI_ScrollSpeed*s;
        }
        node->target_view_offset.x += result.scroll_delta.x*UI_ScrollSpeed*s;
    }
    
    if(node->flags & UI_Flags_ScrollChildrenY)
    {
        node->target_view_offset.y += result.scroll_delta.y*UI_ScrollSpeed*s;
    }
    
    return result;
}

//-NOTE(tbt): measurement

Function F32
UI_MeasureSizeFromText(UI_Node *node, Axis2 axis)
{
    F32 result = 0.0f;
    FONT_PreparedText *pt = UI_PreparedTextFromS8(node->font_provider, node->font_size, node->text);
    if(Axis2_X == axis)
    {
        result = Measure2F(pt->bounds).x;
    }
    else
    {
        result = pt->lines_count*FONT_ProviderVerticalAdvanceFromSize(node->font_provider, node->font_size);
    }
    return result;
}

Function Range1F
UI_MeasureSumOfChildren(UI_Node *root, Axis2 axis)
{
    Range1F result = {0};
    
    for(UI_Node *child = root->first; 0 != child; child = child->next)
    {
        if(!(child->flags & UI_Flags_Floating))
        {
            if(root->children_layout_axis == axis)
            {
                result.min += child->computed_size.elements[axis]*child->size[axis].strictness;
                result.max += child->computed_size.elements[axis];
            }
            else
            {
                Range1F child_size = Range1F(child->computed_size.elements[axis]*child->size[axis].strictness,
                                             child->computed_size.elements[axis]);
                result = Bounds1F(result, child_size);
            }
        }
    }
    
    return result;
}

//-NOTE(tbt): layout

Function void
UI_LayoutPassIndependentSizes(UI_Node *root, Axis2 axis)
{
    F32 s = root->scale;
    
    if(UI_SizeMode_Pixels == root->size[axis].mode)
    {
        root->computed_size.elements[axis] = root->size[axis].f*s;
    }
    else if(UI_SizeMode_FromText == root->size[axis].mode)
    {
        F32 text_size = UI_MeasureSizeFromText(root, axis);
        F32 padding = root->size[axis].f*s;
        root->computed_size.elements[axis] = text_size + 2.0f*padding;
    }
    
    for(UI_Node *child = root->first; 0 != child; child = child->next)
    {
        UI_LayoutPassIndependentSizes(child, axis);
    }
}

Function void
UI_LayoutPassUpwardsDependentSizes(UI_Node *root, Axis2 axis)
{
    if(UI_SizeMode_PercentageOfAncestor == root->size[axis].mode)
    {
        UI_Node *ancestor = root->parent;
        while(0 != ancestor && ancestor->size[axis].mode == UI_SizeMode_SumOfChildren)
        {
            ancestor = ancestor->parent;
        }
        Assert(0 != ancestor);
        
        root->computed_size.elements[axis] = ancestor->computed_size.elements[axis]*root->size[axis].f;
    }
    
    for(UI_Node *child = root->first; 0 != child; child = child->next)
    {
        UI_LayoutPassUpwardsDependentSizes(child, axis);
    }
}

Function void
UI_LayoutPassDownwardsDependentSizes(UI_Node *root, Axis2 axis)
{
    for(UI_Node *child = root->first; 0 != child; child = child->next)
    {
        UI_LayoutPassDownwardsDependentSizes(child, axis);
    }
    
    if(UI_SizeMode_SumOfChildren == root->size[axis].mode)
    {
        root->computed_size.elements[axis] = UI_MeasureSumOfChildren(root, axis).max;
    }
}

Function void
UI_LayoutPassSolveViolations(UI_Node *root, Axis2 axis)
{
    root->overflow.elements[axis] = 0.0f;
    
    if(axis == root->children_layout_axis)
    {
        Range1F total = UI_MeasureSumOfChildren(root, axis);
        
        F32 err = total.max - root->computed_size.elements[axis];
        
        if(err > 0.0f)
        {
            F32 new_sum = 0.0f;
            
            F32 total_can_lose = Measure1F(total);
            for(UI_Node *child = root->first; 0 != child; child = child->next)
            {
                if((Axis2_X == axis && !(root->flags & UI_Flags_OverflowX)) ||
                   (Axis2_Y == axis && !(root->flags & UI_Flags_OverflowY)))
                {
                    F32 min_size = child->size[axis].strictness*child->computed_size.elements[axis];
                    F32 can_lose = (1.0f - child->size[axis].strictness)*child->computed_size.elements[axis];
                    child->computed_size.elements[axis] -= err*(can_lose / total_can_lose);
                    child->computed_size.elements[axis] = Max1F(child->computed_size.elements[axis], min_size);
                }
                new_sum += child->computed_size.elements[axis];
            }
            
            root->overflow.elements[axis] = Max1F(new_sum - root->computed_size.elements[axis], 0.0f);
        }
    }
    else
    {
        for(UI_Node *child = root->first; 0 != child; child = child->next)
        {
            if((Axis2_X == axis && !(root->flags & UI_Flags_OverflowX)) ||
               (Axis2_Y == axis && !(root->flags & UI_Flags_OverflowY)))
            {
                F32 err = Max1F(child->computed_size.elements[axis] - root->computed_size.elements[axis], 0.0f);
                F32 min_size = child->size[axis].strictness*child->computed_size.elements[axis];
                child->computed_size.elements[axis] = Max1F(child->computed_size.elements[axis] - err, min_size);
            }
            root->overflow.elements[axis] = Max1F(root->overflow.elements[axis], child->computed_size.elements[axis] - root->computed_size.elements[axis]);
        }
    }
    
    if(root->flags & UI_Flags_ClampViewOffsetToOverflow)
    {
        root->view_offset.elements[axis] = Clamp1F(root->view_offset.elements[axis], -root->overflow.elements[axis], 0.0f);
        root->target_view_offset.elements[axis] = Clamp1F(root->target_view_offset.elements[axis], -root->overflow.elements[axis], 0.0f);
    }
    
    for(UI_Node *child = root->first; 0 != child; child = child->next)
    {
        UI_LayoutPassSolveViolations(child, axis);
    }
}

Function void
UI_LayoutPassComputeRelativeRects(UI_Node *root)
{
    F32 x = 0.0f;
    for(UI_Node *child = root->first; 0 != child; child = child->next)
    {
        if(!(child->flags & UI_Flags_Floating))
        {
            child->target_relative_rect.min.elements[ root->children_layout_axis] = x;
            child->target_relative_rect.min.elements[!root->children_layout_axis] = 0.0f;
            x += child->computed_size.elements[root->children_layout_axis];
        }
        child->target_relative_rect.max.elements[ root->children_layout_axis] = child->target_relative_rect.min.elements[ root->children_layout_axis] + child->computed_size.elements[ root->children_layout_axis];
        child->target_relative_rect.max.elements[!root->children_layout_axis] = child->target_relative_rect.min.elements[!root->children_layout_axis] + child->computed_size.elements[!root->children_layout_axis];
        
        UI_LayoutPassComputeRelativeRects(child);
    }
}

Function void
UI_LayoutPassComputeFinalRects(UI_Node *root)
{
    F32 x = 0.0f;
    for(UI_Node *child = root->first; 0 != child; child = child->next)
    {
        child->rect.min.x = child->relative_rect.min.x + root->rect.min.x + root->view_offset.x;
        child->rect.min.y = child->relative_rect.min.y + root->rect.min.y + root->view_offset.y;
        child->rect.max.x = child->relative_rect.max.x + root->rect.min.x + root->view_offset.x;
        child->rect.max.y = child->relative_rect.max.y + root->rect.min.y + root->view_offset.y;
        
        UI_LayoutPassComputeFinalRects(child);
    }
}

//-NOTE(tbt): animation

Function void
UI_AnimationPass(UI_Node *root, UI_Key *hot_key, UI_Key *active_key)
{
    B32 is_hot = UI_KeyMatch(&root->key, hot_key);
    B32 is_active = UI_KeyMatch(&root->key, active_key);
    
    if((root->flags & UI_Flags_AnimateSmoothScroll) && !root->is_new)
    {
        UI_AnimateF(root->view_offset.elements, root->target_view_offset.elements, 2, UI_ScrollSpeed);
    }
    else
    {
        root->view_offset = root->target_view_offset;
    }
    
    if((root->flags & UI_Flags_AnimateSmoothLayout) && (!root->is_new || (root->flags & UI_Flags_AnimateIn)))
    {
        UI_AnimateF(root->relative_rect.elements, root->target_relative_rect.elements, 4, 15.0f);
    }
    else
    {
        root->relative_rect = root->target_relative_rect;
    }
    
    if(root->flags & UI_Flags_AnimateHot)
    {
        UI_Animate1F(&root->hot_t, is_hot, 15.0f);
    }
    
    if(root->flags & UI_Flags_AnimateActive)
    {
        UI_Animate1F(&root->active_t, is_active, 15.0f);
    }
    
    /*
    if(root->flags & UI_Flags_SetCursorKind)
    {
        if(is_active)
        {
            ui->next_cursor_kind = root->active_cursor_kind;
        }
        else if(HasPoint2F(root->clipped_rect, G_WindowMousePositionGet(ui->window)))
        {
            ui->next_cursor_kind = root->hot_cursor_kind;
        }
    }
*/
    
    UI_Node *ancestor = root->parent;
    while(0 != ancestor && UI_KeyIsNil(&ancestor->key))
    {
        ancestor = ancestor->parent;
    }
    if(0 != ancestor)
    {
        if((root->flags & UI_Flags_AnimateInheritHot) && !(root->flags & UI_Flags_AnimateHot))
        {
            root->hot_t = ancestor->hot_t;
        }
        if((root->flags & UI_Flags_AnimateInheritActive) && !(root->flags & UI_Flags_AnimateActive))
        {
            root->active_t = ancestor->active_t;
        }
    }
    
    //UI_Animate1F(&root->keyboard_focus_t, !!(root->flags & UI_Flags_KeyboardFocused) && ui->was_last_interaction_keyboard, 15.0f);
    
    for(UI_Node *child = root->first; 0 != child; child = child->next)
    {
        UI_AnimationPass(child, hot_key, active_key);
    }
}

//-NOTE(tbt): rendering

Function void
UI_RenderPass(UI_Node *root, Range2F mask, OpaqueHandle window, DRW_List *draw_list)
{
    if(root->flags & UI_Flags_Hidden)
    {
        return;
    }
    
    Arena *frame_arena = G_WindowFrameArenaGet(window);
    
    F32 s = root->scale;
    
    V4F fg = root->fg;
    V4F bg = root->bg;
    F32 growth = root->growth;
    F32 stroke_width = root->stroke_width;
    F32 corner_radius = root->corner_radius;
    F32 shadow_radius = root->shadow_radius;
    
    if(root->flags & UI_Flags_DrawHotStyle)
    {
        fg = ColMix(fg, root->hot_fg, root->hot_t);
        bg = ColMix(bg, root->hot_bg, root->hot_t);
        growth = InterpolateLinear1F(growth, root->hot_growth, root->hot_t);
        stroke_width = InterpolateLinear1F(stroke_width, root->hot_stroke_width, root->hot_t);
        corner_radius = InterpolateLinear1F(corner_radius, root->hot_corner_radius, root->hot_t);
        shadow_radius = InterpolateLinear1F(shadow_radius, root->hot_shadow_radius, root->hot_t);
    }
    
    if(root->flags & UI_Flags_DrawActiveStyle)
    {
        fg = ColMix(fg, root->active_fg, root->active_t);
        bg = ColMix(bg, root->active_bg, root->active_t);
        growth = InterpolateLinear1F(growth, root->active_growth, root->active_t);
        stroke_width = InterpolateLinear1F(stroke_width, root->active_stroke_width, root->active_t);
        corner_radius = InterpolateLinear1F(corner_radius, root->active_corner_radius, root->active_t);
        shadow_radius = InterpolateLinear1F(shadow_radius, root->active_shadow_radius, root->active_t);
    }
    
    growth *= s;
    stroke_width *= s;
    corner_radius *= s;
    
    Range2F rect = Grow2F(root->rect, U2F(growth));
    if(rect.min.x > rect.max.x)
    {
        rect.max.x = rect.min.x;
    }
    if(rect.min.y > rect.max.y)
    {
        rect.max.y = rect.min.y;
    }
    
    if((root->flags & UI_Flags_DrawDropShadow) && shadow_radius > 1.0f)
    {
        R2D_Quad quad =
        {
            .dst = Grow2F(rect, U2F(shadow_radius)),
            .src = r2d_entire_texture,
            .stroke_colour = Col(0.0f, 0.0f, 0.0f, fg.a / shadow_radius),
            .fill_colour = Col(0.0f, 0.0f, 0.0f, bg.a / shadow_radius),
            .stroke_width = stroke_width + shadow_radius,
            .corner_radius = corner_radius,
            .edge_softness = shadow_radius,
            .mask = mask,
        };
        DRW_Sprite(frame_arena, draw_list, quad, root->texture);
    }
    
    if(root->flags & (UI_Flags_DrawStroke | UI_Flags_DrawFill))
    {
        R2D_Quad quad =
        {
            .dst = rect,
            .src = root->texture_region,
            .edge_softness = Clamp1F(corner_radius, 0.0f, 1.0f),
            .corner_radius = corner_radius,
            .mask = mask,
        };
        if(root->flags & UI_Flags_DrawStroke)
        {
            quad.stroke_colour = fg;
            quad.stroke_width = (stroke_width > UI_AnimateSlop*2.0f) ? stroke_width : 0.0f;
        }
        if(root->flags & UI_Flags_DrawFill)
        {
            quad.fill_colour = bg;
        }
        DRW_Sprite(frame_arena, draw_list, quad, root->texture);
    }
    
    root->clipped_rect = Intersection2F(rect, mask);
    if(!(root->flags & UI_Flags_DoNotMask))
    {
        mask = root->clipped_rect;
    }
    
    if(root->flags & (UI_Flags_DrawText | UI_Flags_DrawTextCursor))
    {
        ArenaTemp scratch = OS_TCtxScratchBegin(0, 0);
        
        FONT_PreparedText *pt = UI_PreparedTextFromS8(root->font_provider, root->font_size, root->text);
        V2F text_pos = UI_TextPositionFromRect(pt, rect); 
        
        if(root->flags & UI_Flags_DrawText)
        {
            if(1 == pt->lines_count && root->text_trunc_suffix.len > 0 && !(root->flags & (UI_Flags_OverflowX | UI_Flags_DoNotMask)))
            {
                FONT_PreparedText *suffix_pt = UI_PreparedTextFromS8(root->font_provider, root->font_size, root->text_trunc_suffix);
                
                F32 max_width = root->clipped_rect.max.x - text_pos.x;
                S8 truncated_string = UI_TruncateWithSuffix(scratch.arena, root->text, max_width, pt, suffix_pt->bounds.max.x - suffix_pt->bounds.min.x, root->text_trunc_suffix);
                if(truncated_string.len > 0)
                {
                    pt = UI_PreparedTextFromS8(root->font_provider, root->font_size, truncated_string);
                }
            }
            
            DRW_Text(frame_arena, draw_list, pt, text_pos, fg, mask);
        }
        
        if(root->flags & UI_Flags_DrawTextCursor)
        {
            F32 v_advance = FONT_ProviderVerticalAdvanceFromSize(root->font_provider, root->font_size);
            
            F32 caret_width = 2.0f;
            V2F caret_pos = V2F(0.0f, -v_advance);
            
            Range1U64 selection = root->text_selection;
            Range1U64 char_indices = Range1U64(S8CharIndexFromByteIndex(root->text, selection.cursor),
                                               S8CharIndexFromByteIndex(root->text, selection.mark));
            Assert(char_indices.min <= pt->glyphs_count);
            Assert(char_indices.max <= pt->glyphs_count);
            
            FONT_PreparedTextLine *line_start = 0;
            FONT_PreparedTextLine *line_end = 0;
            FONT_PreparedTextLine *cursor_line = 0;
            FONT_PreparedTextGlyph *l = FONT_PreparedTextGlyphFromIndex(pt, Min1U64(char_indices.cursor, char_indices.mark), &line_start);
            FONT_PreparedTextGlyph *r = FONT_PreparedTextGlyphFromIndex(pt, Max1U64(char_indices.cursor, char_indices.mark), &line_end);
            FONT_PreparedTextGlyph *c = FONT_PreparedTextGlyphFromIndex(pt, char_indices.cursor, &cursor_line);
            
            if(0 != c)
            {
                for(FONT_PreparedTextLine *line = line_start; 0 != line; line = line->next)
                {
                    Range2F highlight_relative = Range2F(V2F(line->bounds.min.x, line->base_line - v_advance),
                                                         V2F(line->bounds.max.x, line->base_line));
                    if(line == line_start && 0 != l)
                    {
                        highlight_relative.min.x = l->bounds.min.x;
                    }
                    if(line == line_end && 0 != r)
                    {
                        highlight_relative.max.x = r->bounds.min.x;
                    }
                    
                    Range2F highlight =
                    {
                        .min = Add2F(text_pos, highlight_relative.min),
                        .max = Add2F(text_pos, highlight_relative.max),
                    };
                    R2D_Quad highlight_quad =
                    {
                        .dst = highlight,
                        .src = r2d_entire_texture,
                        .fill_colour = Scale4F(root->fg, 0.4f),
                        .mask = mask,
                    };
                    DRW_Sprite(frame_arena, draw_list, highlight_quad, R2D_TextureNil());
                    
                    if(line == line_end)
                    {
                        break;
                    }
                }
                
                caret_pos = V2F(c->bounds.min.x, cursor_line->base_line - v_advance);
            }
            
            root->target_text_cursor_pos = caret_pos;
            UI_AnimateF(root->text_cursor_pos.elements, root->target_text_cursor_pos.elements, 2, 15.0f);
            
            F32 blink_anim = Min1F(Abs1F(Sin1F(OS_TimeInSeconds()*2.0f)) + root->cursor_blink_cooldown, 1.0f);
            if(root->cursor_blink_cooldown < 1.0f)
            {
                G_DoNotWaitForEventsUntil(OS_TimeInMicroseconds() + 1000000ULL);
            }
            R2D_Quad caret_quad =
            {
                .dst = RectMake2F(Add2F(root->text_cursor_pos, text_pos), V2F(2.0f, v_advance)),
                .src = r2d_entire_texture,
                .fill_colour = Scale4F(root->fg, blink_anim),
                .mask = mask,
            };
            DRW_Sprite(frame_arena, draw_list, caret_quad, R2D_TextureNil());
        }
        
        ArenaTempEnd(scratch);
    }
    
    for(UI_Node *child = root->last; 0 != child; child = child->prev)
    {
        UI_RenderPass(child, mask, window, draw_list);
    }
}

Function void
UI_RenderKeyboardFocusHighlight(UI_Node *root, Range2F mask, OpaqueHandle window, DRW_List *draw_list)
{
    if(root->keyboard_focus_t > 2.0f*UI_AnimateSlop)
    {
        R2D_Quad quad =
        {
            .dst = Grow2F(root->rect, U2F(root->keyboard_focus_t*10.0f)),
            .src = r2d_entire_texture,
            .edge_softness = 3.0f,
            .corner_radius = root->keyboard_focus_t*4.0f,
            .stroke_colour = Scale4F(ColFromHex(0xb48eadff), root->keyboard_focus_t),
            .stroke_width = root->keyboard_focus_t*6.0f,
            .mask = mask,
        };
        DRW_Sprite(G_WindowFrameArenaGet(window), draw_list, quad, R2D_TextureNil());
    }
    
    if(!(root->flags & UI_Flags_DoNotMask))
    {
        mask = root->clipped_rect;
    }
    
    for(UI_Node *child = root->first; 0 != child; child = child->next)
    {
        UI_RenderKeyboardFocusHighlight(child, mask, window, draw_list);
    }
}

//-NOTE(tbt): 

Function UI_State *
UI_Init(OpaqueHandle window)
{
    Arena *arena = G_WindowArenaGet(window);
    UI_State *result = PushArray(arena, UI_State, 1);
    result->window = window;
    result->frame_index = 1;
    return result;
}

Function void
UI_Begin(UI_State *ui_state)
{
    ui = ui_state;
    
    ui->frame_index += 1;
    
    if(0 == ui_font.p)
    {
        // NOTE(tbt): this leaks the previous font when the DPI changes - hopefully the DPI shouldn't change too many times per run so shouldn't be an issue
        Arena *arena = G_WindowArenaGet(ui->window);
        S8 font_data_regular = { .buffer = font_atkinson_hyperlegible_regular, .len = font_atkinson_hyperlegible_regular_len, };
        S8 font_data_bold = { .buffer = font_atkinson_hyperlegible_bold, .len = font_atkinson_hyperlegible_bold_len, };
        S8 font_data_icons = { .buffer = font_icons, .len = font_icons_len, };
        ui_font = FONT_ProviderFromTTF(arena, font_data_regular);
        ui_font_bold = FONT_ProviderFromTTF(arena, font_data_bold);
        ui_font_icons = FONT_ProviderFromTTF(arena, font_data_icons);
    }
    
    ui->next_cursor_kind = ui->default_cursor_kind;
    ui->hot = UI_KeyNil();
    
    ui->root.size[Axis2_X] = UI_SizeNone();
    ui->root.size[Axis2_Y] = UI_SizeNone();
    ui->root.rect = G_WindowClientRectGet(ui->window);
    ui->root.clipped_rect = ui->root.rect;
    ui->root.computed_size = ui->root.rect.max;
    ui->root.first = 0;
    ui->root.last = 0;
    ui->root.prev = 0;
    ui->root.next = 0;
    
    UI_ScalePush(G_WindowScaleFactorGet(ui->window));
}

Function void
UI_End(void)
{
    UI_ScalePop();
    
#define UI_ControlStackDef(NAME_L, NAME_U, TYPE, DEFAULT) \
Assert(0 == ui->Glue(NAME_L, _stack_count));
    UI_ControlStackList
#undef UI_ControlStackDef
    
    for(UI_Node *root = &ui->root; 0 != root; root = root->next)
    {
        for(Axis2 axis = 0; axis < Axis2_MAX; axis += 1)
        {
            UI_LayoutPassIndependentSizes(root, axis);
            UI_LayoutPassUpwardsDependentSizes(root, axis);
            UI_LayoutPassDownwardsDependentSizes(root, axis);
            UI_LayoutPassSolveViolations(root, axis);
        }
        UI_LayoutPassComputeRelativeRects(root);
        UI_AnimationPass(root, &ui->hot, &ui->active);
        UI_LayoutPassComputeFinalRects(root);
        
        ui->draw_list.first = ui->draw_list.last = 0;
        UI_RenderPass(root, ui->root.rect, ui->window, &ui->draw_list);
        UI_RenderKeyboardFocusHighlight(root, ui->root.rect, ui->window, &ui->draw_list);
        DRW_ListSubmit(ui->window, &ui->draw_list);
    }
    
    if(!G_WindowKeyStateGet(ui->window, G_Key_MouseButtonLeft))
    {
        UI_Node *active = UI_NodeLookup(&ui->active);
        if(0 != active)
        {
            if(UI_Flags_CursorPositionToCenter == (active->flags & UI_Flags_CursorPositionToCenter))
            {
                G_WindowMousePositionSet(ui->window, Centre2F(active->rect));
            }
            else if(active->flags & UI_Flags_CursorPositionToCenterX)
            {
                G_WindowMousePositionSet(ui->window, V2F(Centre2F(active->rect).x, G_WindowMousePositionGet(ui->window).y));
            }
            else if(active->flags & UI_Flags_CursorPositionToCenterY)
            {
                G_WindowMousePositionSet(ui->window, V2F(G_WindowMousePositionGet(ui->window).x, Centre2F(active->rect).y));
            }
            
            UI_Key selected_interface_key = UI_NavInterfaceStackPeek();
            UI_Node *selected_interface = UI_NodeLookup(&selected_interface_key);
            while(0 != selected_interface && !UI_NodeIsChildOf(active, &selected_interface_key) && !selected_interface->is_new)
            {
                selected_interface_key = UI_NavInterfaceStackPop();
                selected_interface = UI_NodeLookup(&selected_interface_key);
            }
        }
        
        ui->last_initial_mouse_position= V2F(0.0f, 0.0f);
        ui->last_initial_mouse_relative_position = V2F(0.0f, 0.0f);
        ui->active = UI_KeyNil();
    }
    else
    {
        ui->was_last_interaction_keyboard = False;
    }
    
    UI_Key selected_interface_key = UI_NavInterfaceStackPeek();
    while(!UI_KeyIsNil(&selected_interface_key) && 0 == UI_NodeLookup(&selected_interface_key))
    {
        selected_interface_key = UI_NavInterfaceStackPop();
    }
    if(UI_KeyIsNil(&selected_interface_key))
    {
        G_EventQueue *events = G_WindowEventQueueGet(ui->window);
        if(G_EventQueueHasKeyDown(events, G_Key_Tab, 0, True))
        {
            UI_NavDefaultNext(&ui->root);
        }
        else if(G_EventQueueHasKeyDown(events, G_Key_Tab, G_ModifierKeys_Shift, True))
        {
            UI_NavDefaultPrev(&ui->root);
        }
    }
    
    if(ui->next_cursor_kind != ui->cursor_kind)
    {
        G_WindowCursorKindSet(ui->window, ui->next_cursor_kind);
        ui->cursor_kind = ui->next_cursor_kind;
    }
    
    for(U64 bucket_index = 0; bucket_index < ArrayCount(ui->buckets); bucket_index += 1)
    {
        UI_Node *bucket = &ui->buckets[bucket_index];
        if(!UI_KeyIsNil(&bucket->key))
        {   
            UI_Node *next = 0;
            UI_Node **prev_indirect = &bucket->next_hash;
            for(UI_Node *node = *prev_indirect; 0 != node; node = next)
            {
                if(ui->frame_index > node->last_touched_frame_index + 2)
                {
                    *prev_indirect = next;
                    MemorySet(node, 0, sizeof(*node));
                    node->next = ui->free_list;
                    ui->free_list = node;
                }
            }
            
            if(ui->frame_index > bucket->last_touched_frame_index + 2)
            {
                if(0 == bucket->next_hash)
                {
                    MemorySet(bucket, 0, sizeof(*bucket));
                }
                else
                {
                    *bucket = *bucket->next_hash;
                }
            }
        }
    }
    
    for(U64 bucket_index = 0; bucket_index < ArrayCount(ui->pt_cache.buckets); bucket_index += 1)
    {
        UI_PreparedTextCacheNode *next = 0;
        UI_PreparedTextCacheNode **prev_indirect = &ui->pt_cache.buckets[bucket_index];
        for(UI_PreparedTextCacheNode *node = *prev_indirect; 0 != node; node = next)
        {
            if(ui->frame_index > node->last_touched_frame_index + 2)
            {
                *prev_indirect = next;
                FONT_PreparedTextRelease(&ui->pt_free_list, &node->pt);
                node->next = ui->pt_cache.free_list;
                ui->pt_cache.free_list = node;
            }
        }
    }
    
    ui = 0;
}

//~NOTE(tbt): buider helpers

Function void
UI_Spacer(UI_Size size)
{
    UI_Node *parent = UI_ParentPeek();
    UI_Node *node = PushArray(G_WindowFrameArenaGet(ui->window), UI_Node, 1);
    node->size[parent->children_layout_axis] = size;
    node->texture = R2D_TextureNil();
    node->nav_context = UI_NavContext(&UI_NavDefault, &ui->root);
#define UI_ControlStackDef(NAME_L, NAME_U, TYPE, DEFAULT) \
if(ui->Glue(NAME_L, _set_next_flag)) \
{ \
Glue(UI_, Glue(NAME_U, Pop)) ();\
}
    UI_ControlStackList
#undef UI_ControlStackDef
    UI_NodeInsert(node, parent);
}

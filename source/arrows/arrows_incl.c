
Function Board *
PushBoard(Arena *arena, Boards *boards)
{
    Board *board = PushArray(arena, Board, 1);
    board->prev = boards->last;
    if(0 == boards->last)
    {
        boards->first = board;
    }
    else
    {
        boards->last->next = board;
    }
    boards->last = board;
    board->actions_up_to = ~((U64)0);
    return board;
}

Function V2F
WorldSpaceFromScreenSpace(V2F screen_space_position, OpaqueHandle window, V2F camera_position, F32 zoom)
{
    F32 s = G_WindowScaleFactorGet(window);
    V2F result = Add2F(Scale2F(screen_space_position, 1.0f / (zoom*s)), camera_position);
    return result;
}

Function Board *
BoardFromWorldSpacePosition(V2F world_space_position, Boards boards)
{
    Board *result = &nil_board;
    for(Board *board = boards.last; 0 != board; board = board->prev)
    {
        Range2F rect = RectMake2F(board->position, board_size);
        if(HasPoint2F(rect, world_space_position))
        {
            result = board;
        }
    }
    return result;
}

Function V2F
BoardSpaceFromWorldSpace(V2F world_space_position, Board *board)
{
    V2F result = Clamp2F(Sub2F(world_space_position, board->position), V2F(0.0f, 0.0f), board_size);
    return result;
}

Function V2F
ScreenSpaceFromBoardSpace(V2F board_space_position, Board *board, OpaqueHandle window, V2F camera_position, F32 zoom)
{
    F32 s = G_WindowScaleFactorGet(window);
    V2F world_space_position = Add2F(board_space_position, board->position);
    V2F result = Scale2F(Sub2F(world_space_position, camera_position), zoom*s);
    return result;
}

Function void
OpsClear(Ops *ops)
{
    ArenaClear(ops->arena);
    ops->count = 0;
    ops->ops = 0;
}

Function void
OpsAppend(Ops *ops, Op op)
{
    if(OpKind_NONE != op.kind)
    {
        Op *o = PushArray(ops->arena, Op, 1);
        if(0 == ops->ops)
        {
            ops->ops = o;
        }
        MemoryCopy(o, &op, sizeof(op));
        ops->count += 1;
    }
}

Function Op
OpsPop(Ops *ops)
{
    Op result = { OpKind_NONE, };
    if(ops->count > 0)
    {
        result = ops->ops[ops->count - 1];
        ArenaPop(ops->arena, sizeof(Op));
        ops->count -= 1;
    }
    return result;
}

Function TagMetadata *
TagMetadataFromId(Arena *arena, Tags *tags, U64 id)
{
    TagMetadata *result = 0;
    for(TagMetadata *tag = tags->first; 0 != tag; tag = tag->next)
    {
        if(tag->id == id)
        {
            result = tag;
            break;
        }
    }
    if(0 == result)
    {
        result = PushArray(arena, TagMetadata, 1);
        result->next = tags->first;
        tags->first = result;
        result->id = id;
        result->bounds.min = board_size;
        result->bounds.max = U2F(0.0f);
        result->name = PushS8Builder(arena, 128);
        S8BuilderAppendFmt(&result->name, "Tag %llu", id);
    }
    return result;
}

Function InteractionFlags
InteractionFlagsFromKeyAndModifiers(G_Key key, G_ModifierKeys modifiers)
{
    InteractionFlags result = 0;
    for(U64 entry_index = 0; entry_index < ArrayCount(interaction_table); entry_index += 1)
    {
        InteractionTableEntry *entry = &interaction_table[entry_index];
        if(entry->key == key && (0 == entry->modifiers || entry->modifiers == modifiers))
        {
            result = entry->flags;
            break;
        }
    }
    return result;
}

Function void
WindowHookOpen(OpaqueHandle window)
{
    AppState *app = G_WindowUserDataGet(window);
    Arena *permanent_arena = OS_TCtxGet()->permanent_arena;
    app->segments = LINE_PushSegmentList(permanent_arena, LINE_SegmentListCap);
    app->quads = R2D_PushQuadList(permanent_arena, R2D_QuadListCap);
    app->target_zoom = app->zoom = 1.0f;
    Board *default_board = PushBoard(permanent_arena, &app->boards);
    default_board->actions.arena = ArenaAlloc(Gigabytes(1));
    default_board->effects.arena = ArenaAlloc(Gigabytes(1));
    app->active_board = &nil_board;
    app->ui = UI_Init(window);
    G_WindowClearColourSet(window, ColFromHex(0xd8dee9ff));
}

F32
UnevenCapsuleSDF(V2F sample_position,
                 V2F a_position,
                 V2F b_position,
                 F32 a_radius,
                 F32 b_radius)
{
    V2F p = Sub2F(sample_position,  a_position);
    V2F pb = Sub2F(b_position, a_position);
    F32 h = Dot2F(pb, pb);
    
    V2F q = Scale2F(V2F(Abs1F(Dot2F(p, V2F(pb.y, -pb.x))), Dot2F(p, pb)), 1.0f / h);
    
    F32 b = a_radius - b_radius;
    V2F c = V2F(Sqrt1F(h - b*b), b);
    
    F32 k = c.x*q.y - c.y*q.x;
    F32 m = Dot2F(c, q);
    F32 n = Dot2F(q, q);
    
    if     (k < 0.0) return Sqrt1F(h*(n)) - a_radius;
    else if(k > c.x) return Sqrt1F(h*(n + 1.0 - 2.0*q.y)) - b_radius;
    else             return m - a_radius;
}

Function void
WindowHookDraw(OpaqueHandle window)
{
    AppState *app = G_WindowUserDataGet(window);
    
    ArenaTemp scratch = OS_TCtxScratchBegin(0, 0);
    Arena *permanent_arena = OS_TCtxGet()->permanent_arena;
    
    G_EventQueue *events = G_WindowEventQueueGet(window);
    Range2F client_rect = G_WindowClientRectGet(window);
    V2F mouse_position = G_WindowMousePositionGet(window);
    F32 s = G_WindowScaleFactorGet(window);
    
    // TODO(tbt): some kind of 'brush system'
    ReadOnlyVar Persist V4F brush_colour = ColFromHexInitialiser(0x2e3440ff);
    ReadOnlyVar Persist F32 brush_width = 4.0f;
    ReadOnlyVar Persist F32 eraser_width = 16.0f;
    
    //-NOTE(tbt): animate (state*target state -> state')
    UI_Animate1F(&app->zoom, app->target_zoom, 15.0f);
    UI_AnimateF(app->camera_position.elements, app->target_camera_position.elements, 2, 20.0f);
    
    //-NOTE(tbt): ui build (events*state -> ui tree*state')
    UI_Begin(app->ui);
    UI_Width(UI_SizeNone()) UI_Height(UI_SizeNone())
    {
        for(Board *board = app->boards.first; 0 != board; board = board->next)
        {
            Range2F board_rect = Range2F(ScreenSpaceFromBoardSpace(U2F(0.0f), board, window, app->camera_position, app->zoom),
                                         ScreenSpaceFromBoardSpace(board_size, board, window, app->camera_position, app->zoom));
            Range2F actions_rect = Range2F(V2F(board_rect.max.x,            board_rect.min.y),
                                           V2F(board_rect.max.x + 256.0f*s, board_rect.max.y));
            board->actions_up_to = ~((U64)0);
            if(IsOverlapping2F(actions_rect, client_rect))
            {
                UI_SetNextFixedRect(actions_rect);
                UI_SetNextChildrenLayout(Axis2_Y);
                UI_Node *actions_panel =
                    UI_NodeMakeFromFmt(UI_Flags_Scrollable |
                                       UI_Flags_ScrollChildrenY |
                                       UI_Flags_OverflowY |
                                       UI_Flags_ClampViewOffsetToOverflow |
                                       UI_Flags_AnimateSmoothScroll |
                                       UI_Flags_FixedFinalRect,
                                       "actions panel for board %p", board);
                U64 run_count = 0;
                UI_Parent(actions_panel) UI_Width(UI_SizeFromText(8.0f, 1.0f)) UI_Height(UI_SizePx(32.0f, 1.0f))
                    for(U64 action_index = 0; action_index < board->actions.count; action_index += 1)
                {
                    run_count += 1;
                    Op *action = &board->actions.ops[action_index];
                    Op *next = (action_index + 1< board->actions.count) ? &board->actions.ops[action_index + 1] : 0;
                    if((0 == next || next->kind != action->kind) && action->kind != OpKind_Begin)
                    {
                        S8 label = (action->kind == OpKind_Stroke ? S8("Stroke") :
                                    action->kind == OpKind_Erase  ? S8("Erase") :
                                    S8(""));
                        UI_SetNextHotFgCol(ColFromHex(0x81a1c1ff));
                        UI_SetNextActiveFgCol(ColFromHex(0x88c0d0ff));
                        UI_SetNextTextTruncSuffix(S8(""));
                        UI_Node *button = UI_NodeMakeFromFmt(UI_Flags_DrawText |
                                                             UI_Flags_DrawHotStyle |
                                                             UI_Flags_DrawActiveStyle |
                                                             UI_Flags_Pressable |
                                                             UI_Flags_AnimateHot |
                                                             UI_Flags_AnimateActive,
                                                             "%.*s (%llu)##%p", FmtS8(label), run_count, action);
                        UI_Animate1F(&button->user_t, action_index >= board->actions_up_to, 15.0f);
                        button->fg = Scale4F(button->fg, InterpolateLinear1F(1.0f, 0.25f, button->user_t));
                        UI_Use use = UI_UseFromNode(button);
                        if(use.is_hovering)
                        {
                            board->actions_up_to = action_index + 1;
                        }
                        run_count = 0;
                    }
                }
                UI_UseFromNode(actions_panel);
            }
        }
    }
    UI_Column()
    {
        UI_Spacer(UI_SizeFill());
        UI_Height(UI_SizePx(40.0f, 1.0f)) UI_Row() UI_Pad(UI_SizeFill()) UI_Width(UI_SizeFromText(4.0f, 1.0f))
            for(U64 interaction_index = 0; interaction_index < ArrayCount(interaction_table); interaction_index += 1)
        {
            InteractionTableEntry *entry = &interaction_table[interaction_index];
            if(entry->label.len > 0)
                UI_Pad(UI_SizePx(4.0f, 1.0f))
            {
                S8 key_string = G_S8FromKey(entry->key);
                if((app->interaction_flags & entry->flags) == entry->flags &&
                   G_WindowKeyStateGet(window, entry->key))
                {
                    UI_SetNextFgCol(ColFromHex(0x5e81acff));
                }
                UI_SetNextFont(ui_font_bold);
                UI_LabelFromFmt("%.*s: ", FmtS8(key_string));
                UI_Label(entry->label);
            }
        }
    }
    
    //-NOTE(tbt): input (events*actions -> actions')
    for(G_EventQueueForEach(events, e))
    {
        F32 pressure = Max1F(e->pressure, 0.125f);
        V2F world_space_position = WorldSpaceFromScreenSpace(e->position, window, app->camera_position, app->zoom);
        Board *board = BoardFromWorldSpacePosition(world_space_position, app->boards);
        if(G_EventKind_Key == e->kind && !e->is_repeat)
        {
            InteractionFlags flags = InteractionFlagsFromKeyAndModifiers(e->key, e->modifiers);
            SetMask(app->interaction_flags, flags, e->is_down);
            if(0 != flags)
            {
                G_EventConsume(e);
            }
            if(flags & InteractionFlags_PenDown)
            {
                if(e->is_down)
                {
                    app->active_board = board;
                    if(!(app->interaction_flags & InteractionFlags_DoNotDraw))
                    {
                        V2F board_space_position = BoardSpaceFromWorldSpace(world_space_position, board);
                        OpsAppend(&board->actions, Op(OpKind_Begin, 255*pressure, board_space_position.x, board_space_position.y));
                    }
                }
                else
                {
                    app->active_board = &nil_board;
                }
            }
        }
        else if(G_EventKind_MouseMove == e->kind && (app->interaction_flags & InteractionFlags_PenDown))
        {
            if(app->interaction_flags & InteractionFlags_Panning)
            {
                app->target_camera_position = Sub2F(app->target_camera_position, Scale2F(e->delta, 1.0f / (s*app->zoom)));
                G_EventConsume(e);
            }
            else if(!(app->interaction_flags & InteractionFlags_DoNotDraw))
            {
                OpKind kind = (app->interaction_flags & InteractionFlags_Erasing) ? OpKind_Erase : OpKind_Stroke;
                V2F board_space_position = BoardSpaceFromWorldSpace(world_space_position, app->active_board);
                B32 should_filter = False;
                if(app->active_board->actions.count > 1 &&
                   kind == app->active_board->actions.ops[app->active_board->actions.count - 1].kind)
                {
                    V2F a = V2F(app->active_board->actions.ops[app->active_board->actions.count - 1].x,
                                app->active_board->actions.ops[app->active_board->actions.count - 1].y);
                    V2F b = board_space_position;
                    F32 dist_squared = LengthSquared2F(Sub2F(b, a));
                    F32 min_dist = 2.0f;
                    if(dist_squared < min_dist*min_dist)
                    {
                        should_filter = True;
                    }
                }
                if(!should_filter)
                {
                    OpsAppend(&app->active_board->actions, Op(kind, 255*pressure, board_space_position.x, board_space_position.y));
                }
                G_EventConsume(e);
            }
        }
        else if(G_EventKind_MouseScroll == e->kind)
        {
            F32 zoom_delta = 0.01f*app->target_zoom*e->delta.y;
            F32 old_zoom = app->target_zoom;
            app->target_zoom += zoom_delta;
            // NOTE(tbt): keep same point under mouse cursor when zooming
            V2F old_mouse_world_position = WorldSpaceFromScreenSpace(mouse_position, window, app->target_camera_position, old_zoom);
            V2F mouse_world_position = WorldSpaceFromScreenSpace(mouse_position, window, app->target_camera_position, app->target_zoom);
            V2F camera_position_delta = Sub2F(old_mouse_world_position, mouse_world_position);
            app->target_camera_position = Add2F(app->target_camera_position, camera_position_delta);
            G_EventConsume(e);
        }
    }
    
    for(Board *board = app->boards.first; 0 != board; board = board->next)
    {
        //-NOTE(tbt): normalise (actions -> effects)
        U64 actions_count = Min1U64(board->actions.count, board->actions_up_to);
        if(actions_count < board->actions_applied_count)
        {
            OpsClear(&board->effects);
            board->actions_applied_count = 0;
        }
        for(U64 action_index = board->actions_applied_count; action_index < actions_count; action_index += 1)
        {
            Op *action = &board->actions.ops[action_index];
            if(OpKind_Begin == action->kind || OpKind_Stroke == action->kind)
            {
                Op to_append = *action;
                OpsAppend(&board->effects, to_append);
            }
            else if(OpKind_Erase == action->kind)
            {
                Op *prev_action = &board->actions.ops[action_index - 1];
                B32 should_start_new_stroke = False;
                for(U64 effect_index = 0; effect_index < board->effects.count; effect_index += 1)
                {
                    Op *effect = &board->effects.ops[effect_index];
                    if(should_start_new_stroke)
                    {
                        effect->kind = OpKind_Begin;
                        should_start_new_stroke = False;
                    }
                    V2F sample_position = V2F(effect->x, effect->y);
                    V2F a = V2F(prev_action->x, prev_action->y);
                    V2F b = V2F(action->x, action->y);
                    F32 dist = UnevenCapsuleSDF(sample_position, a, b, 0.5f*eraser_width*prev_action->pressure / 255.0f, 0.5f*eraser_width*action->pressure / 255.0f);
                    if(dist < 0.5f*brush_width*effect->pressure / 255.0f)
                    {
                        should_start_new_stroke = True;
                        MemoryMove(effect, &board->effects.ops[effect_index + 1], (board->effects.count - effect_index - 1)*sizeof(Op));
                        board->effects.count -= 1;
                        effect_index -= 1;
                        ArenaPop(board->effects.arena, sizeof(Op));
                    }
                }
            }
            board->actions_applied_count += 1;
        }
        
        //-NOTE(tbt): render (effects -> pixels)
        Range2F board_rect = Range2F(ScreenSpaceFromBoardSpace(U2F(0.0f), board, window, app->camera_position, app->zoom),
                                     ScreenSpaceFromBoardSpace(board_size, board, window, app->camera_position, app->zoom));
        R2D_QuadListClear(&app->quads);
        LINE_SegmentListClear(&app->segments);
        R2D_Quad board_shadow_quad = 
        {
            .dst = Grow2F(board_rect, U2F(16.0f)),
            .mask = client_rect,
            .fill_colour = Col(0.0f, 0.0f, 0.0f, 0.125f),
            .corner_radius = 24.0f,
            .edge_softness = 8.0f,
        };
        R2D_QuadListAppend(&app->quads, board_shadow_quad);
        R2D_Quad board_quad = 
        {
            .dst = board_rect,
            .mask = client_rect,
            .fill_colour = ColFromHex(0xeceff4ff),
            .corner_radius = 16.0f,
            .edge_softness = 1.0f,
        };
        R2D_QuadListAppend(&app->quads, board_quad);
        R2D_QuadListSubmit(window, app->quads, R2D_TextureNil());
        for(U64 effect_index = 0; effect_index < board->effects.count; effect_index += 1)
        {
            // NOTE(tbt): normalised ops should only consist of Begin and Stroke ops
            Op *effect = &board->effects.ops[effect_index];
            if(effect->kind == OpKind_Stroke && effect_index > 0)
            {
                Op *prev = &board->effects.ops[effect_index - 1];
                V2F p = V2F(prev->x, prev->y);
                V2F q = V2F(effect->x, effect->y);
                LINE_Segment segment =
                {
                    .a =
                    {
                        .pos = ScreenSpaceFromBoardSpace(p, board, window, app->camera_position, app->zoom),
                        .col = brush_colour,
                        .width = (prev->pressure / 255.0f)*brush_width*s*app->zoom,
                    },
                    .b =
                    {
                        .pos = ScreenSpaceFromBoardSpace(q, board, window, app->camera_position, app->zoom),
                        .col = brush_colour,
                        .width = (effect->pressure / 255.0f)*brush_width*s*app->zoom,
                    },
                };
                while(!LINE_SegmentListAppend(&app->segments, segment))
                {
                    LINE_SegmentListSubmit(window, app->segments, client_rect);
                    LINE_SegmentListClear(&app->segments);
                }
            }
        }
        LINE_SegmentListSubmit(window, app->segments, client_rect);
        // TODO(tbt): this basically relies on there only being one board - will need to think this through properly later
        if(app->interaction_flags & InteractionFlags_Translating)
        {
            R2D_QuadListClear(&app->quads);
            for(TagMetadata *tag = app->tags.first; 0 != tag; tag = tag->next)
            {
                R2D_Quad tag_highlight_quad =
                {
                    .dst = Range2F(ScreenSpaceFromBoardSpace(tag->bounds.min, board, window, app->camera_position, app->zoom),
                                   ScreenSpaceFromBoardSpace(tag->bounds.max, board, window, app->camera_position, app->zoom)),
                    .mask = client_rect,
                    .fill_colour = ColFromHex(0x88c0d088),
                    .stroke_colour = ColFromHex(0x88c0d0ff),
                    .stroke_width = 4.0f,
                    .corner_radius = 8.0f,
                    .edge_softness = 1.0f,
                };
                while(!R2D_QuadListAppend(&app->quads, tag_highlight_quad))
                {
                    R2D_QuadListSubmit(window, app->quads, R2D_TextureNil());
                    R2D_QuadListClear(&app->quads);
                }
            }
            R2D_QuadListSubmit(window, app->quads, R2D_TextureNil());
        }
    }
    if(app->interaction_flags & InteractionFlags_Erasing)
    {
        V2F centre = mouse_position;
        V2F half_span = U2F(0.5f*eraser_width*s);
        R2D_QuadListClear(&app->quads);
        R2D_Quad eraser_quad = 
        {
            .dst = Range2F(Sub2F(centre, half_span),
                           Add2F(centre, half_span)),
            .mask = client_rect,
            .stroke_colour = ColFromHex(0x2e3440ff),
            .stroke_width = 4.0f,
            .corner_radius = half_span.x,
            .edge_softness = 1.0f,
        };
        R2D_QuadListAppend(&app->quads, eraser_quad);
        R2D_QuadListSubmit(window, app->quads, R2D_TextureNil());
    }
    
    //-NOTE(tbt): update tag bounds (effects*tag metadata -> tag metadata')
    // TODO(tbt): this basically relies on there only being one board - will need to think this through properly later
    for(TagMetadata *tag = app->tags.first; 0 != tag; tag = tag->next)
    {
        tag->bounds.min = board_size;
        tag->bounds.max = U2F(0.0f);
    }
    for(Board *board = app->boards.first; 0 != board; board = board->next)
    {
        for(U64 effect_index = 0; effect_index < board->effects.count; effect_index += 1)
        {
            Op *effect = &board->effects.ops[effect_index];
            /*
            TagMetadata *tag = TagMetadataFromId(permanent_arena, &app->tags, effect->tag);
            tag->bounds.min = Mins2F(tag->bounds.min, effect->point);
            tag->bounds.max = Maxs2F(tag->bounds.max, effect->point);
*/
        }
    }
    
    //-NOTE(tbt): render UI chrome (ui tree -> pixels)
    UI_End();
    
    ArenaTempEnd(scratch);
}

Function void
WindowHookClose(OpaqueHandle window)
{
    AppState *app = G_WindowUserDataGet(window);
}

EntryPoint
{
    OS_Init();
    G_Init();
    R2D_Init();
    LINE_Init();
    FONT_Init();
    
    AppState app_state = {0};
    
    G_WindowHooks window_hooks ={
        WindowHookOpen,
        WindowHookDraw,
        WindowHookClose,
    };
    G_WindowOpen(S8("Arrows"), V2I(1024, 768), window_hooks, &app_state);
    
    G_MainLoop();
    
    LINE_Cleanup();
    R2D_Cleanup();
    G_Cleanup();
    OS_Cleanup();
}

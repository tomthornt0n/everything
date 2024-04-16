
Function DRW_Backend
DRW_BackendFromCmdKind (DRW_CmdKind kind)
{
    switch(kind)
    {
        case(DRW_CmdKind_Sprite): return DRW_Backend_Renderer2D;
        case(DRW_CmdKind_Text): return DRW_Backend_Renderer2D;
        case(DRW_CmdKind_LineSegment): return DRW_Backend_RendererLine;
    };
    return DRW_Backend_MAX + 1;
}

Function R2D_Quad
DRW_Renderer2DQuadFromCmd(DRW_Cmd *cmd)
{
    R2D_Quad result =
    {
        .dst.min = cmd->position_1,
        .dst.max = cmd->position_2,
        .src = cmd->src,
        .mask = cmd->mask,
        .fill_colour = cmd->colour_1,
        .stroke_colour = cmd->colour_2,
        .stroke_width = cmd->width_1,
        .corner_radius = cmd->width_2,
        .edge_softness = cmd->width_3,
    };
    return result;
}

Function LINE_Segment
DRW_RendererLineSegmentFromCmd(DRW_Cmd *cmd)
{
    LINE_Segment result =
    {
        .a =
        {
            .pos = cmd->position_1,
            .col = cmd->colour_1,
            .width = cmd->width_1,
        },
        .b =
        {
            .pos = cmd->position_2,
            .col = cmd->colour_2,
            .width = cmd->width_2,
        },
    };
    return result;
}

Function void
DRW_Sprite(Arena *arena, DRW_List *list, R2D_Quad quad, OpaqueHandle texture)
{
    DRW_Cmd *cmd = PushArray(arena, DRW_Cmd, 1);
    if(0 == list->last)
    {
        list->first = cmd;
    }
    else
    {
        list->last->next = cmd;
    }
    list->last = cmd;
    cmd->kind = DRW_CmdKind_Sprite;
    cmd->texture = texture;
    cmd->position_1 = quad.dst.min;
    cmd->position_2 = quad.dst.max;
    cmd->src = quad.src;
    cmd->mask = quad.mask;
    cmd->colour_1 = quad.fill_colour;
    cmd->colour_2 = quad.stroke_colour;
    cmd->width_1 = quad.stroke_width;
    cmd->width_2 = quad.corner_radius;
    cmd->width_3 = quad.edge_softness;
    if(R2D_TextureIsNil(texture))
    {
        cmd->colour_1.a = -cmd->colour_1.a;
    }
}

Function void
DRW_LineSegment(Arena *arena, DRW_List *list, LINE_Segment segment, Range2F mask)
{
    DRW_Cmd *cmd = PushArray(arena, DRW_Cmd, 1);
    if(0 == list->last)
    {
        list->first = cmd;
    }
    else
    {
        list->last->next = cmd;
    }
    list->last = cmd;
    cmd->kind = DRW_CmdKind_LineSegment;
    cmd->position_1 = segment.a.pos;
    cmd->position_2 = segment.b.pos;
    cmd->colour_1 = segment.a.col;
    cmd->colour_2 = segment.b.col;
    cmd->width_1 = segment.a.width;
    cmd->width_2 = segment.b.width;
    cmd->mask = mask;
}

Function void
DRW_Text(Arena *arena, DRW_List *list, FONT_PreparedText *text, V2F position, V4F colour, Range2F mask)
{
    DRW_Cmd *cmd = PushArray(arena, DRW_Cmd, 1);
    if(0 == list->last)
    {
        list->first = cmd;
    }
    else
    {
        list->last->next = cmd;
    }
    list->last = cmd;
    cmd->kind = DRW_CmdKind_Text;
    cmd->pt = text;
    cmd->position_1 = position;
    cmd->colour_1 = colour;
    cmd->mask = mask;
}

Function void
DRW_ListSubmit(OpaqueHandle window, DRW_List *list)
{
    ArenaTemp scratch = OS_TCtxScratchBegin(0, 0);
    
    DRW_Backend prev_backend = DRW_Backend_None;
    Range2F prev_mask = Range2F(NaN);
    OpaqueHandle texture = R2D_TextureNil();
    R2D_QuadList quads = R2D_PushQuadList(scratch.arena, R2D_QuadListCap);
    LINE_SegmentList lines = LINE_PushSegmentList(scratch.arena, LINE_SegmentListCap);
    
    OpaqueHandle font_cache_atlas = FONT_Renderer2DTextureFromCache(font_cache);
    
    for(DRW_Cmd *cmd = list->first; 0 != cmd; cmd = cmd->next)
    {
        if(DRW_CmdKind_Text == cmd->kind)
        {
            cmd->texture = font_cache_atlas;
        }
        
        DRW_Backend backend = DRW_BackendFromCmdKind(cmd->kind);
        
        B32 needs_to_flush_batch = False;
        if(backend != prev_backend && DRW_Backend_None != prev_backend) { needs_to_flush_batch = True; }
        if(!(OpaqueHandleMatch(cmd->texture, texture) || R2D_TextureIsNil(texture) || R2D_TextureIsNil(cmd->texture))) { needs_to_flush_batch = True; }
        if(DRW_Backend_Renderer2D == backend)
        {
            if(quads.count >= quads.cap) { needs_to_flush_batch = True; }
        }
        if(DRW_Backend_RendererLine == backend)
        {
            if(lines.count >= lines.cap) { needs_to_flush_batch = True; }
            if(!IsNaN1F(prev_mask.x0))
            {
                if(cmd->mask.min.x != prev_mask.min.x) { needs_to_flush_batch = True; }
                if(cmd->mask.min.y != prev_mask.min.y) { needs_to_flush_batch = True; }
                if(cmd->mask.max.x != prev_mask.max.x) { needs_to_flush_batch = True; }
                if(cmd->mask.max.y != prev_mask.max.y) { needs_to_flush_batch = True; }
            }
        }
        
        if(needs_to_flush_batch)
        {
            if(quads.count > 0)
            {
                R2D_QuadListSubmit(window, quads, texture);
                R2D_QuadListClear(&quads);
            }
            if(lines.count > 0)
            {
                LINE_SegmentListSubmit(window, lines, cmd->mask);
                R2D_QuadListClear(&quads);
            }
        }
        
        if(!R2D_TextureIsNil(cmd->texture))
        {
            texture = cmd->texture;
        }
        
        prev_backend = backend;
        prev_mask = cmd->mask;
        
        if(DRW_CmdKind_Sprite == cmd->kind)
        {
            R2D_QuadListAppend(&quads, DRW_Renderer2DQuadFromCmd(cmd));
        }
        else if(DRW_CmdKind_Text == cmd->kind)
        {
            FONT_Renderer2DQuadsFromPreparedText(&quads, cmd->position_1, cmd->colour_1, cmd->pt, cmd->mask);
        }
        else if(DRW_CmdKind_LineSegment == cmd->kind)
        {
            LINE_SegmentListAppend(&lines, DRW_RendererLineSegmentFromCmd(cmd));
        }
    }
    
    R2D_QuadListSubmit(window, quads, texture);
    
    ArenaTempEnd(scratch);
}

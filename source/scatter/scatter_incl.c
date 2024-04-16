
Function void
SCTR_WindowHookOpen(OpaqueHandle window)
{
    Arena *arena = G_WindowArenaGet(window);
    SCTR_Dict *dict = ArenaPush(arena, sizeof(*dict));
    dict->quads = R2D_QuadListFromArena(ArenaMake());
    dict->line_batch = LINE_BatchFromArena(ArenaMake());
    dict->ui_state = UI_Init(window);
    G_WindowUserDataSet(window, dict);
}

Function V4F
SCTR_ColourFromSeriesName(S8 name)
{
    
    
    
}

Function SCTR_PanelPerSeriesData *
SCTR_PanelPerSeriesDataFromName (SCTR_Panel *panel, S8 series_name)
{
    SCTR_PanelPerSeriesData *result = 0;
    
    for(SCTR_PanelPerSeriesData *psd = panel->series; 0 != psd; psd = psd->next)
    {
        if(S8Match(psd->series_name, series_name, 0))
        {
            result = psd;
            break;
        }
    }
    
    return result;
}

Function void
SCTR_SeriesChecklistBuildUI(OpaqueHandle window, Arena *arena, SCTR_Panel *panel, SCTR_Series *series_list)
{
    UI_Column()
    {
        for(SCTR_Series *series = series_list; 0 != series; series = series->next)
        {
            SCTR_PanelPerSeriesData *psd = SCTR_PanelPerSeriesDataFromName(panel, series->name);
            
            B32 is_showing = (0 != psd) && psd->is_showing;
            
            UI_Height(UI_SizePx(32.0f, 1.0f)) UI_ChildrenLayout(Axis2_X)
            {
                UI_Node *row = UI_NodeMakeFromFmt(UI_Flags_Pressable |
                                                  UI_Flags_AnimateHot |
                                                  UI_Flags_AnimateActive |
                                                  UI_Flags_AnimateSmoothLayout,
                                                  "series checklist row %p %p",
                                                  panel, series);
                
                UI_Parent(row) UI_DefaultFlags(UI_Flags_AnimateInheritHot | UI_Flags_AnimateInheritActive)
                {
                    UI_Width(UI_SizePx(24.0f, 1.0f)) UI_LabelFromIcon(is_showing ? FONT_Icon_Check : FONT_Icon_CheckEmpty);
                    UI_Width(UI_SizeFromText(8.0f, 1.0f)) UI_Label(series->name);
                }
                
                if(UI_UseFromNode(row).is_pressed)
                {
                    if(0 == psd)
                    {
                        U64 hash[2] = {0};
                        MurmurHash3_x64_128(series->name.buffer, series->name.len, 0, hash);
                        U64 col_index = (hash[0]*11) % ArrayCount(sctr_palette);
                        
                        psd = ArenaPush(arena, sizeof(*psd));
                        psd->series_name = series->name;
                        psd->is_showing = True;
                        psd->col_index = col_index;
                        psd->next = panel->series;
                        panel->series = psd;
                    }
                    else
                    {
                        psd->is_showing = !is_showing;
                    }
                }
                
                if(is_showing)
                {
                    UI_Width(UI_SizePx(168.0f, 1.0f)) UI_Row()
                    {
                        UI_Spacer(UI_SizePx(24.0f, 1.0f));
                        
                        V4F col = sctr_palette[psd->col_index];
                        
                        UI_Width(UI_SizePx(32.0f, 1.0f)) UI_CornerRadius(16.0f) UI_BgCol(col) UI_HotGrowth(4.0f) UI_ActiveGrowth(2.0f)
                        {
                            UI_Node *plot_col_button = UI_NodeMakeFromFmt(UI_Flags_DrawFill |
                                                                          UI_Flags_DrawDropShadow |
                                                                          UI_Flags_DrawHotStyle |
                                                                          UI_Flags_DrawActiveStyle |
                                                                          UI_Flags_Pressable |
                                                                          UI_Flags_AnimateHot |
                                                                          UI_Flags_AnimateActive |
                                                                          UI_Flags_AnimateSmoothLayout,
                                                                          "%s###plot colour %p",
                                                                          psd->is_connected ? "line" : "scatter",
                                                                          psd);
                            if(UI_UseFromNode(plot_col_button).is_pressed)
                            {
                                psd->col_index = (psd->col_index + 1) % ArrayCount(sctr_palette);
                            }
                        }
                        
                        UI_Spacer(UI_SizePx(16.0f, 1.0f));
                        
                        UI_Width(UI_SizePx(96.0f, 1.0f))
                        {
                            UI_Node *plot_kind_button = UI_NodeMakeFromFmt(UI_Flags_DrawFill |
                                                                           UI_Flags_DrawText |
                                                                           UI_Flags_DrawDropShadow |
                                                                           UI_Flags_DrawHotStyle |
                                                                           UI_Flags_DrawActiveStyle |
                                                                           UI_Flags_Pressable |
                                                                           UI_Flags_AnimateHot |
                                                                           UI_Flags_AnimateActive |
                                                                           UI_Flags_AnimateSmoothLayout,
                                                                           "%s###plot kind %p",
                                                                           psd->is_connected ? "line" : "scatter",
                                                                           psd);
                            if(UI_UseFromNode(plot_kind_button).is_pressed)
                            {
                                psd->is_connected = !psd->is_connected;
                            }
                        }
                    }
                    
                    UI_Width(UI_SizePx(168.0f, 1.0f)) UI_Row()
                    {
                        SCTR_Series *series = SCTR_SeriesFromName(window, psd->series_name);
                        if(0 != series->last_plot)
                        {
                            UI_LabelFromFmt("(%.1f, %.1f)", series->last_plot->x, series->last_plot->y);
                        }
                    }
                    UI_Spacer(UI_SizePx(16.0f, 1.0f));
                }
            }
        }
    }
}

Function void
SCTR_PanelBuildUI(OpaqueHandle window, SCTR_Panel *panel)
{
    SCTR_Dict *dict = G_WindowUserDataGet(window);
    Arena *arena = G_WindowArenaGet(window);
    
    UI_SetNextChildrenLayout(Axis2_X);
    UI_SetNextGrowth(-4.0f);
    UI_Node *node = UI_NodeMakeFromFmt(UI_Flags_DrawFill | UI_Flags_DrawDropShadow,
                                       "scatter panel %p", panel);
    
    UI_Parent(node)
    {
        UI_Spacer(UI_SizePx(16.0f, 1.0f));
        UI_Width(UI_SizeSum(1.0f)) UI_Column()
        {
            UI_Spacer(UI_SizePx(16.0f, 1.0f));
            
            UI_Height(UI_SizePx(32.0f, 1.0f))
            {
                UI_Width(UI_SizePx(128.0f, 1.0f))
                {
                    UI_Node *auto_scale_button = UI_NodeMakeFromFmt(UI_Flags_DrawFill |
                                                                    UI_Flags_DrawText |
                                                                    UI_Flags_DrawDropShadow |
                                                                    UI_Flags_DrawHotStyle |
                                                                    UI_Flags_DrawActiveStyle |
                                                                    UI_Flags_Pressable |
                                                                    UI_Flags_AnimateHot |
                                                                    UI_Flags_AnimateActive,
                                                                    "auto scale##scatter %p", panel);
                    if(UI_UseFromNode(auto_scale_button).is_pressed)
                    {
                        Range2F range = Range2F(U2F(FLT_MAX), U2F(FLT_MIN));
                        for(SCTR_PanelPerSeriesData *psd = panel->series; 0 != psd; psd = psd->next)
                        {
                            SCTR_Series *series = SCTR_SeriesFromName(window, psd->series_name);
                            range = Bounds2F(range, series->range);
                        }
                        
                        V2F panel_dim = Dimensions2F(panel->rect);
                        panel->scale_x = panel_dim.x / (range.max.x - range.min.x);
                        panel->scale_y = panel_dim.y / (range.max.y - range.min.y);
                        panel->pan_x = range.min.x + panel_dim.x*0.5 / panel->scale_x;
                        panel->pan_y = range.min.y + panel_dim.y*0.5 / panel->scale_y;
                    }
                }
            }
            
            UI_Spacer(UI_SizePx(16.0f, 1.0f));
            
            SCTR_SeriesChecklistBuildUI(window, arena, panel, dict->series);
        }
        
        UI_Width(UI_SizePx(10.0f, 1.0f)) UI_Height(UI_SizeFill()) UI_Growth(-4.0f)
            UI_NodeMake(UI_Flags_DrawDropShadow | UI_Flags_DrawFill, S8(""));
        
        UI_Width(UI_SizeFill()) UI_Height(UI_SizeFill())
        {
            UI_Node *inner_rect = UI_NodeMakeFromFmt(UI_Flags_Pressable | UI_Flags_Scrollable, "scatter panel %p inner rect", panel);
            panel->rect = Grow2F(inner_rect->rect, U2F(-4.0f));
            
            UI_Parent(inner_rect)
                if(0 == panel->series)
            {
                UI_Label(S8("select series to show"));
            }
            
            UI_Use use = UI_UseFromNode(inner_rect);
            
            panel->scale_x += panel->scale_x*use.scroll_delta.y / 10.f;
            panel->scale_y += panel->scale_y*use.scroll_delta.y / 10.f;
            panel->scale_x = Max(panel->scale_x, 0.0);
            panel->scale_y = Max(panel->scale_y, 0.0);
            
            panel->pan_x -= use.drag_delta.x / panel->scale_x;
            panel->pan_y -= use.drag_delta.y / panel->scale_y;
        }
    }
}

Function V2F
SCTR_PanelScreenPositionFromPoint(SCTR_Panel *panel, SCTR_Point point)
{
    V2F panel_dim = Dimensions2F(panel->rect);
    V2F p = V2F((point.x - panel->pan_x)*panel->scale_x + panel_dim.x*0.5,
                (point.y - panel->pan_y)*panel->scale_y + panel_dim.y*0.5);
    p = Add2F(p, panel->rect.min);
    return p;
}

Function void
SCTR_PanelDraw(OpaqueHandle window, R2D_QuadList *quads, LINE_Batch *line_batch, SCTR_Panel *panel)
{
    Range2F mask = panel->rect;
    if(mask.min.x >= mask.max.x ||
       mask.min.y >= mask.max.y)
    {
        mask = G_WindowClientRectGet(window);
    }
    
    for(SCTR_PanelPerSeriesData *psd = panel->series; 0 != psd; psd = psd->next)
    {
        if(psd->is_showing)
        {
            SCTR_Series *series = SCTR_SeriesFromName(window, psd->series_name);
            
            V2F prev_point = {0};
            
            for(SCTR_Points *chunk = &series->points; 0 != chunk; chunk = chunk->next)
            {
                for(U64 point_index = 0; point_index < chunk->points_count; point_index += 1)
                {
                    V2F p = SCTR_PanelScreenPositionFromPoint(panel, chunk->points[point_index]);
                    
                    if(point_index > 0 && psd->is_connected)
                    {
                        V4F col = Scale4F(sctr_palette[psd->col_index], 0.5f);
                        LINE_SegmentLinear segment =
                        {
                            .start = { .pos = prev_point, .col = col, .width = 2.0f, },
                            .end = { .pos = p, .col = col, .width = 2.0f, },
                        };
                        while(!LINE_BatchAppendSegmentLinear(line_batch, segment))
                        {
                            LINE_BatchSubmit(window, line_batch, mask);
                            LINE_BatchClear(line_batch);
                        }
                    }
                    
                    if(HasPoint2F(panel->rect, p))
                    {
                        V2F s = U2F(4.0f);
                        R2D_Quad quad =
                        {
                            .dst = Range2F(Sub2F(p, s), Add2F(p, s)),
                            .src = r2d_entire_texture,
                            .fill_colour = sctr_palette[psd->col_index],
                            .corner_radius = s.x,
                            .edge_softness = 1.0f,
                        };
                        while(!R2D_QuadListAppend(quads, quad))
                        {
                            R2D_QuadListSubmit(window, quads, R2D_TextureNil(), mask);
                            R2D_QuadListClear(quads);
                        }
                    }
                    
                    prev_point = p;
                }
            }
        }
    }
    
    LINE_BatchSubmit(window, line_batch, mask);
    R2D_QuadListSubmit(window, quads, R2D_TextureNil(), mask);
}

Function void
SCTR_WindowHookDraw(OpaqueHandle window)
{
    // NOTE(tbt): get per window data
    SCTR_Dict *dict = G_WindowUserDataGet(window);
    Arena *arena = G_WindowArenaGet(window);
    Range2F client_rect = G_WindowClientRectGet(window);
    
    G_WindowClearColourSet(window, ColFromHex(0xd8dee9ff));
    
    UI_Begin(dict->ui_state);
    
    UI_Column() UI_HotFgCol(ColFromHex(0x81a1c1ff))
    {
        UI_Height(UI_SizePx(40.0f, 1.0f)) UI_Row() UI_Width(UI_SizeFromText(8.0f, 1.0f)) UI_Font(ui_font_icons)
        {
            UI_Node *open_panel_button = UI_NodeMakeFromFmt(UI_Flags_DrawFill |
                                                            UI_Flags_DrawText |
                                                            UI_Flags_DrawDropShadow |
                                                            UI_Flags_DrawHotStyle |
                                                            UI_Flags_DrawActiveStyle |
                                                            UI_Flags_Pressable |
                                                            UI_Flags_AnimateHot |
                                                            UI_Flags_AnimateActive,
                                                            "%c##scatter new panel",
                                                            FONT_Icon_PlusCircled);
            if(UI_UseFromNode(open_panel_button).is_pressed)
            {
                SCTR_Panel *panel = ArenaPush(arena, sizeof(*panel));
                panel->next = dict->panels;
                dict->panels = panel;
            }
        }
        
        UI_Row()
        {
            for(SCTR_Panel *panel = dict->panels; 0 != panel; panel = panel->next)
            {
                SCTR_PanelBuildUI(window, panel);
            }
        }
    }
    
    UI_End();
    
    R2D_QuadListClear(dict->quads);
    for(SCTR_Panel *panel = dict->panels; 0 != panel; panel = panel->next)
    {
        SCTR_PanelDraw(window, dict->quads, dict->line_batch, panel);
    }
}

Function void
SCTR_WindowHookClose(OpaqueHandle window)
{
    SCTR_Dict *dict = G_WindowUserDataGet(window);
    R2D_QuadListDestroy(dict->quads);
}

Function OpaqueHandle
SCTR_OpenWindow(S8 window_title)
{
    OpaqueHandle result = G_WindowOpen(window_title, V2I(1024, 768), sctr_window_hooks, 0);
    return result;
}

Function SCTR_Series *
SCTR_SeriesFromName(OpaqueHandle window, S8 name)
{
    SCTR_Series *series = 0;
    
    SCTR_Dict *dict = G_WindowUserDataGet(window);
    
    for(SCTR_Series *s = dict->series; 0 != s; s = s->next)
    {
        if(S8Match(name, s->name, 0))
        {
            series = s;
            break;
        }
    }
    
    if(0 == series)
    {
        Arena *arena = G_WindowArenaGet(window);
        series = ArenaPush(arena, sizeof(*series));
        series->next = dict->series;
        dict->series = series;
        series->name = S8Clone(arena, name);
        series->range = Range2F(U2F(FLT_MAX), U2F(FLT_MIN));
    }
    
    return series;
}

Function void
SCTR_SetMinTimeBetweenPlots(OpaqueHandle window, S8 series_name, U64 microseconds)
{
    SCTR_Series *series = SCTR_SeriesFromName(window, series_name);
    
    series->min_difference_t = microseconds;
}

Function void
SCTR_SetMinDifferenceX(OpaqueHandle window, S8 series_name, F64 difference)
{
    SCTR_Series *series = SCTR_SeriesFromName(window, series_name);
    
    series->min_difference_x = difference;
}

Function void
SCTR_SetMinDifferenceY(OpaqueHandle window, S8 series_name, F64 difference)
{
    SCTR_Series *series = SCTR_SeriesFromName(window, series_name);
    
    series->min_difference_y = difference;
}

Function void
SCTR_PlotPoint(OpaqueHandle window, S8 series_name, F64 x, F64 y)
{
    y *= -1;
    
    if(IsNaN1F(x) || IsNaN1F(y))
    {
        return;
    }
    
    SCTR_Series *series = SCTR_SeriesFromName(window, series_name);
    
    U64 t = OS_TimeInMicroseconds();
    
    if(0 == series->last_plot ||
       ((t - series->last_plot_t) >= series->min_difference_t &&
        Abs1F(x - series->last_plot->x) >= series->min_difference_x &&
        Abs1F(y - series->last_plot->y) >= series->min_difference_y))
    {
        SCTR_Points *points = &series->points;
        while(0 != points && points->points_count >= ArrayCount(points->points))
        {
            points = points->next;
        }
        
        if(0 == points)
        {
            Arena *arena = G_WindowArenaGet(window);
            points = ArenaPush(arena, sizeof(*points));
            points->next = series->points.next;
            series->points.next = points;;
        }
        
        series->last_plot = &points->points[points->points_count];
        series->last_plot->x = x;
        series->last_plot->y = y;
        points->points_count += 1;
        
        series->range.min = Mins2F(series->range.min, V2F(x, y));
        series->range.max = Maxs2F(series->range.max, V2F(x, y));
        
        series->last_plot_t = t;
    }
    else
    {
        series->filter = True;
    }
}

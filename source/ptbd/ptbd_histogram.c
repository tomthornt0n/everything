

Function U64
PTBD_HistogramNavNext(PTBD_Index *index, DB_Cache data, I64 *keyboard_focus_id)
{
    PTBD_IndexCoord coord = PTBD_IndexCoordFromDBId(index, data, *keyboard_focus_id);
    PTBD_IndexCoord next_coord = PTBD_IndexNextFromCoord(index, coord);
    U64 next_index = PTBD_IndexCoordLookup(index, next_coord);
    I64 next_id = data.rows[next_index].id;
    *keyboard_focus_id = next_id;
    
    ui->was_last_interaction_keyboard = True;
    
    U64 scroll_to_row_id = next_coord.record;
    
    return scroll_to_row_id;
}

Function U64
PTBD_HistogramNavPrev(PTBD_Index *index, DB_Cache data, I64 *keyboard_focus_id)
{
    PTBD_IndexCoord coord = PTBD_IndexCoordFromDBId(index, data, *keyboard_focus_id);
    PTBD_IndexCoord prev_coord = PTBD_IndexPrevFromCoord(index, coord);
    U64 prev_index = PTBD_IndexCoordLookup(index, prev_coord);
    I64 prev_id = data.rows[prev_index].id;
    *keyboard_focus_id = prev_id;
    
    ui->was_last_interaction_keyboard = True;
    
    U64 scroll_to_row_id = prev_coord.record;
    
    return scroll_to_row_id;
}

Function void
PTBD_HistogramNav(void *user_data, UI_Node *node)
{
    I64 *keyboard_focus_id = user_data;
    if(*keyboard_focus_id > 0)
    {
        ArenaTemp scratch = OS_TCtxScratchBegin(0, 0);
        
        S8 text = S8FromFmt(scratch.arena, "histogram bar %lld", *keyboard_focus_id);
        
        if(S8Match(text, node->text, 0))
        {
            node->flags |= UI_Flags_KeyboardFocused;
        }
        
        ArenaTempEnd(scratch);
    }
}

Function void
PTBD_SetNextUIStyleForHistogramBar(DB_Row *timesheet, F32 width_per_hour, F32 height_per_row, F32 padding, B32 is_selected)
{
    UI_SetNextStrokeWidth(0.0f);
    UI_SetNextHotGrowth(3.0f);
    UI_SetNextActiveGrowth(2.0f);
    
    V4F invoice_colour = PTBD_ColourFromInvoiceId(timesheet->invoice_id);
    
    if(0 == timesheet->invoice_id)
    {
        UI_SetNextBgCol(ColMix(ColFromHex(0xd8dee9ff), ColFromHex(0x4c566aff), ptbd_is_dark_mode_t));
    }
    else if(DB_InvoiceNumber_WrittenOff == timesheet->invoice_number)
    {
        UI_SetNextStrokeWidth(4.0f);
        UI_SetNextFgCol(ColFromHex(0xbf616aff));
        UI_SetNextBgCol(U4F(0.0f));
    }
    else if(DB_InvoiceNumber_Draft == timesheet->invoice_number)
    {
        UI_SetNextTexture(ptbd_gradient_vertical);
        UI_SetNextTextureRegion(Range2F(V2F(0.0f, 0.99f), V2F(1.0f, 0.01f)));
        UI_SetNextBgCol(invoice_colour);
    }
    else
    {
        UI_SetNextBgCol(invoice_colour);
    }
    
    if(is_selected)
    {
        UI_SetNextFgCol(ColMix(ColFromHex(0x2e3440ff), ColFromHex(0xd8dee9ff), ptbd_is_dark_mode_t));
        UI_SetNextStrokeWidth(2.0f);
        UI_SetNextGrowth(4.0f);
    }
    
    if(DB_TimesheetIsDisbursement(timesheet))
    {
        F32 zoom = width_per_hour > 48.0f ? 2.0f : 1.0f;
        UI_SetNextCornerRadius(0.5f*(height_per_row - padding));
        UI_SetNextWidth(UI_SizePx(zoom*height_per_row - padding, 1.0f));
        UI_SetNextShadowRadius(12.0f);
    }
    else
    {
        UI_SetNextCornerRadius(4.0f);
        UI_SetNextWidth(UI_SizePx(width_per_hour*timesheet->hours - padding, 1.0f));
    }
    
    UI_SetNextChildrenLayout(Axis2_X);
}

Function void
PTBD_BuildVerticalLabelFromFmt(const char *fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    PTBD_BuildVerticalLabelFromFmtV(fmt, args);
    va_end(args);
}

Function void
PTBD_BuildVerticalLabelFromFmtV(const char *fmt, va_list args)
{
    ArenaTemp scratch = OS_TCtxScratchBegin(0, 0);
    S8 string = S8FromFmtV(scratch.arena, fmt, args);
    PTBD_BuildVerticalLabel(string);
    ArenaTempEnd(scratch);
}

Function void
PTBD_BuildVerticalLabel(S8 string)
{
    ArenaTemp scratch = OS_TCtxScratchBegin(0, 0);
    
    S8List chars = {0};
    while(string.len > 0)
    {
        UTFConsume consume = CodepointFromUTF8(string, 0);
        S8 c = S8Advance(&string, consume.advance);
        S8ListAppend(scratch.arena, &chars, c);
    }
    
    S8 vertical_string = S8ListJoinFormatted(scratch.arena, chars, S8ListJoinFormat(.delimiter = S8("\n")));
    
    UI_TextTruncSuffix(S8(""))
        UI_Label(vertical_string);
    
    ArenaTempEnd(scratch);
}

Function UI_Node *
PTBD_BuildHistogramInner(S8 string,
                         DB_Cache timesheets,
                         DB_Cache jobs,
                         DB_Cache employees,
                         PTBD_Index *index,
                         UI_Window window,
                         PTBD_Selection *selection,
                         PTBD_BuildHistogramInnerFlags flags,
                         PTBD_Histogram *view,
                         F32 padding,
                         PTBD_MsgQueue *m2c,
                         PTBD_Inspectors *inspectors)
{
    ArenaTemp scratch = OS_TCtxScratchBegin(0, 0);
    
    UI_SetNextChildrenLayout(Axis2_Y);
    UI_Node *scroll_region =
        UI_NodeMake(UI_Flags_DrawStroke |
                    UI_Flags_OverflowY |
                    UI_Flags_AnimateSmoothScroll |
                    UI_Flags_Pressable,
                    string);
    
    scroll_region->target_view_offset = view->scroll;
    
    UI_Parent(scroll_region)
    {
        UI_Spacer(UI_SizePx(window.space_before, 1.0f));
        
        UI_Height(UI_SizePx(view->height_per_row - padding, 1.0f)) 
            for(U64 row_index = window.index_range.min; row_index < window.index_range.max; row_index += 1)
        {
            PTBD_IndexRecord *record = &index->records[row_index];
            UI_Width(UI_SizeSum(1.0f)) UI_Row()
            {
                for(U64 timesheet_index = 0; timesheet_index < record->indices_count; timesheet_index += 1)
                {
                    DB_Row *timesheet = &timesheets.rows[record->indices[timesheet_index]];
                    UI_SaltFmt("%lld", timesheet->id)
                    {
                        PTBD_Inspector *inspector = PTBD_InspectorFromId(inspectors, timesheet->id);
                        
                        B32 is_selected = PTBD_SelectionHas(selection, timesheet->id);
                        B32 is_disbursement = DB_TimesheetIsDisbursement(timesheet);
                        
                        PTBD_SetNextUIStyleForHistogramBar(timesheet, view->width_per_hour, view->height_per_row, padding, is_selected);
                        UI_Node *bar = UI_NodeMake(UI_Flags_DrawFill |
                                                   UI_Flags_DrawStroke |
                                                   UI_Flags_DrawDropShadow |
                                                   UI_Flags_DrawHotStyle |
                                                   UI_Flags_DrawActiveStyle |
                                                   UI_Flags_Pressable |
                                                   UI_Flags_AnimateHot |
                                                   UI_Flags_AnimateActive |
                                                   (UI_Flags_AnimateSmoothLayout*((flags & PTBD_BuildHistogramInnerFlags_AnimateSmoothBars) || is_disbursement)),
                                                   S8("histogram bar"));
                        
                        UI_Animate1F(&bar->user_t, (0 == inspector ? 0 : inspector->should_write) || ptbd_scroll_to[DB_Table_Timesheets] == timesheet->id, 15.0f);
                        
                        bar->growth += 8.0f*bar->user_t;
                        bar->fg = ColMix(bar->fg, ColFromHex(0x81a1c1ff), bar->user_t);
                        
                        UI_Parent(bar)
                            UI_Pad(UI_SizePx(Max1F(4.0f, bar->corner_radius*0.33f), 1.0f))
                            UI_FontSize(16)
                            UI_TextTruncSuffix(S8(""))
                        {
                            F32 bar_width = bar->size[Axis2_X].f; // NOTE(tbt): always UI_SizeMode_Pixels
                            F32 width_threshold_empty = 20.0f;
                            F32 width_threshold_vertical = view->height_per_row*0.8f;
                            
                            if(bar_width >= width_threshold_empty)
                            {
                                UI_Height(UI_SizeFill()) UI_Width(UI_SizeFill())
                                    UI_Column()
                                    UI_Pad(UI_SizeFill())
                                    UI_Width(UI_SizeFromText(2.0f, 1.0f))
                                    UI_Height(UI_SizeFromText(4.0f, 1.0f))
                                {
                                    DB_Row *job = DB_RowFromId(jobs, timesheet->job_id);
                                    DB_Row *employee = DB_RowFromId(employees, timesheet->employee_id);
                                    
                                    if(is_disbursement)
                                    {
                                        if(bar_width > view->height_per_row)
                                        {
                                            if(flags & PTBD_BuildHistogramInnerFlags_JobInfoInBars)
                                            {
                                                UI_BgCol(bar->bg) PTBD_BuildJobEmblem(job, True);
                                            }
                                            UI_SetNextHeight(UI_SizeSum(1.0f));
                                            UI_SetNextWidth(UI_SizeSum(0.25f));
                                            UI_Row() { UI_LabelFromIcon(FONT_Icon_Pound); UI_LabelFromFmt("%.2f", timesheet->cost); }
                                            UI_TextTruncSuffix(S8("...")) UI_Label(timesheet->description);
                                        }
                                        else
                                        {
                                            if(flags & PTBD_BuildHistogramInnerFlags_JobInfoInBars)
                                            {
                                                UI_BgCol(bar->bg) PTBD_BuildJobEmblem(job, False);
                                            }
                                            UI_SetNextHeight(UI_SizeSum(1.0f));
                                            UI_SetNextWidth(UI_SizeSum(0.25f));
                                            UI_Row() { UI_LabelFromIcon(FONT_Icon_Pound); UI_LabelFromFmt("%.2f", timesheet->cost); }
                                        }
                                    }
                                    else
                                    {
                                        if(bar_width < width_threshold_vertical)
                                        {
                                            UI_Width(UI_SizeFill()) UI_FontSize(12)
                                            {
                                                if(flags & PTBD_BuildHistogramInnerFlags_JobInfoInBars)
                                                {
                                                    UI_Font(ui_font_bold) PTBD_BuildVerticalLabel(timesheet->job_number);
                                                }
                                                else
                                                {
                                                    F32 hours = Floor1F(timesheet->hours);
                                                    F32 minutes = 60.0f*Fract1F(timesheet->hours);
                                                    PTBD_BuildVerticalLabelFromFmt("%.0f:%02.0f", hours, minutes);
                                                }
                                            }
                                        }
                                        else
                                        {
                                            if(flags & PTBD_BuildHistogramInnerFlags_JobInfoInBars)
                                            {
                                                UI_BgCol(bar->bg) PTBD_BuildJobEmblem(job, True);
                                            }
                                            
                                            UI_SetNextHeight(UI_SizeSum(1.0f));
                                            UI_SetNextWidth(UI_SizeSum(0.25f));
                                            UI_Row()
                                            {
                                                F32 hours = Floor1F(timesheet->hours);
                                                F32 minutes = 60.0f*Fract1F(timesheet->hours);
                                                UI_LabelFromIcon(FONT_Icon_Clock);
                                                UI_LabelFromFmt("%.0f:%02.0f", hours, minutes);
                                                
                                                UI_Spacer(UI_SizePx(4.0f, 1.0f));
                                                
                                                UI_LabelFromIcon(FONT_Icon_Cab);
                                                UI_LabelFromFmt("%.2f", timesheet->miles);
                                                
                                                UI_Spacer(UI_SizePx(4.0f, 1.0f));
                                                
                                                PTBD_BuildEmployeeEmblem(employee, bar_width > 200.0f);
                                            }
                                            
                                            UI_TextTruncSuffix(S8("...")) UI_Label(timesheet->description);
                                        }
                                    }
                                }
                            }
                            
                            if(!is_disbursement)
                            {
                                UI_SetNextWidth(UI_SizePx(12.0f, 1.0f));
                                UI_SetNextBgCol(ColMix(ColFromHex(0xd8dee9ff),
                                                       ColFromHex(0x4c566aff),
                                                       ptbd_is_dark_mode_t));
                                UI_SetNextGrowth(-4.0f);
                                UI_SetNextHotGrowth(-2.0f);
                                UI_SetNextActiveGrowth(-2.0f);
                                UI_SetNextCornerRadius(2.0f);
                                UI_SetNextHotCornerRadius(4.0f);
                                UI_SetNextActiveCornerRadius(4.0f);
                                UI_SetNextHotCursorKind(G_CursorKind_HResize);
                                UI_SetNextActiveCursorKind(G_CursorKind_Hidden);
                                UI_Node *resize_bar = UI_NodeMake(UI_Flags_DrawFill |
                                                                  UI_Flags_DrawDropShadow |
                                                                  UI_Flags_DrawHotStyle |
                                                                  UI_Flags_DrawActiveStyle |
                                                                  UI_Flags_Pressable |
                                                                  UI_Flags_NavDefaultFilter |
                                                                  UI_Flags_SetCursorKind |
                                                                  UI_Flags_AnimateHot |
                                                                  UI_Flags_AnimateActive |
                                                                  UI_Flags_CursorPositionToCenter,
                                                                  S8("resize bar"));
                                UI_Use use = UI_UseFromNode(resize_bar);
                                
                                Range1F range = Range1F(0.5f, 12.0f);
                                
                                double mouse_x = G_WindowMousePositionGet(ui->window).x;
                                
                                double new_value = (mouse_x - bar->rect.min.x + use.initial_mouse_relative_position.x) / (view->width_per_hour*G_WindowScaleFactorGet(ui->window));
                                new_value = IsNaN1F(new_value) ? 0.0f : new_value;
                                new_value = Clamp1F(new_value, range.min, range.max);
                                new_value = Round1F(new_value * 12.0f) / 12.0f;
                                
                                if(use.is_left_up)
                                {
                                    M2C_WriteTimesheet(m2c,
                                                       timesheet->id,
                                                       timesheet->date,
                                                       timesheet->employee_id,
                                                       timesheet->job_id,
                                                       timesheet->hours, // NOTE(tbt): cached value has already been overwritten - better to write what the user can see, rather than a new value which could be inconsistent
                                                       timesheet->miles,
                                                       -1.0f,
                                                       timesheet->description,
                                                       timesheet->invoice_id);
                                    M2C_Reload(m2c);
                                }
                                else if(use.is_dragging)
                                {
                                    timesheet->hours = new_value;
                                    
                                    if(0 != inspector)
                                    {
                                        F32 hours = Floor1F(new_value);
                                        F32 minutes = 60.0f*Fract1F(new_value);
                                        
                                        inspector->fields[PTBD_InspectorColumn_Hours].len = 0;
                                        S8BuilderAppendFmt(&inspector->fields[PTBD_InspectorColumn_Hours], "%.0f:%02.0f", hours, minutes);
                                    }
                                }
                            }
                        }
                        
                        if(view->is_dragging_selection_rect && IsOverlapping2F(bar->rect, scroll_region->rect))
                        {
                            Arena *permanent_arena = OS_TCtxGet()->permanent_arena;
                            PTBD_SelectionSet(permanent_arena, selection, timesheet->id, IsOverlapping2F(view->selection_rect, bar->rect));
                        }
                        
                        UI_Use use = UI_UseFromNode(bar);
                        if(use.is_pressed)
                        {
                            Arena *permanent_arena = OS_TCtxGet()->permanent_arena;
                            
                            if(G_WindowKeyStateGet(ui->window, G_Key_Shift) && view->last_selected_id > 0)
                            {
                                PTBD_SelectionClear(selection);
                                
                                PTBD_IndexCoord start_coord = PTBD_IndexCoordFromDBId(index, timesheets, view->last_selected_id);
                                PTBD_IndexCoord target_coord = PTBD_IndexCoordFromDBId(index, timesheets, timesheet->id);
                                
                                PTBD_SelectionSet(permanent_arena, selection, view->last_selected_id, True);
                                
                                U64 no_of_iterations = 0;
                                U64 max_no_of_iterations = 512;
                                
                                PTBD_IndexCoord coord = start_coord;
                                while(coord.record != target_coord.record || coord.index != target_coord.index)
                                {
                                    no_of_iterations += 1;
                                    if(no_of_iterations > max_no_of_iterations)
                                    {
                                        break;
                                    }
                                    
                                    if((target_coord.record < start_coord.record) ||
                                       (target_coord.record == start_coord.record && target_coord.index < start_coord.index))
                                    {
                                        coord = PTBD_IndexPrevFromCoord(index, coord);
                                    }
                                    else if((target_coord.record > start_coord.record) ||
                                            (target_coord.record == start_coord.record && target_coord.index > start_coord.index))
                                    {
                                        coord = PTBD_IndexNextFromCoord(index, coord);
                                    }
                                    U64 timesheet_index = PTBD_IndexCoordLookup(index, coord);
                                    I64 timesheet_id = timesheets.rows[timesheet_index].id;
                                    
                                    PTBD_SelectionSet(permanent_arena, selection, timesheet_id, True);
                                }
                            }
                            else
                            {
                                if(!G_WindowKeyStateGet(ui->window, G_Key_Ctrl))
                                {
                                    PTBD_SelectionClear(selection);
                                }
                                PTBD_SelectionSet(permanent_arena, selection, timesheet->id, !is_selected);
                                view->last_selected_id = timesheet->id;
                                view->keyboard_focus_id = timesheet->id;
                            }
                        }
                        else if(use.is_dragging && 0 == ptbd_drag_drop.count)
                        {
                            if(is_selected)
                            {
                                PTBD_DragSelection(DB_Table_Timesheets, timesheets, selection);
                            }
                            else
                            {
                                PTBD_DragRow(DB_Table_Timesheets, *timesheet);
                            }
                        }
                        else if(use.is_left_up && ptbd_drag_drop.count > 0)
                        {
                            if(ptbd_drag_drop.count == 1)
                            {
                                if(DB_Table_Jobs == ptbd_drag_drop.kind)
                                {
                                    DB_Row *job = ptbd_drag_drop.data;
                                    M2C_WriteTimesheet(m2c, 
                                                       timesheet->id,
                                                       timesheet->date,
                                                       timesheet->employee_id,
                                                       job->id,
                                                       timesheet->hours,
                                                       timesheet->miles,
                                                       timesheet->cost,
                                                       timesheet->description,
                                                       timesheet->invoice_id);
                                    M2C_Reload(m2c);
                                }
                                else if(DB_Table_Employees == ptbd_drag_drop.kind)
                                {
                                    DB_Row *employee = ptbd_drag_drop.data;
                                    M2C_WriteTimesheet(m2c, 
                                                       timesheet->id,
                                                       timesheet->date,
                                                       employee->id,
                                                       timesheet->job_id,
                                                       timesheet->hours,
                                                       timesheet->miles,
                                                       timesheet->cost,
                                                       timesheet->description,
                                                       timesheet->invoice_id);
                                    M2C_Reload(m2c);
                                }
                                else
                                {
                                    OS_TCtxErrorPush(S8("Drag a job or employee here to edit this timesheet."));
                                }
                            }
                            else
                            {
                                OS_TCtxErrorPush(S8("Drag a single job or employee here to edit this timesheet."));
                            }
                        }
                        UI_Spacer(UI_SizePx(padding, 1.0f));
                    }
                }
            }
            UI_Spacer(UI_SizePx(padding, 1.0f));
        }
        
        UI_Spacer(UI_SizePx(window.space_after, 1.0f));
    }
    
    ArenaTempEnd(scratch);
    
    return scroll_region;
}

Function UI_Use
PTBD_BuildScrollBarForHistogram(S8 string, F32 *scroll, F32 overflow, F32 container_size)
{
    F32 s = G_WindowScaleFactorGet(ui->window);
    F32 positive_scroll = -(*scroll);
    F32 thumb_size = Max1F(container_size*container_size / ((container_size + overflow)*s), 16.0f);
    UI_Use use = UI_Slider1F(string, &positive_scroll, Range1F(0.0f, overflow), thumb_size);
    *scroll = -positive_scroll;
    return use;
}

Function UI_Node *
PTBD_BuildHistogramLeaderColumn(PTBD_BuildHistogramLeaderColumnFlags flags,
                                S8 string,
                                DB_Cache timesheets,
                                DB_Cache jobs,
                                PTBD_Index *index,
                                UI_Window window,
                                PTBD_Selection *selection,
                                I64 *last_selected_id,
                                F32 height_per_row,
                                F32 padding,
                                F32 vertical_scroll,
                                PTBD_MsgQueue *m2c)
{
    ArenaTemp scratch = OS_TCtxScratchBegin(0, 0);
    
    UI_SetNextChildrenLayout(Axis2_Y);
    UI_SetNextFgCol(ptbd_grey);
    UI_SetNextCornerRadius(16.0f);
    UI_Node *column = UI_NodeMake(UI_Flags_DrawFill |
                                  UI_Flags_DrawStroke |
                                  UI_Flags_DrawDropShadow |
                                  UI_Flags_Overflow |
                                  UI_Flags_AnimateSmoothScroll,
                                  string);
    
    column->target_view_offset.y = vertical_scroll;
    
    UI_Parent(column)
    {
        UI_Spacer(UI_SizePx(window.space_before, 1.0f));
        
        UI_Height(UI_SizePx(height_per_row - padding, 1.0f)) 
            for(U64 row_index = window.index_range.min; row_index < window.index_range.max; row_index += 1)
        {
            PTBD_IndexRecord *record = &index->records[row_index];
            DB_Row *job = &jobs.rows[record->key];
            
            UI_Row()
            {
                if(flags & PTBD_BuildHistogramLeaderColumnFlags_DateMode)
                {
                    DateTime date = DateTimeFromDenseTime(record->key);
                    DayOfWeek day_of_week = DayOfWeekFromDateTime(date);
                    if(DayOfWeek_Saturday == day_of_week ||
                       DayOfWeek_Sunday == day_of_week)
                    {
                        UI_SetNextCornerRadius(16.0f);
                        UI_SetNextBgCol(ColMix(ColFromHex(0xe5e9f0ff), UI_BgColPeek(), ptbd_is_dark_mode_t));
                        UI_SetNextDefaultFlags(UI_Flags_DrawFill | UI_Flags_DrawStroke | UI_Flags_DrawDropShadow);
                        UI_SetNextGrowth(-8.0f);
                        UI_SetNextFgCol(ptbd_grey);
                    }
                }
                UI_SetNextHotGrowth(UI_GrowthPeek());
                UI_SetNextActiveGrowth(UI_GrowthPeek());
                UI_Node *button = UI_NodeMakeFromFmt(UI_Flags_Pressable |
                                                     UI_Flags_AnimateHot |
                                                     UI_Flags_AnimateActive,
                                                     "histogram leader %llu",
                                                     record->key);
                
                UI_Parent(button)
                {
                    UI_HotGrowth(UI_GrowthPeek())
                        UI_ActiveGrowth(UI_GrowthPeek())
                        UI_HotFgCol(ColFromHex(0x81a1c1ff))
                        UI_ActiveFgCol(ColFromHex(0x88c0d0ff))
                        UI_DefaultFlags(UI_Flags_AnimateInheritHot | UI_Flags_AnimateInheritActive)
                        UI_Width(UI_SizeFill())
                    {
                        if(flags & PTBD_BuildHistogramLeaderColumnFlags_DateMode)
                        {
                            DateTime date = DateTimeFromDenseTime(record->key);
                            UI_Font(ui_font_bold) UI_LabelFromFmt("%02d/%02d/%d", date.day + 1, date.mon + 1, date.year);
                        }
                        else if(flags & PTBD_BuildHistogramLeaderColumnFlags_JobMode)
                        {
                            UI_Column() UI_Height(UI_SizeFromText(0.0f, 1.0f)) UI_Pad(UI_SizeFill())
                            {
                                UI_Font(ui_font_bold) UI_Label(job->job_number);
                                UI_Label(job->job_name);
                            }
                        }
                    }
                    
                    if(flags & PTBD_BuildHistogramLeaderColumnFlags_NewTimesheetButton)
                    {
                        UI_SetNextWidth(UI_SizeFromText(4.0f, 1.0f));
                        S8 button_id = S8FromFmt(scratch.arena, "new timesheet %llu", record->key);
                        if(UI_IconButton(button_id, FONT_Icon_PlusCircled).is_pressed)
                        {
                            DenseTime date = DenseTimeFromDateTime(OS_NowLTC());
                            if(flags & PTBD_BuildHistogramLeaderColumnFlags_DateMode)
                            {
                                date = record->key;
                            }
                            I64 job_id = 0;
                            if(flags & PTBD_BuildHistogramLeaderColumnFlags_JobMode)
                            {
                                job_id = jobs.rows[record->key].id;
                            }
                            
                            M2C_WriteTimesheet(m2c, 0, date, 0, job_id, 4.0f, 0.0f, -1.0f, S8("description"), 0);
                            M2C_Reload(m2c);
                        }
                        
                        UI_Spacer(UI_SizePx(8.0f, 1.0f));
                    }
                }
                
                UI_Use use = UI_UseFromNode(button);
                if(use.is_pressed)
                {
                    if(G_WindowKeyStateGet(ui->window, G_Key_Shift) && *last_selected_id > 0)
                    {
                        PTBD_SelectionClear(selection);
                        
                        Arena *permanent_arena = OS_TCtxGet()->permanent_arena;
                        
                        U64 start_row = PTBD_IndexCoordFromDBId(index, timesheets, *last_selected_id).record;
                        U64 target_row = row_index;
                        
                        {
                            PTBD_IndexRecord *r = &index->records[start_row];
                            for(U64 timesheet_index = 0; timesheet_index < r->indices_count; timesheet_index += 1)
                            {
                                DB_Row *timesheet = &timesheets.rows[r->indices[timesheet_index]];
                                PTBD_SelectionSet(permanent_arena, selection, timesheet->id, True);
                            }
                        }
                        
                        U64 row = start_row;
                        while(row != target_row)
                        {
                            if(target_row < start_row) { row -= 1; }
                            else                       { row += 1; }
                            PTBD_IndexRecord *r = &index->records[row];
                            for(U64 timesheet_index = 0; timesheet_index < r->indices_count; timesheet_index += 1)
                            {
                                DB_Row *timesheet = &timesheets.rows[r->indices[timesheet_index]];
                                PTBD_SelectionSet(permanent_arena, selection, timesheet->id, True);
                            }
                        }
                    }
                    else
                    {
                        if(!G_WindowKeyStateGet(ui->window, G_Key_Ctrl))
                        {
                            PTBD_SelectionClear(selection);
                        }
                        for(U64 timesheet_index = 0; timesheet_index < record->indices_count; timesheet_index += 1)
                        {
                            Arena *permanent_arena = OS_TCtxGet()->permanent_arena;
                            DB_Row *timesheet = &timesheets.rows[record->indices[timesheet_index]];
                            PTBD_SelectionSet(permanent_arena, selection, timesheet->id, True);
                            *last_selected_id = timesheet->id;
                        }
                    }
                }
                else if(use.is_dragging && 0 == ptbd_drag_drop.count)
                {
                    if(flags & PTBD_BuildHistogramLeaderColumnFlags_JobMode)
                    {
                        PTBD_DragRow(DB_Table_Jobs, *job);
                    }
                }
                else if(use.is_left_up && ptbd_drag_drop.count > 0)
                {
                    if(DB_Table_Timesheets == ptbd_drag_drop.kind)
                    {
                        M2C_UndoContinuationBegin(m2c);
                        for(U64 payload_index = 0; payload_index < ptbd_drag_drop.count; payload_index += 1)
                        {
                            DB_Row *timesheet = &ptbd_drag_drop.data[payload_index];
                            
                            I64 job_id = timesheet->job_id;
                            if(flags & PTBD_BuildHistogramLeaderColumnFlags_JobMode)
                            {
                                job_id = jobs.rows[record->key].id;
                            }
                            
                            DenseTime date = timesheet->date;
                            if(flags & PTBD_BuildHistogramLeaderColumnFlags_DateMode)
                            {
                                date = record->key;
                            }
                            
                            M2C_WriteTimesheet(m2c,
                                               timesheet->id,
                                               date,
                                               timesheet->employee_id,
                                               job_id,
                                               timesheet->hours,
                                               timesheet->miles,
                                               timesheet->cost,
                                               timesheet->description,
                                               timesheet->invoice_id);
                        }
                        M2C_UndoContinuationEnd(m2c);
                        M2C_Reload(m2c);
                    }
                    else
                    {
                        OS_TCtxErrorPushFmt("Drag timesheets here to move to this %s.",
                                            (flags & PTBD_BuildHistogramLeaderColumnFlags_DateMode) ? "date" :
                                            (flags & PTBD_BuildHistogramLeaderColumnFlags_JobMode) ? "job" : "");
                    }
                }
            }
            UI_Spacer(UI_SizePx(padding, 1.0f));
        }
        
        UI_Spacer(UI_SizePx(window.space_after, 1.0f));
    }
    
    ArenaTempEnd(scratch);
    
    return column;
}

Function UI_Use
PTBD_BuildHistogramHoursScale(S8 string, DB_Cache timesheets, PTBD_Index *index, F32 *width_per_hour, F32 leader_column_width, F32 horizontal_scroll)
{
    UI_Use result = {0};
    
    F32 s = G_WindowScaleFactorGet(ui->window);
    
    F32 max_total_hours = 0.0f;
    for(U64 record_index = 0; record_index < index->records_count; record_index += 1)
    {
        PTBD_IndexRecord *record = &index->records[record_index];
        
        F32 row_total_hours = 0.0f;
        for(U64 timesheet_index = 0; timesheet_index < record->indices_count; timesheet_index += 1)
        {
            DB_Row *timesheet = &timesheets.rows[record->indices[timesheet_index]];
            row_total_hours += timesheet->hours;
        }
        
        max_total_hours = Max1F(max_total_hours, row_total_hours);
    }
    
    UI_ChildrenLayout(Axis2_X)
    {
        UI_Node *row = UI_NodeMake(UI_Flags_DoNotMask | UI_Flags_AnimateSmoothScroll, string);
        row->target_view_offset.x = horizontal_scroll - 0.5f**width_per_hour*s + leader_column_width*s;
        
        UI_Parent(row) UI_Salt(string)
        {
            UI_SetNextWidth(UI_SizeSum(1.0f));
            UI_SetNextHotCursorKind(G_CursorKind_HResize);
            UI_SetNextActiveCursorKind(G_CursorKind_Hidden);
            UI_SetNextFgCol(ptbd_grey);
            UI_SetNextCornerRadius(14.0f);
            UI_Node *inner = UI_NodeMake(UI_Flags_DrawFill |
                                         UI_Flags_DrawStroke |
                                         UI_Flags_DrawDropShadow |
                                         UI_Flags_Pressable |
                                         UI_Flags_CursorPositionToCenterY |
                                         UI_Flags_AnimateSmoothLayout |
                                         UI_Flags_SetCursorKind,
                                         S8("inner"));
            
            UI_Parent(inner) UI_Width(UI_SizePx(*width_per_hour, 1.0f)) UI_TextTruncSuffix(S8(""))
                for(I32 i = 0; i <= Ceil1F(max_total_hours); i += 1)
            {
                UI_LabelFromFmt("%d", i);
            }
            
            result = UI_UseFromNode(inner);
            
            *width_per_hour = Clamp1F(*width_per_hour + result.drag_delta.x, 32.0f, 300.0f);
        }
    }
    
    return result;
}

Function void
PTBD_BuildTimelineHistogram(DB_Cache timesheets,
                            DB_Cache jobs,
                            DB_Cache employees,
                            PTBD_Index *index,
                            PTBD_Selection *selection,
                            PTBD_Histogram *view,
                            PTBD_MsgQueue *m2c,
                            PTBD_Inspectors *inspectors)
{
    F32 padding = 4.0f;
    
    UI_SetNextChildrenLayout(Axis2_Y);
    UI_Node *container = UI_NodeMake(UI_Flags_Pressable |
                                     UI_Flags_Scrollable |
                                     UI_Flags_DoNotMask |
                                     UI_Flags_AnimateSmoothLayout,
                                     S8("timesheets histogram"));
    
    UI_Node *inner;
    UI_Node *leader;
    
    UI_Parent(container) UI_Salt(S8("timesheets")) UI_Nav(UI_NavContext(PTBD_HistogramNav, &view->keyboard_focus_id, container->key))
    {
        PTBD_ApplyScrollToForHistogram(timesheets,
                                       index,
                                       &view->scroll.y,
                                       container->computed_size.y,
                                       view->height_per_row);
        
        UI_Window window = PTBD_UIWindowFromContainerScrollHeightPerRowAndRowsCount(container, view->height_per_row, index->records_count, view->scroll.y);
        
        UI_SetNextHeight(UI_SizePx(32.0f, 1.0f));
        UI_Use hours_scale = PTBD_BuildHistogramHoursScale(S8("hours scale"),  timesheets, index, &view->width_per_hour, view->leader_column_width, view->scroll.x);
        
        UI_Row()
        {
            UI_SetNextWidth(UI_SizePx(view->leader_column_width, 1.0f));
            leader = PTBD_BuildHistogramLeaderColumn(PTBD_BuildHistogramLeaderColumnFlags_DateMode |
                                                     PTBD_BuildHistogramLeaderColumnFlags_NewTimesheetButton,
                                                     S8("date column"),
                                                     timesheets,
                                                     (DB_Cache){0},
                                                     index,
                                                     window,
                                                     selection,
                                                     &view->last_selected_id,
                                                     view->height_per_row,
                                                     padding,
                                                     view->scroll.y,
                                                     m2c);
            
            UI_Column()
            {
                PTBD_BuildHistogramInnerFlags flags =
                (PTBD_BuildHistogramInnerFlags_JobInfoInBars |
                 (PTBD_BuildHistogramInnerFlags_AnimateSmoothBars*(!hours_scale.is_dragging)));
                
                UI_SetNextFgCol(ColFromHex(0xb48eadff));
                UI_SetNextStrokeWidth(2.0f*container->user_t);
                inner = PTBD_BuildHistogramInner(S8("inner"), timesheets, jobs, employees, index, window, selection, flags, view, padding, m2c, inspectors);
                
                UI_SetNextHeight(UI_SizePx(12.0f, 1.0f));
                UI_Use scroll_bar = PTBD_BuildScrollBarForHistogram(S8("h scroll"), &view->scroll.x, inner->overflow.x, inner->computed_size.x);
                if(scroll_bar.is_edited)
                {
                    inner->view_offset.x = inner->target_view_offset.x;
                }
            }
            UI_SetNextWidth(UI_SizePx(12.0f, 1.0f));
            UI_Use scroll_bar = PTBD_BuildScrollBarForHistogram(S8("v scroll"), &view->scroll.y, inner->overflow.y, inner->computed_size.y);
            if(scroll_bar.is_edited)
            {
                inner->view_offset.y = inner->target_view_offset.y;
                leader->view_offset.y = leader->target_view_offset.y;
            }
        }
    }
    
    UI_Use use = UI_UseFromNode(container);
    view->scroll = Clamp2F(Add2F(view->scroll, Scale2F(use.scroll_delta, UI_ScrollSpeed)), Scale2F(inner->overflow, -1.0f), U2F(0.0f));
    
    if(view->is_dragging_selection_rect)
    {
        G_DoNotWaitForEventsUntil(OS_TimeInMicroseconds() + 500000ULL);
    }
    view->is_dragging_selection_rect = False;
    
    if(use.is_dragging)
    {
        V2F a = use.initial_mouse_position;
        V2F b = G_WindowMousePositionGet(ui->window);
        view->selection_rect = Range2F(Mins2F(a, b), Maxs2F(a, b));
        view->is_dragging_selection_rect = True;
        
        UI_SetNextParent(UI_OverlayFromString(S8("selection rect")));
        UI_SetNextFixedRect(view->selection_rect);
        UI_SetNextWidth(UI_SizeNone());
        UI_SetNextHeight(UI_SizeNone());
        UI_SetNextFgCol(ColFromHex(0x81a1c1aa));
        UI_SetNextBgCol(ColFromHex(0x81a1c188));
        UI_Node *selection_rect = UI_NodeMake(UI_Flags_DrawFill | UI_Flags_DrawStroke | UI_Flags_FixedFinalRect, S8(""));
    }
    else if(use.is_pressed)
    {
        if(UI_NavInterfaceStackMatchTop(&container->key))
        {
            PTBD_SelectionClear(selection);
            view->last_selected_id = 0;
        }
        else
        {
            UI_NavInterfaceStackPop();
            UI_NavInterfaceStackPush(&container->key);
        }
    }
    
    UI_Node *active = UI_NodeLookup(&ui->active);
    if(UI_NodeIsChildOf(active, container) &&
       UI_KeyMatch(&ui->hot, &ui->active) &&
       UI_KeyMatch(&active->nav_context.key, &container->key) &&
       !G_WindowKeyStateGet(ui->window, G_Key_MouseButtonLeft))
    {
        UI_NavInterfaceStackPop();
        UI_NavInterfaceStackPush(&container->key);
    }
    
    B32 is_top_of_interface_stack = UI_NavInterfaceStackMatchTop(&container->key);
    
    UI_Animate1F(&container->user_t, is_top_of_interface_stack, 15.0f);
    
    if(is_top_of_interface_stack)
    {
        F32 s = G_WindowScaleFactorGet(ui->window);
        
        G_EventQueue *events = G_WindowEventQueueGet(ui->window);
        if(G_EventQueueHasKeyDown(events, G_Key_Tab, 0, True))
        {
            U64 scroll_to_row_index = PTBD_HistogramNavNext(index, timesheets, &view->keyboard_focus_id);
            view->scroll.y = Clamp1F(0.5f*container->computed_size.y - view->height_per_row*s*scroll_to_row_index, -inner->overflow.y, 0.0f);
        }
        else if(G_EventQueueHasKeyDown(events, G_Key_Tab, G_ModifierKeys_Shift, True))
        {
            U64 scroll_to_row_index = PTBD_HistogramNavPrev(index, timesheets, &view->keyboard_focus_id);
            view->scroll.y = Clamp1F(0.5f*container->computed_size.y - view->height_per_row*s*scroll_to_row_index, -inner->overflow.y, 0.0f);
        }
    }
}

Function void
PTBD_BuildInvoicingHistogram(DB_Cache timesheets,
                             DB_Cache jobs,
                             DB_Cache employees,
                             PTBD_Index *index,
                             PTBD_Selection *selection,
                             PTBD_Histogram *view,
                             PTBD_MsgQueue *m2c,
                             PTBD_Inspectors *inspectors)
{
    F32 padding = 4.0f;
    
    UI_SetNextChildrenLayout(Axis2_Y);
    UI_Node *container = UI_NodeMake(UI_Flags_Pressable |
                                     UI_Flags_Scrollable |
                                     UI_Flags_DoNotMask |
                                     UI_Flags_AnimateSmoothLayout,
                                     S8("invoicing histogram"));
    
    UI_Node *inner;
    UI_Node *leader;
    
    UI_Parent(container) UI_Salt(S8("invoicing")) UI_Nav(UI_NavContext(PTBD_HistogramNav, &view->keyboard_focus_id, container->key))
    {
        PTBD_ApplyScrollToForList(DB_Table_Jobs, jobs, timesheets, index, 0, &view->scroll.y, container->computed_size.y, view->height_per_row, 1);
        PTBD_ApplyScrollToForHistogram(timesheets, index, &view->scroll.y, container->computed_size.y, view->height_per_row);
        
        UI_Window window = PTBD_UIWindowFromContainerScrollHeightPerRowAndRowsCount(container, view->height_per_row, index->records_count, view->scroll.y);
        
        UI_SetNextHeight(UI_SizePx(32.0f, 1.0f));
        UI_Use hours_scale = PTBD_BuildHistogramHoursScale(S8("hours scale"),  timesheets, index, &view->width_per_hour, view->leader_column_width, view->scroll.x);
        
        UI_Row()
        {
            UI_SetNextWidth(UI_SizePx(view->leader_column_width, 1.0f));
            leader = PTBD_BuildHistogramLeaderColumn(PTBD_BuildHistogramLeaderColumnFlags_JobMode,
                                                     S8("job column"),
                                                     timesheets,
                                                     jobs,
                                                     index,
                                                     window,
                                                     selection,
                                                     &view->last_selected_id,
                                                     view->height_per_row,
                                                     padding,
                                                     view->scroll.y,
                                                     m2c);
            
            UI_Column()
            {
                PTBD_BuildHistogramInnerFlags flags =
                (PTBD_BuildHistogramInnerFlags_AnimateSmoothBars*(!hours_scale.is_dragging));
                
                UI_SetNextFgCol(ColFromHex(0xb48eadff));
                UI_SetNextStrokeWidth(2.0f*container->user_t);
                inner = PTBD_BuildHistogramInner(S8("inner"), timesheets, jobs, employees, index, window, selection, flags, view, padding, m2c, inspectors);
                
                UI_SetNextHeight(UI_SizePx(12.0f, 1.0f));
                UI_Use scroll_bar = PTBD_BuildScrollBarForHistogram(S8("h scroll"), &view->scroll.x, inner->overflow.x, inner->computed_size.x);
                if(scroll_bar.is_edited)
                {
                    inner->view_offset.x = inner->target_view_offset.x;
                }
            }
            UI_SetNextWidth(UI_SizePx(12.0f, 1.0f));
            UI_Use scroll_bar = PTBD_BuildScrollBarForHistogram(S8("v scroll"), &view->scroll.y, inner->overflow.y, inner->computed_size.y);
            if(scroll_bar.is_edited)
            {
                inner->view_offset.y = inner->target_view_offset.y;
                leader->view_offset.y = leader->target_view_offset.y;
            }
        }
    }
    
    UI_Use use = UI_UseFromNode(container);
    view->scroll = Clamp2F(Add2F(view->scroll, Scale2F(use.scroll_delta, UI_ScrollSpeed)), Scale2F(inner->overflow, -1.0f), U2F(0.0f));
    
    if(view->is_dragging_selection_rect)
    {
        G_DoNotWaitForEventsUntil(OS_TimeInMicroseconds() + 500000ULL);
    }
    view->is_dragging_selection_rect = False;
    
    if(use.is_dragging)
    {
        V2F a = use.initial_mouse_position;
        V2F b = G_WindowMousePositionGet(ui->window);
        
        view->selection_rect = Range2F(Mins2F(a, b), Maxs2F(a, b));
        view->is_dragging_selection_rect = True;
        
        UI_SetNextParent(UI_OverlayFromString(S8("selection rect")));
        UI_SetNextFixedRect(view->selection_rect);
        UI_SetNextWidth(UI_SizeNone());
        UI_SetNextHeight(UI_SizeNone());
        UI_SetNextFgCol(ColFromHex(0x81a1c1aa));
        UI_SetNextBgCol(ColFromHex(0x81a1c188));
        UI_Node *selection_rect = UI_NodeMake(UI_Flags_DrawFill | UI_Flags_DrawStroke | UI_Flags_FixedFinalRect, S8(""));
    }
    else if(use.is_pressed)
    {
        if(UI_NavInterfaceStackMatchTop(&container->key))
        {
            PTBD_SelectionClear(selection);
            view->last_selected_id = 0;
        }
        else
        {
            UI_NavInterfaceStackPop();
            UI_NavInterfaceStackPush(&container->key);
        }
    }
    
    UI_Node *active = UI_NodeLookup(&ui->active);
    if(UI_NodeIsChildOf(active, container) &&
       UI_KeyMatch(&ui->hot, &ui->active) &&
       UI_KeyMatch(&active->nav_context.key, &container->key) &&
       !G_WindowKeyStateGet(ui->window, G_Key_MouseButtonLeft))
    {
        UI_NavInterfaceStackPop();
        UI_NavInterfaceStackPush(&container->key);
    }
    
    B32 is_top_of_interface_stack = UI_NavInterfaceStackMatchTop(&container->key);
    
    UI_Animate1F(&container->user_t, is_top_of_interface_stack, 15.0f);
    
    if(is_top_of_interface_stack)
    {
        F32 s = G_WindowScaleFactorGet(ui->window);
        
        G_EventQueue *events = G_WindowEventQueueGet(ui->window);
        if(G_EventQueueHasKeyDown(events, G_Key_Tab, 0, True))
        {
            U64 scroll_to_row_index = PTBD_HistogramNavNext(index, timesheets, &view->keyboard_focus_id);
            view->scroll.y = Clamp1F(0.5f*container->computed_size.y - view->height_per_row*s*scroll_to_row_index, -inner->overflow.y, 0.0f);
        }
        else if(G_EventQueueHasKeyDown(events, G_Key_Tab, G_ModifierKeys_Shift, True))
        {
            U64 scroll_to_row_index = PTBD_HistogramNavPrev(index, timesheets, &view->keyboard_focus_id);
            view->scroll.y = Clamp1F(0.5f*container->computed_size.y - view->height_per_row*s*scroll_to_row_index, -inner->overflow.y, 0.0f);
        }
    }
}

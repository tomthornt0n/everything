
Function void
PTBD_JobsListNavNext(PTBD_Index *index,
                     DB_Cache jobs,
                     DB_Cache timesheets,
                     PTBD_JobsListRowId *keyboard_focus_id,
                     PTBD_Selection *expanded)
{
    U64 record_index = ~((U64)0);
    {
        for(U64 ri = 0; ri < index->records_count; ri += 1)
        {
            PTBD_IndexRecord *r = &index->records[ri];
            if(jobs.rows[r->key].id == keyboard_focus_id->job_id)
            {
                record_index = ri;
                break;
            }
        }
    }
    PTBD_IndexRecord *record = (record_index < index->records_count) ? &index->records[record_index] : 0;
    
    U64 indices_index = ~((U64)0);
    if(0 != record)
    {
        for(U64 ii = 0; ii < record->indices_count; ii += 1)
        {
            if(timesheets.rows[record->indices[ii]].id == keyboard_focus_id->timesheet_id)
            {
                indices_index = ii;
                break;
            }
        }
    }
    
    B32 next_job = False;
    B32 next_timesheet = False;
    B32 first_timesheet = False;
    
    if(PTBD_SelectionHas(expanded, keyboard_focus_id->job_id) && record->indices_count > 0)
    {
        if(keyboard_focus_id->timesheet_id <= 0)
        {
            first_timesheet = True;
        }
        else
        {
            next_timesheet = True;
        }
    }
    else
    {
        next_job = True;
    }
    
    if(first_timesheet)
    {
        keyboard_focus_id->timesheet_id = timesheets.rows[record->indices[0]].id;
    }
    
    if(next_timesheet)
    {
        U64 new_indices_index = indices_index + 1;
        if(new_indices_index < record->indices_count)
        {
            keyboard_focus_id->timesheet_id = timesheets.rows[record->indices[new_indices_index]].id;
        }
        else
        {
            next_job = True;
        }
    }
    
    if(next_job)
    {
        U64 new_record_index = record_index + 1;
        if(new_record_index < index->records_count)
        {
            keyboard_focus_id->job_id = jobs.rows[index->records[new_record_index].key].id;
            keyboard_focus_id->timesheet_id = 0;
        }
    }
    
    ui->was_last_interaction_keyboard = True;
}

Function void
PTBD_JobsListNavPrev(PTBD_Index *index,
                     DB_Cache jobs,
                     DB_Cache timesheets,
                     PTBD_JobsListRowId *keyboard_focus_id,
                     PTBD_Selection *expanded)
{
    if(keyboard_focus_id->job_id <= 0)
    {
        PTBD_IndexRecord *record = &index->records[index->records_count - 1];
        DB_Row *job = &jobs.rows[record->key];
        keyboard_focus_id->job_id = job->id;
        if(PTBD_SelectionHas(expanded, job->id) && record->indices_count > 0)
        {
            DB_Row *timesheet = &timesheets.rows[record->indices[record->indices_count - 1]];
            keyboard_focus_id->timesheet_id = timesheet->id;
        }
    }
    else
    {
        U64 record_index = ~((U64)0);
        {
            for(U64 ri = 0; ri < index->records_count; ri += 1)
            {
                PTBD_IndexRecord *r = &index->records[ri];
                if(jobs.rows[r->key].id == keyboard_focus_id->job_id)
                {
                    record_index = ri;
                    break;
                }
            }
        }
        PTBD_IndexRecord *record = (record_index < index->records_count) ? &index->records[record_index] : 0;
        
        U64 indices_index = ~((U64)0);
        if(0 != record)
        {
            for(U64 ii = 0; ii < record->indices_count; ii += 1)
            {
                if(timesheets.rows[record->indices[ii]].id == keyboard_focus_id->timesheet_id)
                {
                    indices_index = ii;
                    break;
                }
            }
        }
        
        if(keyboard_focus_id->timesheet_id <= 0)
        {
            if(0 < record_index && record_index < index->records_count)
            {
                U64 new_record_index = record_index - 1;
                PTBD_IndexRecord *new_record = &index->records[new_record_index];
                
                keyboard_focus_id->job_id = jobs.rows[new_record->key].id;
                keyboard_focus_id->timesheet_id = 0;
                
                if(PTBD_SelectionHas(expanded, keyboard_focus_id->job_id) && new_record->indices_count > 0)
                {
                    keyboard_focus_id->timesheet_id = timesheets.rows[new_record->indices[new_record->indices_count - 1]].id;
                }
            }
        }
        else
        {
            if(0 < indices_index && PTBD_SelectionHas(expanded, keyboard_focus_id->job_id) && record->indices_count > 0)
            {
                U64 new_indices_index = indices_index - 1;
                keyboard_focus_id->timesheet_id = timesheets.rows[record->indices[new_indices_index]].id;
            }
            else
            {
                keyboard_focus_id->timesheet_id = 0;
            }
        }
    }
    
    ui->was_last_interaction_keyboard = True;
}

Function void
PTBD_JobsListNav(void *user_data, UI_Node *node)
{
    PTBD_JobsListRowId *keyboard_focus_id = user_data;
    if(keyboard_focus_id->job_id > 0)
    {
        ArenaTemp scratch = OS_TCtxScratchBegin(0, 0);
        
        S8 job_text = S8FromFmt(scratch.arena, "job row %lld jobs", keyboard_focus_id->job_id);
        S8 timesheet_text = S8FromFmt(scratch.arena, "job row %lld timesheets", keyboard_focus_id->timesheet_id);
        
        if((0 == keyboard_focus_id->timesheet_id && S8Match(job_text, node->text, 0)) ||
           S8Match(timesheet_text, node->text, 0))
        {
            node->flags |= UI_Flags_KeyboardFocused;
        }
        
        ArenaTempEnd(scratch);
    }
}

Function U64
PTBD_JobsListRowIndexFromId(DB_Cache jobs,
                            DB_Cache timesheets,
                            PTBD_Index *index,
                            PTBD_Selection *expanded,
                            PTBD_JobsListRowId id)
{
    B32 found_row = False;
    
    U64 row_index = 0;
    for(U64 record_index = 0; record_index < index->records_count && !found_row; record_index += 1)
    {
        PTBD_IndexRecord *record = &index->records[record_index];
        DB_Row *job = &jobs.rows[record->key];
        
        if(id.timesheet_id <= 0 && job->id == id.job_id)
        {
            found_row = True;
            break;
        }
        
        row_index += 1;
        
        if(PTBD_SelectionHas(expanded, job->id))
        {
            for(U64 indices_index = 0; indices_index < record->indices_count; indices_index += 1)
            {
                DB_Row *timesheet = &timesheets.rows[record->indices[indices_index]];
                
                if(timesheet->id == id.timesheet_id && job->id == id.job_id)
                {
                    found_row = True;
                    break;
                }
                
                row_index += 1;
            }
        }
    }
    
    U64 result = 0;
    if(found_row)
    {
        result = row_index;
    }
    
    return result;
}

Function UI_Use
PTBD_BuildJobsListRow(DB_Row *data,
                      DB_Table kind,
                      Range1U64 index_range,
                      U64 *row_index,
                      B32 is_toggled,
                      DB_Cache jobs,
                      DB_Cache invoices,
                      PTBD_Inspectors *inspectors,
                      PTBD_JobEditor *job_editor,
                      PTBD_MsgQueue *m2c)
{
    UI_Use result = {0};
    
    if(HasPoint1U64(index_range, *row_index))
    {
        ArenaTemp scratch = OS_TCtxScratchBegin(0, 0);
        
        UI_SetNextChildrenLayout(Axis2_X);
        S8 table_string = DB_S8FromTable(kind);
        
        UI_Node *row =
            UI_NodeMakeFromFmt(UI_Flags_DrawHotStyle |
                               UI_Flags_DrawActiveStyle |
                               UI_Flags_DoNotMask |
                               UI_Flags_Pressable |
                               UI_Flags_AnimateActive,
                               "job row %lld %.*s",
                               data->id, FmtS8(table_string));
        
        UI_Parent(row)
            UI_HotFgCol(ColFromHex(0x81a1c1ff))
            UI_ActiveFgCol(ColFromHex(0x88c0d0ff))
            UI_DefaultFlags(UI_Flags_AnimateInheritHot | UI_Flags_AnimateInheritActive)
        {
            UI_Width(UI_SizePx(48.0f, 1.0f))
            {
                if(DB_Table_Jobs == kind)
                {
                    UI_LabelFromIcon(is_toggled ? FONT_Icon_UpDir : FONT_Icon_DownDir);
                }
                else if(DB_Table_Timesheets == kind)
                {
                    UI_Row()
                    {
                        UI_Spacer(UI_SizePx(16.0f, 1.0f));
                        UI_LabelFromIcon(is_toggled ? FONT_Icon_Check : FONT_Icon_CheckEmpty);
                    }
                }
            }
            UI_Width(UI_SizePct(0.70f, 0.0f)) UI_ChildrenLayout(Axis2_X) UI_Parent(UI_NodeMake(0, S8("")))
            {
                if(DB_Table_Jobs == kind)
                {
                    if(data->id == job_editor->id)
                    {
                        PTBD_BuildJobEditor(jobs, job_editor, m2c);
                    }
                    else
                    {
                        UI_Width(UI_SizeFill()) UI_LabelFromFmt("%.*s - %.*s", FmtS8(data->job_number), FmtS8(data->job_name));
                        
                        S8 button_id = S8FromFmt(scratch.arena, "edit job %lld", data->id);
                        UI_SetNextFgCol(ColFromHex(0x4c566aff));
                        UI_SetNextWidth(UI_SizeFromText(8.0f, 1.0f));
                        UI_Use edit_button = UI_IconButton(button_id, FONT_Icon_Pencil);
                        if(edit_button.is_pressed)
                        {
                            UI_NodeStrings strings = UI_TextAndIdFromString(S8FromFmt(scratch.arena, "job editor %lld", data->id));
                            UI_Key key = UI_KeyFromIdAndSaltStack(strings.id, ui->salt_stack);
                            UI_NavInterfaceStackPush(&key);
                            
                            job_editor->id = data->id;
                            job_editor->number.len = 0;
                            job_editor->name.len = 0;
                            
                            S8BuilderAppend(&job_editor->number, data->job_number);
                            S8BuilderAppend(&job_editor->name, data->job_name);
                        }
                    }
                }
                else if(DB_Table_Timesheets == kind)
                {
                    UI_Width(UI_SizeFill()) UI_Label(data->description);
                    
                    UI_Width(UI_SizePx(40.0f, 1.0f)) UI_Height(UI_SizePx(40.0f, 1.0f)) UI_CornerRadius(16.0f) UI_Growth(-4.0f) UI_SaltFmt("%lld", data->id)
                    {
                        DB_Row *invoice = DB_RowFromId(invoices, data->invoice_id);
                        UI_Use use = PTBD_BuildInvoiceEmblem(invoice);
                        if(use.is_hovering && 0 != invoice)
                        {
                            PTBD_ScrollToRow(DB_Table_Invoices, data->invoice_id);
                            PTBD_ScrollToRow(DB_Table_Timesheets, data->id);
                        }
                    }
                }
            }
            
            UI_Width(UI_SizePct(0.1f, 0.0f)) 
            {
                I32 hours = Floor1F(data->hours);
                I32 minutes = 60.0f*Fract1F(data->hours);
                
                S8 hours_label = S8FromFmt(scratch.arena, "%d:%02d", hours, minutes);
                S8 miles_label = S8FromFmt(scratch.arena, "%0.1f", data->miles);
                S8 cost_label = S8FromFmt(scratch.arena, "£%0.2f", data->cost);
                
                if(DB_Table_Timesheets == kind)
                {
                    if(DB_TimesheetIsDisbursement(data))
                    {
                        hours_label = S8("-");
                        miles_label = S8("-");
                    }
                    else
                    {
                        cost_label = S8("-");
                    }
                }
                
                if(data->hours < 0.01f) UI_SetNextFgCol(ptbd_grey);
                UI_Label(hours_label);
                if(data->miles < 0.01f) UI_SetNextFgCol(ptbd_grey);
                UI_Label(miles_label);
                if(data->cost < 0.01f) UI_SetNextFgCol(ptbd_grey);
                UI_Label(cost_label);
            }
        }
        
        result = UI_UseFromNode(row);
        
        B32 is_hovering = result.is_hovering || ptbd_scroll_to[kind] == data->id;
        if(DB_Table_Timesheets == kind)
        {
            PTBD_Inspector *inspector = PTBD_InspectorFromId(inspectors, data->id);
            is_hovering = is_hovering || (0 == inspector ? 0 : inspector->should_write);
        }
        UI_Animate1F(&row->hot_t, is_hovering, 15.0f);
        
        ArenaTempEnd(scratch);
    }
    *row_index += 1;
    
    return result;
}

Function UI_Use
PTBD_BuildJobNumberEditor(DB_Cache jobs, PTBD_JobEditor *job_editor)
{
    ArenaTemp scratch = OS_TCtxScratchBegin(0, 0);
    
    UI_SetNextWidth(UI_SizePx(150.0f, 0.5f));
    UI_SetNextCornerRadius(14.0f);
    UI_SetNextGrowth(-4.0f);
    UI_SetNextFgCol(ptbd_grey);
    UI_Use result = UI_LineEdit(S8FromFmt(scratch.arena, "job %lld number", job_editor->id), &job_editor->number);
    
    if(0 == job_editor->number.len)
    {
        OS_TCtxErrorPush(S8("The job number cannot be left blank."));
        result.node->fg = ColFromHex(0xbf616aff);
        result.node->hot_fg = ColFromHex(0xbf616aff);
    }
    else
    {
        for(U64 row_index = 0; row_index < jobs.rows_count; row_index += 1)
        {
            S8 number = S8FromBuilder(job_editor->number);
            if(S8Match(number, jobs.rows[row_index].job_number, 0) && jobs.rows[row_index].id != job_editor->id)
            {
                OS_TCtxErrorPushFmt("There is already a job with the number '%.*s'.", FmtS8(number));
                result.node->fg = ColFromHex(0xbf616aff);
                result.node->hot_fg = ColFromHex(0xbf616aff);
                break;
            }
        }
    }
    
    ArenaTempEnd(scratch);
    
    return result;
}

Function UI_Use
PTBD_BuildJobNameEditor(PTBD_JobEditor *job_editor)
{
    ArenaTemp scratch = OS_TCtxScratchBegin(0, 0);
    
    UI_SetNextWidth(UI_SizeFill());
    UI_SetNextCornerRadius(14.0f);
    UI_SetNextGrowth(-4.0f);
    UI_SetNextFgCol(ptbd_grey);
    UI_Use result = UI_LineEdit(S8FromFmt(scratch.arena, "job %lld name", job_editor->id), &job_editor->name);
    
    if(0 == job_editor->name.len)
    {
        OS_TCtxErrorPush(S8("The job name cannot be left blank."));
        result.node->fg = ColFromHex(0xbf616aff);
        result.node->hot_fg = ColFromHex(0xbf616aff);
    }
    
    ArenaTempEnd(scratch);
    
    return result;
}

Function void
PTBD_BuildJobEditor(DB_Cache jobs, PTBD_JobEditor *job_editor, PTBD_MsgQueue *m2c)
{
    ArenaTemp scratch = OS_TCtxScratchBegin(0, 0);
    
    B32 should_close = False;
    B32 should_save = False;
    
    OS_TCtxErrorScopePush();
    
    UI_Font(ui_font)
    {
        UI_SetNextStrokeWidth(2.0f);
        UI_SetNextCornerRadius(18.0f);
        UI_SetNextChildrenLayout(Axis2_X);
        UI_Node *editor =
            UI_NodeMakeFromFmt(UI_Flags_DrawFill |
                               UI_Flags_DrawStroke |
                               UI_Flags_DrawDropShadow |
                               UI_Flags_AnimateSmoothLayout |
                               UI_Flags_AnimateIn,
                               "job editor %lld", 
                               job_editor->id);
        
        if(editor->is_new)
        {
            editor->flags |= UI_Flags_Hidden;
            editor->size[Axis2_X] = UI_SizeNone();
            editor->size[Axis2_Y] = UI_SizeNone();
        }
        
        UI_Parent(editor) UI_Nav(UI_NavContext(&UI_NavDefault, editor, editor->key))
            UI_Pad(UI_SizePx(4.0f, 1.0f))
        {
            
            UI_Width(UI_SizeFromText(4.0f, 1.0f)) UI_Label(S8("number:"));
            UI_Use number_use = PTBD_BuildJobNumberEditor(jobs, job_editor);
            if(editor->is_new)
            {
                editor->nav_default_keyboard_focus = number_use.node->key;
            }
            
            UI_Width(UI_SizeFromText(4.0f, 1.0f)) UI_Label(S8("name:"));
            PTBD_BuildJobNameEditor(job_editor);
            
            UI_Spacer(UI_SizePx(8.0f, 1.0f));
            
            UI_Width(UI_SizePx(24.0f, 1.0f))
            {
                S8 save_button_id = S8FromFmt(scratch.arena, "job %lld save", job_editor->id);
                if(UI_IconButton(save_button_id, FONT_Icon_OkCircled).is_pressed)
                {
                    should_save = True;
                }
                S8 cancel_button_id = S8FromFmt(scratch.arena, "job %lld cancel", job_editor->id);
                if(UI_IconButton(cancel_button_id, FONT_Icon_CancelCircled).is_pressed)
                {
                    should_close = True;
                }
            }
        }
        
        UI_Node *active = UI_NodeLookup(&ui->active);
        if(UI_NodeIsChildOf(active, editor) &&
           UI_KeyMatch(&ui->hot, &ui->active) && 
           UI_KeyMatch(&active->nav_context.key, &editor->key) &&
           !G_WindowKeyStateGet(ui->window, G_Key_MouseButtonLeft))
        {
            UI_NavInterfaceStackPop();
            UI_NavInterfaceStackPush(&editor->key);
        }
        
        B32 is_top_of_interface_stack = UI_NavInterfaceStackMatchTop(&editor->key);
        
        UI_Animate1F(&editor->user_t, is_top_of_interface_stack, 15.0f);
        editor->fg = ColMix(ptbd_grey, ColFromHex(0xb48eadff), editor->user_t);
        
        if(is_top_of_interface_stack)
        {
            G_EventQueue *events = G_WindowEventQueueGet(ui->window);
            if(G_EventQueueHasKeyDown(events, G_Key_Tab, 0, True))
            {
                UI_NavDefaultNext(editor);
            }
            else if(G_EventQueueHasKeyDown(events, G_Key_Tab, G_ModifierKeys_Shift, True))
            {
                UI_NavDefaultPrev(editor);
            }
            else if(G_EventQueueHasKeyDown(events, G_Key_Esc, 0, True))
            {
                should_close = True;
            }
            else if(G_EventQueueHasKeyDown(events, G_Key_S, G_ModifierKeys_Ctrl, True))
            {
                should_save = True;
            }
            
            if(job_editor->id < 0 || should_close)
            {
                UI_NavInterfaceStackPop();
            }
        }
        
        if(should_save)
        {
            if(0 == OS_TCtxErrorScopePropogate())
            {
                should_close = True;
                
                S8 job_number = S8FromBuilder(job_editor->number);
                S8 job_name = S8FromBuilder(job_editor->name);
                
                M2C_WriteJob(m2c, job_editor->id, job_number, job_name);
                M2C_Reload(m2c);
            }
        }
        else
        {
            OS_TCtxErrorScopePop(0);
        }
        
        if(should_close)
        {
            job_editor->number.len = 0;
            job_editor->name.len = 0;
            job_editor->id = -1;
        }
    }
    
    ArenaTempEnd(scratch);
}

Function void
PTBD_BuildJobsList(DB_Cache jobs,
                   DB_Cache timesheets,
                   DB_Cache invoices,
                   PTBD_Index *index,
                   PTBD_Selection *selection,
                   PTBD_Selection *expanded,
                   I64 *last_selected_id,
                   PTBD_JobsListRowId *keyboard_focus_id,
                   PTBD_Inspectors *inspectors,
                   PTBD_JobEditor *job_editor,
                   PTBD_MsgQueue *m2c)
{
    UI_SetNextChildrenLayout(Axis2_X);
    UI_Node *container = UI_NodeMake(UI_Flags_Pressable | UI_Flags_DoNotMask | UI_Flags_AnimateSmoothLayout, S8("jobs lister"));
    
    U64 rows_count = index->records_count;
    for(U64 record_index = 0; record_index < index->records_count; record_index += 1)
    {
        PTBD_IndexRecord *record = &index->records[record_index];
        DB_Row *job = &jobs.rows[record->key];
        if(PTBD_SelectionHas(expanded, job->id))
        {
            rows_count += Max1U64(record->indices_count, 1);
        }
    }
    
    UI_Node *scroll_region = 0;
    
    F32 s = G_WindowScaleFactorGet(ui->window);
    
    F32 height_per_row = 40.0f;
    
    UI_Parent(container) UI_Salt(S8("jobs")) UI_Nav(UI_NavContext(PTBD_JobsListNav, keyboard_focus_id, container->key))
        UI_Width(UI_SizeFill())
        UI_Height(UI_SizeFill())
    {
        UI_Column()
        {
            UI_SetNextCornerRadius(18.0f);
            UI_SetNextFgCol(ptbd_grey);
            UI_SetNextDefaultFlags(UI_Flags_DrawFill | UI_Flags_DrawStroke | UI_Flags_DrawDropShadow);
            UI_SetNextHeight(UI_SizePx(40.0f, 1.0f));
            UI_Row() UI_Font(ui_font_bold)
            {
                if(0 != job_editor->id)
                    UI_Width(UI_SizePx(48.0f, 1.0f))
                {
                    if(UI_IconButton(S8("new job"), FONT_Icon_PlusCircled).is_pressed)
                    {
                        UI_NodeStrings strings = UI_TextAndIdFromString(S8("job editor 0"));
                        UI_Key key = UI_KeyFromIdAndSaltStack(strings.id, ui->salt_stack);
                        UI_NavInterfaceStackPush(&key);
                        
                        job_editor->id = 0;
                        job_editor->number.len = 0;
                        job_editor->name.len = 0;
                    }
                }
                
                if(0 == job_editor->id)
                {
                    UI_Width(UI_SizePx(600.0f, 0.75f)) PTBD_BuildJobEditor(jobs, job_editor, m2c);
                }
                
                UI_Spacer(UI_SizePct(0.7f, 0.0f));
                UI_Width(UI_SizePct(0.1f, 0.0f)) UI_LabelFromIcon(FONT_Icon_Clock);
                UI_Width(UI_SizePct(0.1f, 0.0f)) UI_LabelFromIcon(FONT_Icon_Cab);
                UI_Width(UI_SizePct(0.1f, 0.0f)) UI_LabelFromIcon(FONT_Icon_Pound);
            }
            
            UI_Spacer(UI_SizePx(4.0f, 1.0f));
            
            UI_SetNextChildrenLayout(Axis2_Y);
            UI_SetNextFgCol(ColFromHex(0xb48eadff));
            UI_SetNextStrokeWidth(2.0f*container->user_t);
            scroll_region = UI_NodeMake(UI_Flags_DrawStroke |
                                        UI_Flags_Scrollable |
                                        UI_Flags_ScrollChildrenY |
                                        UI_Flags_OverflowY |
                                        UI_Flags_ClampViewOffsetToOverflow |
                                        UI_Flags_AnimateSmoothScroll,
                                        S8("scroll region"));
            
            PTBD_ApplyScrollToForList(DB_Table_Jobs, jobs, timesheets, index, expanded, &scroll_region->target_view_offset.y, scroll_region->computed_size.y, height_per_row, 1);
        }
        
        UI_Width(UI_SizePx(12.0f, 1.0f))
            UI_ScrollBar(S8("scroll bar"), scroll_region, Axis2_Y);
        
        UI_Window window =
            PTBD_UIWindowFromContainerScrollHeightPerRowAndRowsCount(scroll_region,
                                                                     height_per_row,
                                                                     rows_count,
                                                                     scroll_region->target_view_offset.y);
        
        UI_UseFromNode(scroll_region);
        
        UI_Parent(scroll_region)
            UI_Height(UI_SizePx(height_per_row, 1.0f))
        {
            U64 row_index = 0;
            
            UI_Spacer(UI_SizePx(window.space_before, 1.0f));
            
            for(U64 record_index = 0; record_index < index->records_count; record_index += 1)
            {
                PTBD_IndexRecord *record = &index->records[record_index];
                DB_Row *job = &jobs.rows[record->key];
                
                B32 is_expanded = PTBD_SelectionHas(expanded, job->id);
                
                B32 should_drop = False;
                
                UI_Use job_row = PTBD_BuildJobsListRow(job, DB_Table_Jobs, window.index_range, &row_index, is_expanded, jobs, (DB_Cache){0}, 0, job_editor, m2c);
                if(job_row.is_pressed)
                {
                    PTBD_SelectionSet(OS_TCtxGet()->permanent_arena, expanded, job->id, !is_expanded);
                    
                    UI_NavInterfaceStackPop();
                    UI_NavInterfaceStackPush(&container->key);
                    keyboard_focus_id->job_id = job->id;
                    keyboard_focus_id->timesheet_id = 0;
                }
                else if(job_row.is_dragging && 0 == ptbd_drag_drop.count)
                {
                    PTBD_DragRow(DB_Table_Jobs, *job);
                }
                else if(job_row.is_left_up && ptbd_drag_drop.count > 0)
                {
                    should_drop = True;
                }
                
                if(is_expanded)
                {
                    UI_SetNextHeight(UI_SizeSum(1.0f));
                    UI_SetNextGrowth(-2.0f);
                    UI_SetNextFgCol(U4F(0.0f));
                    UI_SetNextBgCol(ptbd_grey);
                    UI_SetNextTexture(ptbd_gradient_vertical);
                    UI_SetNextTextureRegion(Range2F(V2F(0.0f, 0.15f), V2F(1.0f, 0.95f)));
                    UI_Node *children_container =
                        UI_NodeMake(UI_Flags_DrawFill |
                                    UI_Flags_DrawDropShadow |
                                    UI_Flags_DrawHotStyle |
                                    UI_Flags_DrawActiveStyle |
                                    UI_Flags_DoNotMask,
                                    S8(""));
                    if(0 != job_row.node)
                    {
                        children_container->hot_t = job_row.node->hot_t;
                        children_container->active_t = job_row.node->active_t;
                    }
                    
                    UI_Parent(children_container)
                        UI_FgCol(ColMix(ColFromHex(0x4c566aff),
                                        ColFromHex(0xd8dee9bb),
                                        ptbd_is_dark_mode_t))
                    {
                        if(HasPoint1U64(window.index_range, row_index))
                        {
                            UI_SetNextFgCol(ColMix(ColFromHex(0x4c566aff), ColFromHex(0x81a1c1ff), job_row.node == 0 ? 0 : job_row.node->hot_t));
                            UI_SetNextGrowth(1.0f);
                            UI_SetNextHeight(UI_SizeNone());
                            UI_NodeMake(UI_Flags_DrawStroke, S8(""));
                        }
                        
                        for(U64 timesheet_index = 0; timesheet_index < record->indices_count; timesheet_index += 1)
                        {
                            DB_Row *timesheet = &timesheets.rows[record->indices[timesheet_index]];
                            
                            B32 is_selected = PTBD_SelectionHas(selection, timesheet->id);
                            
                            UI_Use timesheet_row = PTBD_BuildJobsListRow(timesheet, DB_Table_Timesheets, window.index_range, &row_index, is_selected, (DB_Cache){0}, invoices, inspectors, 0, m2c);
                            if(timesheet_row.is_pressed)
                            {
                                Arena *permanent_arena = OS_TCtxGet()->permanent_arena;
                                
                                if(G_WindowKeyStateGet(ui->window, G_Key_Shift) && *last_selected_id > 0)
                                {
                                    PTBD_SelectionClear(selection);
                                    
                                    PTBD_IndexCoord start_coord = PTBD_IndexCoordFromDBId(index, timesheets, *last_selected_id);
                                    PTBD_IndexCoord target_coord = PTBD_IndexCoordFromDBId(index, timesheets, timesheet->id);
                                    
                                    PTBD_SelectionSet(permanent_arena, selection, *last_selected_id, True);
                                    
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
                                        
                                        DB_Row *job = &jobs.rows[index->records[coord.record].key];
                                        
                                        if(PTBD_SelectionHas(expanded, job->id))
                                        {
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
                                        else
                                        {
                                            if(target_coord.record < start_coord.record)
                                            {
                                                coord.record -= 1;
                                            }
                                            else if(target_coord.record > start_coord.record)
                                            {
                                                coord.record += 1;
                                            }
                                        }
                                    }
                                }
                                else
                                {
                                    PTBD_SelectionSet(permanent_arena, selection, timesheet->id, !is_selected);
                                    if(!is_selected)
                                    {
                                        *last_selected_id = timesheet->id;
                                    }
                                }
                                
                                UI_NavInterfaceStackPop();
                                UI_NavInterfaceStackPush(&container->key);
                                keyboard_focus_id->job_id = job->id;
                                keyboard_focus_id->timesheet_id = timesheet->id;
                            }
                            else if(timesheet_row.is_dragging && 0 == ptbd_drag_drop.count)
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
                            else if(timesheet_row.is_left_up && ptbd_drag_drop.count > 0)
                            {
                                should_drop = True;
                            }
                            
                            if(timesheet_row.is_hovering && ptbd_drag_drop.count > 0)
                            {
                                job_row.node->flags &= ~UI_Flags_AnimateHot;
                                job_row.node->hot_t = Max1F(job_row.node->hot_t, timesheet_row.node->hot_t);
                            }
                        }
                        
                        if(0 == record->indices_count)
                        {
                            if(HasPoint1U64(window.index_range, row_index))
                                UI_Row() UI_Width(UI_SizeFromText(8.0f, 1.0f)) UI_Pad(UI_SizeFill()) UI_FgCol(ColFromHex(0xbf616aff))
                            {
                                UI_Label(S8("No timesheets for this job."));
                                
                                UI_Spacer(UI_SizePx(8.0f, 1.0f));
                                
                                UI_SetNextFgCol(ptbd_grey);
                                UI_SetNextHotBgCol(ColFromHex(0x81a1c1ff));
                                UI_SetNextActiveBgCol(ColFromHex(0x88c0d0ff));
                                UI_SetNextWidth(UI_SizeSum(1.0f));
                                UI_SetNextGrowth(-4.0f);
                                UI_Node *button = UI_NodeMakeFromFmt(UI_Flags_DrawFill |
                                                                     UI_Flags_DrawStroke |
                                                                     UI_Flags_DrawDropShadow |
                                                                     UI_Flags_DrawHotStyle |
                                                                     UI_Flags_DrawActiveStyle |
                                                                     UI_Flags_AnimateHot |
                                                                     UI_Flags_AnimateActive |
                                                                     UI_Flags_Pressable,
                                                                     "Add one %lld", job->id);
                                UI_Parent(button)
                                {
                                    UI_Width(UI_SizeFromText(12.0f, 1.0f))
                                        UI_DefaultFlags(UI_Flags_AnimateInheritHot | UI_Flags_AnimateInheritActive)
                                        UI_HotFgCol(ColFromHex(0xd8dee9ff))
                                        UI_ActiveFgCol(ColFromHex(0xd8dee9ff))
                                        UI_Label(S8("Add one"));
                                }
                                if(UI_UseFromNode(button).is_pressed)
                                {
                                    M2C_WriteTimesheet(m2c, 0, DenseTimeFromDateTime(OS_NowLTC()), 0, 0, 4.0f, 0.0f, -1.0f, S8("description"), 0);
                                    M2C_Reload(m2c);
                                }
                            }
                            row_index += 1;
                        }
                    }
                }
                
                if(should_drop)
                {
                    if(DB_Table_Timesheets == ptbd_drag_drop.kind)
                    {
                        M2C_UndoContinuationBegin(m2c);
                        for(U64 payload_index = 0; payload_index < ptbd_drag_drop.count; payload_index += 1)
                        {
                            DB_Row *timesheet = &ptbd_drag_drop.data[payload_index];
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
                        }
                        M2C_UndoContinuationEnd(m2c);
                        M2C_Reload(m2c);
                    }
                    else
                    {
                        OS_TCtxErrorPush(S8("Drag timesheets here to move them to this job."));
                    }
                }
            }
            
            UI_Spacer(UI_SizePx(window.space_after, 1.0f));
        }
    }
    
    UI_Use use = UI_UseFromNode(container);
    
    if(use.is_pressed)
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
        if(G_EventQueueHasKeyDown(events, G_Key_Tab, 0, True) ||
           G_EventQueueHasKeyDown(events, G_Key_Down, 0, True))
        {
            PTBD_JobsListNavNext(index, jobs, timesheets, keyboard_focus_id, expanded);
            U64 scroll_to_row_index = PTBD_JobsListRowIndexFromId(jobs, timesheets, index, expanded, *keyboard_focus_id);
            scroll_region->target_view_offset.y =
                0.5f*scroll_region->computed_size.y - (scroll_to_row_index + 0.5f)*height_per_row*s;
        }
        else if(G_EventQueueHasKeyDown(events, G_Key_Tab, G_ModifierKeys_Shift, True) ||
                G_EventQueueHasKeyDown(events, G_Key_Up, 0, True))
        {
            PTBD_JobsListNavPrev(index, jobs, timesheets, keyboard_focus_id, expanded);
            U64 scroll_to_row_index = PTBD_JobsListRowIndexFromId(jobs, timesheets, index, expanded, *keyboard_focus_id);
            scroll_region->target_view_offset.y =
                0.5f*scroll_region->computed_size.y - (scroll_to_row_index + 0.5f)*height_per_row*s;
        }
    }
}

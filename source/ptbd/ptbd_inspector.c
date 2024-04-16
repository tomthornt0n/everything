
Function void
PTBD_InspectorFieldsFromTimesheet(PTBD_Inspector *inspector, DB_Row *timesheet)
{
    if(0 != timesheet && 0 != inspector)
    {
        inspector->invoice_id = timesheet->invoice_id;
        
        DateTime date = DateTimeFromDenseTime(timesheet->date);
        
        for(PTBD_InspectorColumn column_index = 0; column_index < PTBD_InspectorColumn_MAX; column_index += 1)
        {
            inspector->fields[column_index].len = 0;
        }
        
        F32 hours_and_minutes = Max1F(timesheet->hours, 0.0f);
        F32 hours = Floor1F(hours_and_minutes);
        F32 minutes = 60.0f*Fract1F(hours_and_minutes);
        
        S8BuilderAppend(&inspector->fields[PTBD_InspectorColumn_Description], timesheet->description);
        S8BuilderAppendFmt(&inspector->fields[PTBD_InspectorColumn_Hours], "%.0f:%02.0f", hours, minutes);
        S8BuilderAppendFmt(&inspector->fields[PTBD_InspectorColumn_Miles], "%.2f", Max1F(timesheet->miles, 0.0f));
        S8BuilderAppendFmt(&inspector->fields[PTBD_InspectorColumn_Cost], "%.2f", Max1F(timesheet->cost, 0.0f));
        S8BuilderAppendFmt(&inspector->fields[PTBD_InspectorColumn_Date], "%02d/%02d/%d", date.day + 1, date.mon + 1, date.year);
        S8BuilderAppend(&inspector->fields[PTBD_InspectorColumn_Employee], timesheet->employee_name);
        S8BuilderAppend(&inspector->fields[PTBD_InspectorColumn_Job], timesheet->job_number);
    }
}

Function PTBD_Inspector *
PTBD_InspectorFromId(PTBD_Inspectors *inspectors, I64 id)
{
    PTBD_Inspector *result = 0;
    for(PTBD_Inspector *inspector = inspectors->first; 0 != inspector; inspector = inspector->next)
    {
        if(inspector->id == id)
        {
            result = inspector;
            break;
        }
    }
    return result;
}

Function void
PTBD_RequireInspectorForTimesheet(Arena *arena, DB_Cache timesheets, PTBD_Inspectors *inspectors, I64 id)
{
    DB_Row *timesheet = DB_RowFromId(timesheets, id);
    if(id > 0 && 0 != timesheet)
    {
        PTBD_Inspector *inspector = PTBD_InspectorFromId(inspectors, id);
        
        if(0 == inspector)
        {
            inspector = inspectors->free_list;
            if(0 == inspector)
            {
                inspector = PushArray(arena, PTBD_Inspector, 1);
                for(PTBD_InspectorColumn column_index = 0; column_index < PTBD_InspectorColumn_MAX; column_index += 1)
                {
                    inspector->fields[column_index].cap = ptbd_inspector_columns[column_index].line_edit_cap;
                    inspector->fields[column_index].buffer = PushArray(arena, char, inspector->fields[column_index].cap);
                }
            }
            else
            {
                inspectors->free_list = inspectors->free_list->next;
            }
            
            inspector->next = 0;
            if(0 == inspectors->last)
            {
                inspectors->first = inspector;
            }
            else
            {
                inspectors->last->next = inspector;
            }
            inspectors->last = inspector;
            inspectors->count += 1;
            
            PTBD_InspectorFieldsFromTimesheet(inspector, timesheet);
            
            inspectors->grab_keyboard_focus = timesheet->id;
        }
        
        inspector->id = id;
        inspector->was_touched = True;
    }
}

Function void
PTBD_UpdateInspectors(PTBD_Inspectors *inspectors, DB_Cache timesheets)
{
    inspectors->touched_count = 0;
    
    PTBD_Inspector **prev_indirect = &inspectors->first;
    PTBD_Inspector *next = 0;
    PTBD_Inspector *last = inspectors->first;
    for(PTBD_Inspector *inspector = inspectors->first; 0 != inspector; inspector = next)
    {
        if(inspector->was_touched)
        {
            inspectors->touched_count += 1;
        }
        
        UI_Animate1F(&inspector->spawn_t, inspector->was_touched, 15.0f);
        inspector->was_touched = False;
        
        next = inspector->next;
        if(inspector->spawn_t < 0.1f || 0 == DB_RowFromId(timesheets, inspector->id))
        {
            inspector->next = inspectors->free_list;
            inspectors->free_list = inspector;
            *prev_indirect = next;
            inspectors->count -= 1;
        }
        else
        {
            prev_indirect = &inspector->next;
        }
        last = inspector;
    }
    inspectors->last = last;
}

Function F64
PTBD_InspectorValueFromNumericField(S8Builder field)
{
    S8 string = S8FromBuilder(field);
    
    S8Split split_on_colon = S8SplitMake(string, S8(":"), 0);
    S8SplitNext(&split_on_colon);
    S8 before_colon = split_on_colon.current;
    S8SplitNext(&split_on_colon);
    S8 after_colon = split_on_colon.current;
    
    F64 result = S8Parse1F64(before_colon);
    if(after_colon.len > 0)
    {
        result += S8Parse1F64(after_colon) / 60.0;
    }
    
    return result;
}

Function void
PTBD_PushWriteRecordCmdFromInspector(PTBD_MsgQueue *m2c, PTBD_Inspector *inspector, DB_Cache jobs, DB_Cache timesheets, DB_Cache employees)
{
    inspector->should_write = False;
    
    DenseTime date = DenseTimeFromDateTime(DateTimeFromDDMMYYYYString(S8FromBuilder(inspector->fields[PTBD_InspectorColumn_Date]), OS_NowLTC().year));
    S8 employee_name = S8FromBuilder(inspector->fields[PTBD_InspectorColumn_Employee]);
    S8 job_number = S8FromBuilder(inspector->fields[PTBD_InspectorColumn_Job]);
    I64 invoice_id = DB_RowFromId(timesheets, inspector->id)->invoice_id;
    F64 hours = PTBD_InspectorValueFromNumericField(inspector->fields[PTBD_InspectorColumn_Hours]);
    F64 miles = PTBD_InspectorValueFromNumericField(inspector->fields[PTBD_InspectorColumn_Miles]);
    F64 cost = PTBD_InspectorValueFromNumericField(inspector->fields[PTBD_InspectorColumn_Cost]);
    S8 description = S8FromBuilder(inspector->fields[PTBD_InspectorColumn_Description]);
    
    I64 job_id = 0;
    for(U64 row_index = 0; row_index < jobs.rows_count; row_index += 1)
    {
        if(S8Match(job_number, jobs.rows[row_index].job_number, 0))
        {
            job_id = jobs.rows[row_index].id;
            break;
        }
    }
    
    if(!IsNaN1F(cost) && cost > 0)
    {
        M2C_WriteTimesheet(m2c,
                           inspector->id,
                           date,
                           0,
                           job_id,
                           0.0,
                           0.0,
                           cost,
                           description,
                           invoice_id);
    }
    else
    {
        I64 employee_id = 0;
        for(U64 row_index = 0; row_index < employees.rows_count; row_index += 1)
        {
            if(S8Match(employee_name, employees.rows[row_index].employee_name, 0))
            {
                employee_id = employees.rows[row_index].id;
                break;
            }
        }
        
        M2C_WriteTimesheet(m2c,
                           inspector->id,
                           date,
                           employee_id,
                           job_id,
                           hours,
                           miles,
                           -1.0,
                           description,
                           invoice_id);
    }
}

Function UI_Use
PTBD_BuildInspectorField(PTBD_Inspector *inspector, PTBD_InspectorColumn field_index, U64 *last_edit_time, S8List valid_strings)
{
    ArenaTemp scratch = OS_TCtxScratchBegin(0, 0);
    
    OS_TCtxErrorScopePush();
    
    Range1F range = ptbd_inspector_columns[field_index].range;
    B32 is_numeric = (!IsNaN1F(range.min) && !IsNaN1F(range.max));
    
    UI_SetNextCornerRadius(14.0f);
    if(is_numeric)
    {
        UI_SetNextDefaultFlags(UI_Flags_Scrollable);
    }
    S8 string = S8FromFmt(scratch.arena, "inspector %lld %.*s editor", inspector->id, FmtS8(ptbd_inspector_columns[field_index].title));
    UI_Use use = UI_LineEdit(string, &inspector->fields[field_index]);
    
    if(use.is_left_up && ptbd_drag_drop.count > 0)
    {
        DB_Row *row = &ptbd_drag_drop.data[0];
        
        if(PTBD_InspectorColumn_Job == field_index && row->job_number.len > 0)
        {
            inspector->fields[field_index].len = 0;
            S8BuilderAppend(&inspector->fields[field_index], row->job_number);
            use.is_edited = True;
        }
        else if(PTBD_InspectorColumn_Employee == field_index && row->employee_name.len > 0)
        {
            inspector->fields[field_index].len = 0;
            S8BuilderAppend(&inspector->fields[field_index], row->employee_name);
            use.is_edited = True;
        }
    }
    
    S8 value = S8FromBuilder(inspector->fields[field_index]);
    
    if(valid_strings.count > 0)
    {
        B32 has_string = False;
        for(S8ListForEach(valid_strings, s))
        {
            S8Split split = S8SplitMake(s->string, S8("\x1E"), 0);
            S8SplitNext(&split);
            if(S8Match(split.current, value, 0))
            {
                has_string = True;
                break;
            }
        }
        if(!has_string)
        {
            OS_TCtxErrorPushFmt("%.*s is not valid.", FmtS8(ptbd_inspector_columns[field_index].title));
        }
    }
    
    if(is_numeric)
    {
        F64 v = PTBD_InspectorValueFromNumericField(inspector->fields[field_index]);
        
        if(!HasPoint1F(range, v))
        {
            OS_TCtxErrorPushFmt("%.*s must be in the range %.1f - %.0f.", FmtS8(ptbd_inspector_columns[field_index].title), FmtV2F(range.v));
        }
        else if(use.scroll_delta.y != 0)
        {
            F64 new_v = Clamp1F(v + use.scroll_delta.y*0.01f*(range.max - range.min), range.min, range.max);
            inspector->fields[field_index].len = 0;
            if(PTBD_InspectorColumn_Hours == field_index)
            {
                F32 hours = Floor1F(new_v);
                F32 minutes = Round1F(12.0f*Fract1F(new_v))*5.0f;
                S8BuilderAppendFmt(&inspector->fields[field_index], "%.0f:%02.0f", hours, minutes);
            }
            else
            {
                S8BuilderAppendFmt(&inspector->fields[field_index], "%.2f", new_v);
            }
            use.is_edited = True;
        }
    }
    
    if(PTBD_InspectorColumn_Date == field_index)
    {
        DateTime today = OS_NowLTC();
        DateTime date = DateTimeFromDDMMYYYYString(value, today.year);
        DenseTime dense_date = DenseTimeFromDateTime(date);
        if(0 == dense_date)
        {
            OS_TCtxErrorPush(S8("You must enter a valid date."));
        }
        if((dense_date > DenseTimeFromDateTime(today)))
        {
            OS_TCtxErrorPushFmt("%02d/%02d/%d is in the future.", date.day + 1, date.mon + 1, date.year);
        }
    }
    
    if(valid_strings.count > 0 && (use.node->flags & UI_Flags_KeyboardFocused))
        UI_Parent(UI_OverlayFromString(S8("autocomplete")))
    {
        F32 scale = G_WindowScaleFactorGet(ui->window);
        
        S8List filtered_strings = {0};
        F32 max_width = 0.0f;
        for(S8ListForEach(valid_strings, s))
        {
            S8Split split = S8SplitMake(s->string, S8("\x1E"), 0);
            S8 string = S8SplitNext(&split) ? split.current : S8("");
            S8 desc = S8SplitNext(&split) ? split.current : S8("");
            
            if((0 == value.len ||
                S8HasSubstring(string, value, MatchFlags_CaseInsensitive) ||
                S8HasSubstring(desc, value, MatchFlags_CaseInsensitive)) &&
               !S8Match(string, value, 0))
            {
                S8ListAppend(scratch.arena, &filtered_strings, s->string);
                
                ArenaTemp checkpoint = ArenaTempBegin(scratch.arena);
                FONT_PreparedText *mt_1 = UI_PreparedTextFromS8(UI_FontPeek(), UI_FontSizePeek()*scale, string);
                FONT_PreparedText *mt_2 = UI_PreparedTextFromS8(UI_FontPeek(), UI_FontSizePeek()*scale, desc);
                max_width = Max1F(max_width, mt_1->bounds.max.x - mt_1->bounds.min.x + mt_2->bounds.max.x - mt_2->bounds.min.x);
                ArenaTempEnd(checkpoint);
            }
        }
        
        if(filtered_strings.count > 0)
        {
            F32 height_per_row = 24.0f;
            F32 padding = 4.0f;
            
            Range2F rect = Range2F(V2F(use.node->rect.min.x, use.node->rect.min.y - Min1F(height_per_row*filtered_strings.count*scale, 192.0f)),
                                   V2F(use.node->rect.min.x + max_width + 4.0f*padding*scale, use.node->rect.min.y));
            UI_SetNextFixedRect(rect);
            UI_SetNextWidth(UI_SizeNone());
            UI_SetNextHeight(UI_SizeNone());
            UI_SetNextChildrenLayout(Axis2_Y);
            UI_SetNextFgCol(ptbd_grey);
            UI_Node *autocomplete_popup =
                UI_NodeMakeFromFmt(UI_Flags_DrawFill |
                                   UI_Flags_DrawStroke |
                                   UI_Flags_DrawDropShadow |
                                   UI_Flags_FixedFinalRect |
                                   UI_Flags_AnimateSmoothScroll,
                                   "inspector %lld %.*s autocomplete",
                                   inspector->id,
                                   FmtS8(ptbd_inspector_columns[field_index].title));
            
            autocomplete_popup->target_view_offset.y = Clamp1F(-height_per_row*scale*inspector->autocomplete_index[field_index] + 0.5f*(rect.max.y - rect.min.y), rect.max.y - rect.min.y - height_per_row*scale*filtered_strings.count, 0.0f);
            
            UI_Parent(autocomplete_popup)
                UI_Height(UI_SizePx(height_per_row, 1.0f))
                UI_Width(UI_SizePx(max_width / scale + 4.0f*padding, 1.0f))
            {
                G_EventQueue *events = G_WindowEventQueueGet(ui->window);
                if(G_EventQueueHasKeyDown(events, G_Key_Up, 0, True))
                {
                    inspector->autocomplete_index[field_index] += -1 + filtered_strings.count;
                }
                else if(G_EventQueueHasKeyDown(events, G_Key_Down, 0, True))
                {
                    inspector->autocomplete_index[field_index] += 1;
                }
                inspector->autocomplete_index[field_index] = inspector->autocomplete_index[field_index] % filtered_strings.count;
                
                UI_Window window = {0};
                {
                    window.pixel_range = Range1F(-autocomplete_popup->view_offset.y, -autocomplete_popup->view_offset.y + rect.max.y - rect.min.y);
                    window.index_range = Range1U64(window.pixel_range.min / (height_per_row*scale), window.pixel_range.max / (height_per_row*scale) + 1);
                    window.space_before = window.index_range.min*height_per_row;
                    window.space_after = (filtered_strings.count - window.index_range.max)*height_per_row;
                }
                
                UI_Spacer(UI_SizePx(window.space_before, 1.0f));
                
                I32 index = 0;
                for(S8ListForEach(filtered_strings, s))
                {
                    if(HasPoint1U64(window.index_range, index))
                    {
                        UI_Row()
                            UI_Width(UI_SizeFromText(padding, 1.0f))
                        {
                            S8Split split = S8SplitMake(s->string, S8("\x1E"), 0);
                            S8 string = S8SplitNext(&split) ? split.current : S8("");
                            S8 desc = S8SplitNext(&split) ? split.current : S8("");
                            
                            if(index == inspector->autocomplete_index[field_index])
                            {
                                UI_SetNextFgCol(ColFromHex(0x5e81acff));
                                
                                if(G_EventQueueHasKeyDown(events, G_Key_Tab, 0, True))
                                {
                                    inspector->fields[field_index].len = 0;
                                    S8BuilderAppend(&inspector->fields[field_index], string);
                                    use.node->first->next->text_selection = Range1U64(inspector->fields[field_index].len,
                                                                                      inspector->fields[field_index].len);
                                    use.is_edited = True;
                                    OS_TCtxErrorScopeClear(); // NOTE(tbt): validation has already been done, but we have now replaced the entire string with something known to be valid, so errors can be cleared
                                }
                            }
                            
                            UI_Label(string);
                            if(desc.len > 0)
                            {
                                UI_Spacer(UI_SizeFill());
                                UI_SetNextFgCol(ColFromHex(0x4c566aff));
                                UI_Label(split.current);
                            }
                        }
                    }
                    index += 1;
                }
                
                UI_Spacer(UI_SizePx(window.space_after, 1.0f));
            }
        }
    }
    
    U64 errors_count = OS_TCtxErrorScopePropogate();
    
    if(use.is_edited)
    {
        inspector->should_write = (0 == errors_count);
        *last_edit_time = OS_TimeInMicroseconds();
    }
    
    // NOTE(tbt): make sure the UI tree has the most up to date version of the string
    use.node->first->next->text = S8FromBuilder(inspector->fields[field_index]);
    
    UI_Animate1F(&use.node->user_t, inspector->should_write, 30.0f);
    
    V4F default_col = ptbd_grey;
    V4F should_write_col = ColFromHex(0x81a1c1ff);
    V4F error_col = ColFromHex(0xbf616aff);
    use.node->fg = (errors_count > 0) ? error_col : default_col;
    if(use.node->flags & UI_Flags_KeyboardFocused)
    {
        use.node->fg = ColMix(use.node->fg, should_write_col, use.node->user_t);
    }
    use.node->fg = Scale4F(use.node->fg, inspector->spawn_t);
    use.node->hot_fg = use.node->fg;
    
    ArenaTempEnd(scratch);
    
    return use;
}

Function B32
PTBD_InspectorIsDisbursement(PTBD_Inspector *inspector)
{
    F64 cost = S8Parse1F64(S8FromBuilder(inspector->fields[PTBD_InspectorColumn_Cost]));
    B32 result = (!IsNaN1F(cost) && cost > 0.0f);
    return result;
}

Function B32
PTBD_InspectorIsTimesheet(PTBD_Inspector *inspector, B32 is_disbursement)
{
    F64 hours = PTBD_InspectorValueFromNumericField(inspector->fields[PTBD_InspectorColumn_Hours]);
    F64 miles = PTBD_InspectorValueFromNumericField(inspector->fields[PTBD_InspectorColumn_Miles]);
    B32 result = (!is_disbursement &&
                  !IsNaN1F(hours) && hours > 0.0f &&
                  !IsNaN1F(miles) && miles >= 0.0f);
    return result;
}

Function void
PTBD_BuildInspectorSentence(PTBD_Inspector *inspector, U64 *last_edit_time, S8List job_numbers, S8List employee_names, B32 should_grab_keyboard_focus)
{
    ArenaTemp scratch = OS_TCtxScratchBegin(0, 0);
    
    UI_Row() UI_Pad(UI_SizeFill())
    {
        UI_Size label_size = UI_SizeFromText(4.0f, 1.0f);
        UI_Size editor_size = UI_SizeSum(1.0f);
        
        if(PTBD_InspectorIsDisbursement(inspector))
        {
            UI_Width(label_size) UI_Label(S8("On "));
            UI_Width(editor_size)
            {
                UI_Use use = PTBD_BuildInspectorField(inspector, PTBD_InspectorColumn_Date, last_edit_time, (S8List){0});
                if(should_grab_keyboard_focus)
                {
                    UI_Node *inspectors_panel = UI_NavPeek().user_data;
                    inspectors_panel->nav_default_keyboard_focus = use.node->key;
                }
            }
            UI_Width(label_size) UI_Label(S8(", £"));
            UI_Width(editor_size) PTBD_BuildInspectorField(inspector, PTBD_InspectorColumn_Cost, last_edit_time, (S8List){0});
            UI_Width(label_size) UI_Label(S8(" was spent on "));
            UI_Width(editor_size) PTBD_BuildInspectorField(inspector, PTBD_InspectorColumn_Description, last_edit_time, (S8List){0});
            UI_Width(label_size) UI_Label(S8(" for "));
            UI_Width(editor_size) PTBD_BuildInspectorField(inspector, PTBD_InspectorColumn_Job, last_edit_time, job_numbers);
        }
        else if(PTBD_InspectorIsTimesheet(inspector, False))
        {
            UI_Width(label_size) UI_Label(S8("On "));
            UI_Width(editor_size)
            {
                UI_Use use = PTBD_BuildInspectorField(inspector, PTBD_InspectorColumn_Date, last_edit_time, (S8List){0});
                if(should_grab_keyboard_focus)
                {
                    UI_Node *inspectors_panel = UI_NavPeek().user_data;
                    inspectors_panel->nav_default_keyboard_focus = use.node->key;
                }
            }
            UI_Width(label_size) UI_Label(S8(", "));
            UI_Width(editor_size) PTBD_BuildInspectorField(inspector, PTBD_InspectorColumn_Employee, last_edit_time, employee_names);
            UI_Width(label_size) UI_Label(S8(" spent "));
            UI_Width(editor_size) PTBD_BuildInspectorField(inspector, PTBD_InspectorColumn_Hours, last_edit_time, (S8List){0});
            UI_Width(label_size) UI_Label(S8(" hours and "));
            UI_Width(editor_size) PTBD_BuildInspectorField(inspector, PTBD_InspectorColumn_Miles, last_edit_time, (S8List){0});
            UI_Width(label_size) UI_Label(S8(" miles on "));
            UI_Width(editor_size) PTBD_BuildInspectorField(inspector, PTBD_InspectorColumn_Description, last_edit_time, (S8List){0});
            UI_Width(label_size) UI_Label(S8(" for "));
            UI_Width(editor_size) PTBD_BuildInspectorField(inspector, PTBD_InspectorColumn_Job, last_edit_time, job_numbers);
        }
        else
        {
            UI_Width(label_size) UI_Label(S8("On "));
            UI_Width(editor_size) PTBD_BuildInspectorField(inspector, PTBD_InspectorColumn_Date, last_edit_time, (S8List){0});
            UI_Width(label_size) UI_Label(S8(", "));
            UI_Width(editor_size) PTBD_BuildInspectorField(inspector, PTBD_InspectorColumn_Employee, last_edit_time, employee_names);
            UI_Width(label_size) UI_Label(S8(" spent "));
            UI_Width(editor_size) PTBD_BuildInspectorField(inspector, PTBD_InspectorColumn_Hours, last_edit_time, (S8List){0});
            UI_Width(label_size) UI_Label(S8(" hours and "));
            UI_Width(editor_size) PTBD_BuildInspectorField(inspector, PTBD_InspectorColumn_Miles, last_edit_time, (S8List){0});
            UI_Width(label_size) UI_Label(S8(" miles or £"));
            UI_Width(editor_size) PTBD_BuildInspectorField(inspector, PTBD_InspectorColumn_Cost, last_edit_time, (S8List){0});
            UI_Width(label_size) UI_Label(S8(" on "));
            UI_Width(editor_size) PTBD_BuildInspectorField(inspector, PTBD_InspectorColumn_Description, last_edit_time, (S8List){0});
            UI_Width(label_size) UI_Label(S8(" for "));
            UI_Width(editor_size) PTBD_BuildInspectorField(inspector, PTBD_InspectorColumn_Job, last_edit_time, job_numbers);
        }
        
        UI_Spacer(UI_SizePx(8.0f, 1.0f));
    }
    
    ArenaTempEnd(scratch);
}

Function void
PTBD_BuildInspectorTableRow(PTBD_Inspector *inspector, U64 *last_edit_time, S8List job_numbers, S8List employee_names, B32 should_grab_keyboard_focus)
{
    ArenaTemp scratch = OS_TCtxScratchBegin(0, 0);
    
    S8List valid_strings_lut[PTBD_InspectorColumn_MAX] =
    {
        {0},
        employee_names,
        {0},
        {0},
        {0},
        {0},
        job_numbers,
    };
    
    B32 is_disbursement = PTBD_InspectorIsDisbursement(inspector);
    B32 is_timesheet = PTBD_InspectorIsTimesheet(inspector, is_disbursement);
    
    UI_Row() UI_Pad(UI_SizeFill())
        for(U64 column_index = 0; column_index < PTBD_InspectorColumn_MAX; column_index += 1)
    {
        UI_Size width = ptbd_inspector_columns[column_index].width;
        S8List valid_strings = valid_strings_lut[column_index];
        UI_Width(width)
        {
            if((is_timesheet    && (column_index == PTBD_InspectorColumn_Cost)) ||
               (is_disbursement && (column_index == PTBD_InspectorColumn_Employee ||
                                    column_index == PTBD_InspectorColumn_Hours ||
                                    column_index == PTBD_InspectorColumn_Miles)))
            {
                UI_Label(S8("-"));
            }
            else
            {
                UI_Use use = PTBD_BuildInspectorField(inspector, column_index, last_edit_time, valid_strings);
                if(0 == column_index && should_grab_keyboard_focus)
                {
                    UI_Node *inspectors_panel = UI_NavPeek().user_data;
                    inspectors_panel->nav_default_keyboard_focus = use.node->key;
                }
            }
        }
    }
    
    ArenaTempEnd(scratch);
}

Function void
PTBD_BuildInspectorsPanel(PTBD_Inspectors *inspectors, DB_Cache timesheets, DB_Cache jobs, DB_Cache employees, DB_Cache invoices)
{
    ArenaTemp scratch = OS_TCtxScratchBegin(0, 0);
    
    F32 headings_height = 40.0f;
    F32 totals_height = 40.0f;
    F32 decorations_height = headings_height + totals_height;
    F32 padding = 4.0f;
    
    F32 height_per_row = 40.0f;
    F32 scroll_to_column_width = 40.0f;
    F32 max_height_in_rows = 9.5f;
    
    S8List job_numbers = {0};
    for(U64 row_index = 0; row_index < jobs.rows_count; row_index += 1)
    {
        S8ListPushFmt(scratch.arena, &job_numbers,
                      "%.*s\x1E%.*s",
                      FmtS8(jobs.rows[row_index].job_number),
                      FmtS8(jobs.rows[row_index].job_name));
    }
    
    S8List employee_names = {0};
    for(U64 row_index = 0; row_index < employees.rows_count; row_index += 1)
    {
        S8ListPush(scratch.arena, &employee_names, employees.rows[row_index].employee_name);
    }
    
    F32 total_spawn_t = 0;
    F64 total_hours = 0.0f;
    F64 total_miles = 0.0f;
    F64 total_cost = 0.0f;
    for(PTBD_Inspector *inspector = inspectors->first; 0 != inspector; inspector = inspector->next)
    {
        total_spawn_t += inspector->spawn_t;
        
        DB_Row *row = DB_RowFromId(timesheets, inspector->id);
        total_hours += row->hours;
        total_miles += row->miles;
        total_cost += row->cost;
    }
    F32 decorations_spawn_t = Clamp1F(total_spawn_t - 1.0f, 0.0f, 1.0f);
    
    F32 panel_height = Min1F(height_per_row*total_spawn_t + decorations_height*decorations_spawn_t, max_height_in_rows*height_per_row);
    
    UI_SetNextHeight(UI_SizePx(panel_height, 1.0f));
    UI_SetNextChildrenLayout(Axis2_X);
    UI_Node *container = UI_NodeMake(UI_Flags_Pressable | UI_Flags_DoNotMask | UI_Flags_AnimateSmoothLayout, S8("inspectors panel"));
    
    UI_Node *scroll_region = 0;
    
    UI_Parent(container) UI_Salt(S8("inspectors")) UI_Nav(UI_NavContext(&UI_NavDefault, container, container->key))
        UI_Width(UI_SizeFill())
        UI_Height(UI_SizeFill())
    {
        UI_Column()
        {
            if(decorations_spawn_t > UI_AnimateSlop)
            {
                UI_Height(UI_SizePx(headings_height*decorations_spawn_t - padding, 1.0f)) UI_Row()
                {
                    UI_Font(ui_font_bold) UI_Pad(UI_SizeFill())
                        for(U64 column_index = 0; column_index < PTBD_InspectorColumn_MAX; column_index += 1)
                        UI_Width(ptbd_inspector_columns[column_index].width)
                    {
                        UI_Label(ptbd_inspector_columns[column_index].title);
                    }
                    UI_Spacer(UI_SizePx(scroll_to_column_width, 1.0f));
                }
                UI_Spacer(UI_SizePx(padding, 1.0f));
            }
            
            UI_SetNextChildrenLayout(Axis2_Y);
            UI_SetNextFgCol(ColFromHex(0xb48eadff));
            UI_SetNextStrokeWidth(2.0f*container->user_t);
            scroll_region = UI_NodeMake(UI_Flags_DrawStroke |
                                        UI_Flags_Scrollable |
                                        UI_Flags_ScrollChildrenY |
                                        UI_Flags_ClampViewOffsetToOverflow |
                                        UI_Flags_AnimateSmoothScroll,
                                        S8("scroll region"));
            
            if(decorations_spawn_t > UI_AnimateSlop)
            {
                UI_Height(UI_SizePx(totals_height*decorations_spawn_t - padding, 1.0f)) UI_Row()
                {
                    UI_Spacer(UI_SizePx(padding, 1.0f));
                    
                    UI_Font(ui_font_bold) UI_Pad(UI_SizeFill())
                        for(U64 column_index = 0; column_index < PTBD_InspectorColumn_MAX; column_index += 1)
                        UI_Width(ptbd_inspector_columns[column_index].width)
                    {
                        if      (PTBD_InspectorColumn_Hours == column_index) { UI_LabelFromFmt("%.0f:%02.0f", Floor1F(total_hours), 60.0f*Fract1F(total_hours)); }
                        else if (PTBD_InspectorColumn_Miles == column_index) { UI_LabelFromFmt("%.2f", total_miles); }
                        else if (PTBD_InspectorColumn_Cost == column_index)  { UI_LabelFromFmt("£%.2f", total_cost); }
                        else                                                 { UI_Spacer(UI_WidthPeek()); }
                    }
                    UI_Spacer(UI_SizePx(scroll_to_column_width, 1.0f));
                }
            }
        }
        
        UI_Width(UI_SizePx(12.0f, 1.0f))
            UI_ScrollBar(S8("scroll bar"), scroll_region, Axis2_Y);
        
        UI_Window window =
            PTBD_UIWindowFromContainerScrollHeightPerRowAndRowsCount(scroll_region,
                                                                     height_per_row,
                                                                     inspectors->count,
                                                                     scroll_region->target_view_offset.y);
        
        UI_Parent(scroll_region)
        {
            U64 inspector_index = 0;
            
            UI_Spacer(UI_SizePx(window.space_before, 1.0f));
            
            F32 padding = 4.0f;
            
            for(PTBD_Inspector *inspector = inspectors->first; 0 != inspector; inspector = inspector->next)
            {
                if(window.index_range.min <= inspector_index && inspector_index < window.index_range.max)
                {
                    UI_Spacer(UI_SizePx(padding, 1.0f));
                    UI_Height(UI_SizePx(height_per_row - 2.0f*padding, 1.0f)) UI_Row()
                    {
                        B32 should_grab_keyboard_focus = (inspectors->grab_keyboard_focus == inspector->id);
                        if(should_grab_keyboard_focus)
                        {
                            UI_NavInterfaceStackPop();
                            UI_NavInterfaceStackPush(&container->key);
                            inspectors->grab_keyboard_focus = 0;
                        }
                        
                        if(inspectors->touched_count > 1)
                        {
                            PTBD_BuildInspectorTableRow(inspector, &inspectors->last_edit_time, job_numbers, employee_names, should_grab_keyboard_focus);
                        }
                        else
                        {
                            PTBD_BuildInspectorSentence(inspector, &inspectors->last_edit_time, job_numbers, employee_names, should_grab_keyboard_focus);
                        }
                        
                        UI_SaltFmt("%lld", inspector->id)
                            UI_Width(UI_SizePx(scroll_to_column_width, 1.0f))
                            UI_Height(UI_SizePx(scroll_to_column_width, 1.0f))
                            UI_Growth(-4.)
                            UI_CornerRadius(16.0f) 
                        {
                            DB_Row *invoice = DB_RowFromId(invoices, inspector->invoice_id);
                            UI_Use use = PTBD_BuildInvoiceEmblem(invoice);
                            if(use.is_hovering && 0 != invoice)
                            {
                                PTBD_ScrollToRow(DB_Table_Timesheets, inspector->id);
                            }
                        }
                    }
                    UI_Spacer(UI_SizePx(padding, 1.0f));
                }
                inspector_index += 1;
            }
            
            UI_Spacer(UI_SizePx(window.space_after, 1.0f));
        }
        
        UI_UseFromNode(scroll_region);
    }
    
    UI_Use use = UI_UseFromNode(container);
    
    if(use.is_pressed)
    {
        UI_NavInterfaceStackPop();
        UI_NavInterfaceStackPush(&container->key);
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
            UI_Node *new_keyboard_focus = UI_NavDefaultNext(container);
            if(0 != new_keyboard_focus && 0 != new_keyboard_focus->first && 0 != new_keyboard_focus->first->next)
            {
                new_keyboard_focus->first->next->text_selection.cursor = new_keyboard_focus->first->next->text.len;
                new_keyboard_focus->first->next->text_selection.mark = 0;
            }
        }
        else if(G_EventQueueHasKeyDown(events, G_Key_Tab, G_ModifierKeys_Shift, True))
        {
            UI_Node *new_keyboard_focus = UI_NavDefaultPrev(container);
            if(0 != new_keyboard_focus && 0 != new_keyboard_focus->first && 0 != new_keyboard_focus->first->next)
            {
                new_keyboard_focus->first->next->text_selection.cursor = new_keyboard_focus->first->next->text.len;
                new_keyboard_focus->first->next->text_selection.mark = 0;
            }
        }
        else if(G_EventQueueHasKeyDown(events, G_Key_Right, 0, True))
        {
            UI_Node *new_keyboard_focus = UI_NavDefaultNext(container);
            if(0 != new_keyboard_focus && 0 != new_keyboard_focus->first && 0 != new_keyboard_focus->first->next)
            {
                new_keyboard_focus->first->next->text_selection.cursor = 0;
                new_keyboard_focus->first->next->text_selection.mark = 0;
            }
        }
        else if(G_EventQueueHasKeyDown(events, G_Key_Left, 0, True))
        {
            UI_Node *new_keyboard_focus = UI_NavDefaultPrev(container);
            if(0 != new_keyboard_focus && 0 != new_keyboard_focus->first && 0 != new_keyboard_focus->first->next)
            {
                new_keyboard_focus->first->next->text_selection.cursor = new_keyboard_focus->first->next->text.len;
                new_keyboard_focus->first->next->text_selection.mark = new_keyboard_focus->first->next->text.len;
            }
        }
    }
    
    ArenaTempEnd(scratch);
}

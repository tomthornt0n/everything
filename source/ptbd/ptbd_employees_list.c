
Function void
PTBD_EmployeesListNav(void *user_data, UI_Node *node)
{
    I64 *keyboard_focus_id = user_data;
    if(keyboard_focus_id > 0)
    {
        ArenaTemp scratch = OS_TCtxScratchBegin(0, 0);
        
        S8 text = S8FromFmt(scratch.arena, "employee %lld", *keyboard_focus_id);
        if(S8Match(node->text, text, 0))
        {
            node->flags |= UI_Flags_KeyboardFocused;
        }
        
        ArenaTempEnd(scratch);
    }
}

Function U64
PTBD_EmployeesListNavRight(PTBD_WrappedEmployeesList *list, I64 *keyboard_focus_id)
{
    U64 scroll_to_row_index = 0;
    
    B32 should_return_next = False;
    B32 did_return_next = False;
    
    U64 row_index = 0;
    for(PTBD_WrappedEmployeesListRow *row = list->first; 0 != row; row = row->next)
    {
        for(U64 column_index = 0; column_index < row->count; column_index += 1)
        {
            DB_Row *employee = row->employees[column_index];
            if(should_return_next)
            {
                did_return_next = True;
                *keyboard_focus_id = employee->id;
                scroll_to_row_index = row_index;
                goto break_all;
            }
            else if(employee->id == *keyboard_focus_id)
            {
                should_return_next = True;
            }
        }
        row_index += 1;
    }
    break_all:;
    
    if(!did_return_next && list->count > 0 && list->first->count > 0)
    {
        DB_Row *employee = list->first->employees[0];
        *keyboard_focus_id = employee->id;
    }
    
    ui->was_last_interaction_keyboard = True;
    
    return scroll_to_row_index;
}

Function U64
PTBD_EmployeesListNavLeft(PTBD_WrappedEmployeesList *list, I64 *keyboard_focus_id)
{
    U64 scroll_to_row_index = 0;
    
    U64 prev_row_index = list->count - 1;
    I64 prev = 0;
    if(list->count > 0 && list->last->count > 0)
    {
        DB_Row *employee = list->last->employees[list->last->count - 1];
        prev = employee->id;
    }
    
    U64 row_index = 0;
    for(PTBD_WrappedEmployeesListRow *row = list->first; 0 != row; row = row->next)
    {
        for(U64 column_index = 0; column_index < row->count; column_index += 1)
        {
            DB_Row *employee = row->employees[column_index];
            if(employee->id == *keyboard_focus_id)
            {
                *keyboard_focus_id = prev;
                scroll_to_row_index = prev_row_index;
                goto break_all;
            }
            prev_row_index = row_index;
            prev = employee->id;
        }
        row_index += 1;
    }
    break_all:;
    
    ui->was_last_interaction_keyboard = True;
    
    return scroll_to_row_index;
}

Function U64
PTBD_EmployeesListNavDown(PTBD_WrappedEmployeesList *list, I64 *keyboard_focus_id)
{
    U64 scroll_to_row_index = 0;
    
    U64 target_column_index = 0;
    B32 should_return_next = False;
    B32 did_return_next = False;
    
    U64 row_index = 0;
    for(PTBD_WrappedEmployeesListRow *row = list->first; 0 != row; row = row->next)
    {
        if(should_return_next)
        {
            did_return_next = True;
            if(row->count > target_column_index)
            {
                DB_Row *employee = row->employees[target_column_index];
                *keyboard_focus_id = employee->id;
                scroll_to_row_index = row_index;
            }
            break;
        }
        else
        {
            for(U64 column_index = 0; column_index < row->count; column_index += 1)
            {
                DB_Row *employee = row->employees[column_index];
                if(employee->id == *keyboard_focus_id)
                {
                    should_return_next = True;
                    target_column_index = column_index;
                    scroll_to_row_index = row_index;
                    break;
                }
            }
        }
        row_index += 1;
    }
    
    if(!did_return_next && list->count > 0 && list->first->count > 0)
    {
        DB_Row *employee = list->first->employees[0];
        *keyboard_focus_id = employee->id;
    }
    
    ui->was_last_interaction_keyboard = True;
    
    return scroll_to_row_index;
}

Function U64
PTBD_EmployeesListNavUp(PTBD_WrappedEmployeesList *list, I64 *keyboard_focus_id)
{
    U64 scroll_to_row_index = 0;
    
    PTBD_WrappedEmployeesListRow *prev = 0;
    U64 row_index = 0;
    for(PTBD_WrappedEmployeesListRow *row = list->first; 0 != row; row = row->next)
    {
        for(U64 column_index = 0; column_index < row->count; column_index += 1)
        {
            DB_Row *employee = row->employees[column_index];
            if(employee->id == *keyboard_focus_id)
            {
                if(0 != prev && prev->count > column_index)
                {
                    DB_Row *employee = prev->employees[column_index];
                    *keyboard_focus_id = employee->id;
                    scroll_to_row_index = (0 == row_index) ? 0 : row_index - 1;
                }
                goto break_all;
            }
        }
        prev = row;
        row_index += 1;
    }
    break_all:;
    
    ui->was_last_interaction_keyboard = True;
    
    return scroll_to_row_index;
}

Function void
PTBD_WrappedEmployeesListPush(Arena *arena, PTBD_WrappedEmployeesList *list, U64 employees_per_row, DB_Row *employee)
{
    if(0 == list->last || list->last->count >= employees_per_row)
    {
        PTBD_WrappedEmployeesListRow *new_row = PushArray(arena, PTBD_WrappedEmployeesListRow, 1);
        if(0 == list->last)
        {
            list->first = new_row;
        }
        else
        {
            list->last->next = new_row;
        }
        list->last = new_row;
        list->count += 1;
    }
    
    if(list->last->count < ArrayCount(list->last->employees))
    {
        list->last->employees[list->last->count] = employee;
        list->last->count += 1;
    }
}

Function void
PTBD_BuildEmployeesList(DB_Cache employees,
                        PTBD_Index *index,
                        I64 *keyboard_focus_id,
                        PTBD_EmployeeEditor *employee_editor,
                        PTBD_MsgQueue *m2c)
{
    ArenaTemp scratch = OS_TCtxScratchBegin(0, 0);
    
    F32 s = G_WindowScaleFactorGet(ui->window);
    
    UI_SetNextChildrenLayout(Axis2_X);
    UI_Node *container = UI_NodeMake(UI_Flags_Pressable | UI_Flags_DoNotMask | UI_Flags_AnimateSmoothLayout, S8("employees lister"));
    
    UI_NavPush(UI_NavContext(PTBD_EmployeesListNav, keyboard_focus_id, container->key));
    
    UI_SetNextParent(container);
    UI_SetNextChildrenLayout(Axis2_Y);
    UI_SetNextFgCol(ColFromHex(0xb48eadff));
    UI_SetNextStrokeWidth(2.0f*container->user_t);
    UI_Node *scroll_region = UI_NodeMake(UI_Flags_DrawStroke |
                                         UI_Flags_Scrollable |
                                         UI_Flags_ScrollChildrenY |
                                         UI_Flags_ClampViewOffsetToOverflow |
                                         UI_Flags_AnimateSmoothScroll |
                                         UI_Flags_AnimateSmoothLayout,
                                         S8("employees scroll region"));
    
    F32 scroll_region_width = (scroll_region->rect.max.x - scroll_region->rect.min.x) / s;
    
    F32 min_width_per_employee = 110.0f;
    U64 employees_per_row = Clamp1U64(scroll_region_width / min_width_per_employee, 1, PTBD_WrappedEmployeesListMaxPerRow);
    F32 width_per_employee = scroll_region_width / employees_per_row;
    F32 height_per_row = min_width_per_employee + 64.0f;
    
    // NOTE(tbt): build wrapped list every frame for now, could easily be cached but is really not an issue (only expected to have ~4 employees)
    PTBD_WrappedEmployeesList wrapped_list = {0};
    for(U64 record_index = 0; record_index < index->records_count; record_index += 1)
    {
        U64 employee_index = index->records[record_index].key;
        DB_Row *employee = &employees.rows[employee_index];
        PTBD_WrappedEmployeesListPush(scratch.arena, &wrapped_list, employees_per_row, employee);
    }
    // NOTE(tbt): append a nill employee to act as the new employee button
    ReadOnlyVar Persist DB_Row new_employee_placeholder = { .employee_name = S8Initialiser("New employee") };
    PTBD_WrappedEmployeesListPush(scratch.arena, &wrapped_list, employees_per_row, &new_employee_placeholder);
    
    PTBD_ApplyScrollToForList(DB_Table_Employees,
                              employees,
                              (DB_Cache){0},
                              index,
                              0,
                              &scroll_region->target_view_offset.y,
                              scroll_region->computed_size.y,
                              height_per_row,
                              employees_per_row);
    
    UI_Parent(container) UI_Width(UI_SizePx(12.0f, 1.0f))
        UI_ScrollBar(S8("employees scroll bar"), scroll_region, Axis2_Y);
    
    DB_Row *to_edit = 0;
    ErrorScope editor_errors = {0};
    B32 should_save = False;
    B32 should_close = False;
    
    for(PTBD_WrappedEmployeesListRow *row = wrapped_list.first; 0 != row; row = row->next)
    {
        UI_SetNextParent(scroll_region);
        UI_SetNextHeight(UI_SizePx(height_per_row, 1.0f));
        UI_SetNextChildrenLayout(Axis2_X);
        UI_Node *row_parent = UI_NodeMake(UI_Flags_DoNotMask, S8(""));
        
        for(U64 column_index = 0; column_index < row->count; column_index += 1)
        {
            DB_Row *employee = row->employees[column_index];
            
            UI_SetNextParent(row_parent);
            UI_SetNextWidth(UI_SizePx(width_per_employee, 1.0f));
            UI_SetNextChildrenLayout(Axis2_Y);
            UI_Node *panel = UI_NodeMakeFromFmt(UI_Flags_DoNotMask |
                                                UI_Flags_AnimateHot |
                                                UI_Flags_AnimateActive |
                                                UI_Flags_AnimateSmoothLayout |
                                                UI_Flags_Pressable,
                                                "employee %lld",
                                                employee->id);
            UI_Parent(panel)
            {
                UI_Height(UI_SizePx(min_width_per_employee, 1.0f)) UI_Row() UI_Pad(UI_SizeFill())
                {
                    UI_SetNextWidth(UI_SizePx(min_width_per_employee, 1.0f));
                    UI_SetNextFgCol(ptbd_grey);
                    UI_SetNextBgCol(PTBD_ColourFromEmployeeId(employee->id));
                    UI_SetNextCornerRadius(0.5f*(min_width_per_employee - 16.0f));
                    UI_SetNextGrowth(-8.0f);
                    UI_Node *icon_background =
                        UI_NodeMake(UI_Flags_DrawFill |
                                    UI_Flags_DrawDropShadow |
                                    UI_Flags_DrawHotStyle |
                                    UI_Flags_DrawActiveStyle |
                                    UI_Flags_DoNotMask |
                                    UI_Flags_AnimateInheritHot |
                                    UI_Flags_AnimateInheritActive,
                                    S8(""));
                    UI_Parent(icon_background) UI_Font(ui_font_bold) UI_FontSize(50) 
                    {
                        if(0 == employee->id) { UI_Label(S8("+")); }
                        else                  { UI_LabelFromIcon(FONT_Icon_Adult); }
                    }
                }
                
                if(employee->id != employee_editor->id)
                {
                    UI_Height(UI_SizeFromText(8.0f, 0.5f))
                    {
                        UI_Font(ui_font_bold) UI_Label(employee->employee_name);
                        if(0 != employee->id) UI_LabelFromFmt("£%.2f/hour", employee->rate);
                        UI_Spacer(UI_SizePx(12.0f, 1.0f));
                    }
                }
                else
                {
                    UI_Nav(UI_NavContext(&UI_NavDefault, panel, panel->key)) UI_Width(UI_SizeSum(0.5f)) UI_Height(UI_SizePx(32.0f, 1.0f))
                    {
                        OS_TCtxErrorScopePush();
                        
                        UI_Use name_editor;
                        UI_Row()
                        {
                            UI_SetNextFgCol(ptbd_grey);
                            UI_SetNextCornerRadius(15);
                            name_editor = UI_LineEdit(S8("employee editor name"), &employee_editor->name);
                            
                            UI_Spacer(UI_SizePx(4.0f, 0.5f));
                            UI_Width(UI_SizeFromText(4.0f, 0.5f)) UI_DefaultFlags(UI_Flags_NavDefaultFilter)
                            {
                                if(UI_IconButton(S8("employee editor save"), FONT_Icon_OkCircled).is_pressed)       { should_save = True; }
                                if(UI_IconButton(S8("employee editor cancel"), FONT_Icon_CancelCircled).is_pressed) { should_close = True; }
                            }
                        }
                        UI_Use rate_editor;
                        UI_Row()
                        {
                            UI_Width(UI_SizeFromText(4.0f, 0.5f)) UI_Label(S8("£"));
                            UI_SetNextFgCol(ptbd_grey);
                            UI_SetNextCornerRadius(15);
                            rate_editor = UI_LineEdit(S8("employee editor rate"), &employee_editor->rate);
                            UI_Width(UI_SizeFromText(4.0f, 0.5f)) UI_Label(S8("/hour"));
                        }
                        
                        if(name_editor.node->is_new)
                        {
                            panel->nav_default_keyboard_focus = name_editor.node->key;
                        }
                        
                        if(0 == employee_editor->name.len)
                        {
                            OS_TCtxErrorPush(S8("Employee name cannot be left blank."));
                            name_editor.node->fg = ColFromHex(0xbf616aff);
                            name_editor.node->hot_fg = ColFromHex(0xbf616aff);
                        }
                        else
                        {
                            S8 name = S8FromBuilder(employee_editor->name);
                            for(U64 row_index = 0; row_index < employees.rows_count; row_index += 1)
                            {
                                if(S8Match(name, employees.rows[row_index].employee_name, 0) && employees.rows[row_index].id != employee_editor->id)
                                {
                                    OS_TCtxErrorPushFmt("There is already an employee called '%.*s'.", FmtS8(name));
                                    name_editor.node->fg = ColFromHex(0xbf616aff);
                                    name_editor.node->hot_fg = ColFromHex(0xbf616aff);
                                    break;
                                }
                            }
                        }
                        
                        if(0 == employee_editor->rate.len)
                        {
                            OS_TCtxErrorPush(S8("Rate cannot be left blank."));
                            rate_editor.node->fg = ColFromHex(0xbf616aff);
                            rate_editor.node->hot_fg = ColFromHex(0xbf616aff);
                        }
                        else
                        {
                            F64 rate = S8Parse1F64(S8FromBuilder(employee_editor->rate));
                            if(IsNaN1F(rate) || !HasPoint1F(Range1F(10.0f, 500.0f), rate))
                            {
                                OS_TCtxErrorPush(S8("Rate must be a reasonable number."));
                                rate_editor.node->fg = ColFromHex(0xbf616aff);
                                rate_editor.node->hot_fg = ColFromHex(0xbf616aff);
                            }
                        }
                        
                        editor_errors = OS_TCtxErrorScopePop(scratch.arena);
                    }
                    
                    B32 is_top_of_interface_stack = UI_NavInterfaceStackMatchTop(&panel->key);
                    
                    if(is_top_of_interface_stack)
                    {
                        G_EventQueue *events = G_WindowEventQueueGet(ui->window);
                        if(UI_KeyIsNil(&panel->nav_default_keyboard_focus) ||
                           G_EventQueueHasKeyDown(events, G_Key_Tab, 0, True))
                        {
                            UI_NavDefaultNext(panel);
                        }
                        else if(G_EventQueueHasKeyDown(events, G_Key_Tab, G_ModifierKeys_Shift, True))
                        {
                            UI_NavDefaultPrev(panel);
                        }
                        else if(G_EventQueueHasKeyDown(events, G_Key_Esc, 0, True))
                        {
                            should_close = True;
                        }
                        else if(G_EventQueueHasKeyDown(events, G_Key_S, G_ModifierKeys_Ctrl, True))
                        {
                            should_save = True;
                        }
                        
                        if(should_close)
                        {
                            UI_NavInterfaceStackPop();
                        }
                    }
                }
            }
            
            UI_Use use = UI_UseFromNode(panel);
            if(use.is_pressed)
            {
                to_edit = employee;
                UI_NavInterfaceStackPush(&panel->key);
            }
            else if(0 != employee->id)
            {
                if(use.is_dragging && 0 == ptbd_drag_drop.count)
                {
                    PTBD_DragRow(DB_Table_Employees, *employee);
                }
                else if(use.is_left_up && ptbd_drag_drop.count > 0)
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
                                               employee->id,
                                               timesheet->job_id,
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
        }
    }
    
    if(should_save)
    {
        if(0 == editor_errors.count)
        {
            should_close = True;
            
            F64 rate = S8Parse1F64(S8FromBuilder(employee_editor->rate));
            M2C_WriteEmployee(m2c, employee_editor->id, S8FromBuilder(employee_editor->name), rate);
            M2C_Reload(m2c);
        }
        
        OS_TCtxErrorScopeMerge(&editor_errors);
    }
    
    if(should_close)
    {
        employee_editor->name.len = 0;
        employee_editor->rate.len = 0;
        employee_editor->id = -1;
    }
    
    if(0 != to_edit)
    {
        employee_editor->id = to_edit->id;
        employee_editor->name.len = 0;
        S8BuilderAppend(&employee_editor->name, to_edit->employee_name);
        employee_editor->rate.len = 0;
        S8BuilderAppendFmt(&employee_editor->rate, "%.2f", to_edit->rate);
    }
    
    UI_UseFromNode(scroll_region);
    
    UI_NavPop();
    
    UI_Use use = UI_UseFromNode(container);
    
    if(use.is_pressed)
    {
        UI_NavInterfaceStackPop();
        UI_NavInterfaceStackPush(&container->key);
    }
    
    
    B32 is_top_of_interface_stack = UI_NavInterfaceStackMatchTop(&container->key);
    
    UI_Animate1F(&container->user_t, is_top_of_interface_stack, 15.0f);
    
    G_EventQueue *events = G_WindowEventQueueGet(ui->window);
    
    if(is_top_of_interface_stack)
    {
        if(G_EventQueueHasKeyDown(events, G_Key_Tab, 0, True) ||
           G_EventQueueHasKeyDown(events, G_Key_Right, 0, True))
        {
            U64 scroll_to_row_index = PTBD_EmployeesListNavRight(&wrapped_list, keyboard_focus_id);
            scroll_region->target_view_offset.y =
                0.5f*scroll_region->computed_size.y - (scroll_to_row_index + 0.5f)*height_per_row*s;
        }
        else if(G_EventQueueHasKeyDown(events, G_Key_Tab, G_ModifierKeys_Shift, True) ||
                G_EventQueueHasKeyDown(events, G_Key_Left, 0, True))
        {
            U64 scroll_to_row_index = PTBD_EmployeesListNavLeft(&wrapped_list, keyboard_focus_id);
            scroll_region->target_view_offset.y =
                0.5f*scroll_region->computed_size.y - (scroll_to_row_index + 0.5f)*height_per_row*s;
        }
        else if(G_EventQueueHasKeyDown(events, G_Key_Down, 0, True))
        {
            U64 scroll_to_row_index = PTBD_EmployeesListNavDown(&wrapped_list, keyboard_focus_id);
            scroll_region->target_view_offset.y =
                0.5f*scroll_region->computed_size.y - (scroll_to_row_index + 0.5f)*height_per_row*s;
        }
        else if(G_EventQueueHasKeyDown(events, G_Key_Up, 0, True))
        {
            U64 scroll_to_row_index = PTBD_EmployeesListNavUp(&wrapped_list, keyboard_focus_id);
            scroll_region->target_view_offset.y =
                0.5f*scroll_region->computed_size.y - (scroll_to_row_index + 0.5f)*height_per_row*s;
        }
    }
    
    ArenaTempEnd(scratch);
}

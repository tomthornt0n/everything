
Function void
PTBD_InvoicesListNav(void *user_data, UI_Node *node)
{
    I64 *keyboard_focus_id = user_data;
    if(keyboard_focus_id > 0)
    {
        ArenaTemp scratch = OS_TCtxScratchBegin(0, 0);
        
        S8 text = S8FromFmt(scratch.arena, "invoice %lld", *keyboard_focus_id);
        if(S8Match(node->text, text, 0))
        {
            node->flags |= UI_Flags_KeyboardFocused;
        }
        
        ArenaTempEnd(scratch);
    }
}

Function U64
PTBD_InvoicesListNavNext(PTBD_Index *index, DB_Cache invoices, I64 *keyboard_focus_id)
{
    U64 invoice_index = 0;
    
    B32 should_return_next = False;
    for(U64 record_index = 0; record_index < index->records_count; record_index += 1)
    {
        I64 invoice_id = invoices.rows[index->records[record_index].key].id;
        if(should_return_next)
        {
            invoice_index = record_index;
            break;
        }
        if(invoice_id == *keyboard_focus_id)
        {
            should_return_next = True;
        }
    }
    
    *keyboard_focus_id = invoices.rows[index->records[invoice_index].key].id;
    
    ui->was_last_interaction_keyboard = True;
    
    return invoice_index;
}

Function U64
PTBD_InvoicesListNavPrev(PTBD_Index *index, DB_Cache invoices, I64 *keyboard_focus_id)
{
    U64 invoice_index = 0;
    
    for(U64 record_index = 0; record_index < index->records_count; record_index += 1)
    {
        I64 invoice_id = invoices.rows[index->records[record_index].key].id;
        if(*keyboard_focus_id == invoice_id)
        {
            break;
        }
        invoice_index = record_index;
    }
    
    *keyboard_focus_id = invoices.rows[index->records[invoice_index].key].id;
    
    ui->was_last_interaction_keyboard = True;
    
    return invoice_index;
}

Function void
PTBD_BuildInvoicesList(DB_Cache invoices,
                       DB_Cache timesheets,
                       PTBD_Index *index,
                       I64 *keyboard_focus_id,
                       PTBD_InvoiceEditor *invoice_editor,
                       PTBD_MsgQueue *m2c)
{
    ArenaTemp scratch = OS_TCtxScratchBegin(0, 0);
    
    UI_SetNextChildrenLayout(Axis2_X);
    UI_Node *container = UI_NodeMake(UI_Flags_Pressable | UI_Flags_DoNotMask | UI_Flags_AnimateSmoothLayout, S8("invoices lister"));
    
    F32 height_per_row = Min1F(400.0f, (1.5f / G_WindowScaleFactorGet(ui->window))*(container->rect.max.x - container->rect.min.x));
    UI_Node *scroll_region = 0;
    
    UI_Parent(container) UI_Salt(S8("invoices")) UI_Nav(UI_NavContext(PTBD_InvoicesListNav, keyboard_focus_id, container->key))
    {
        UI_SetNextChildrenLayout(Axis2_Y);
        UI_SetNextFgCol(ColFromHex(0xb48eadff));
        UI_SetNextStrokeWidth(2.0f*container->user_t);
        scroll_region = UI_NodeMake(UI_Flags_DrawStroke |
                                    UI_Flags_Scrollable |
                                    UI_Flags_ScrollChildrenY |
                                    UI_Flags_ClampViewOffsetToOverflow |
                                    UI_Flags_AnimateSmoothScroll |
                                    UI_Flags_AnimateSmoothLayout,
                                    S8("scroll region"));
        
        PTBD_ApplyScrollToForList(DB_Table_Invoices, invoices, timesheets, index, 0, &scroll_region->target_view_offset.y, scroll_region->computed_size.y, height_per_row, 1);
        
        UI_Width(UI_SizePx(12.0f, 1.0f))
            UI_ScrollBar(S8("invoices scroll bar"), scroll_region, Axis2_Y);
        
        UI_Parent(scroll_region)
        {
            
            UI_Window window = PTBD_UIWindowFromContainerScrollHeightPerRowAndRowsCount(scroll_region,
                                                                                        height_per_row,
                                                                                        index->records_count,
                                                                                        scroll_region->target_view_offset.y);
            
            UI_Spacer(UI_SizePx(window.space_before, 1.0f));
            
            DB_Row *next_to_edit = 0;
            DB_Row *editing = 0;
            DB_Row *to_finish = 0;
            B32 should_save = False;
            B32 should_close_editor = False;
            ErrorScope editor_errors = {0};
            
            UI_Width(UI_SizeFill())
                for(U64 row_index = window.index_range.min; row_index < window.index_range.max; row_index += 1)
            {
                PTBD_IndexRecord *record = &index->records[row_index];
                DB_Row *invoice = &invoices.rows[record->key];
                
                F32 padding = 8.0f;
                V4F col = PTBD_ColourFromInvoiceId(invoice->id);
                
                // NOTE(tbt): space between invoices where timesheets can be dragged to make a new draft
                {
                    UI_SetNextChildrenLayout(Axis2_X);
                    UI_Node *spacer = UI_NodeMakeFromFmt(0, "invoice spacer %lld", invoice->id);
                    B32 all_dragged_timesheets_are_assigned_to_draft_invoices = (DB_Table_Timesheets == ptbd_drag_drop.kind);
                    B32 all_dragged_timesheets_are_uninvoiced = (DB_Table_Timesheets == ptbd_drag_drop.kind);
                    for(U64 payload_index = 0; payload_index < ptbd_drag_drop.count && (all_dragged_timesheets_are_uninvoiced || all_dragged_timesheets_are_assigned_to_draft_invoices); payload_index += 1)
                    {
                        DB_Row *timesheet = &ptbd_drag_drop.data[payload_index];
                        all_dragged_timesheets_are_assigned_to_draft_invoices = all_dragged_timesheets_are_assigned_to_draft_invoices && DB_InvoiceNumber_Draft == timesheet->invoice_number && timesheet->invoice_id != 0;
                        all_dragged_timesheets_are_uninvoiced = all_dragged_timesheets_are_uninvoiced && timesheet->invoice_id == 0;
                    }
                    B32 is_dragging_useable_rows =
                    (ptbd_drag_drop.count > 0 &&
                     ((DB_Table_Timesheets == ptbd_drag_drop.kind && (all_dragged_timesheets_are_uninvoiced || all_dragged_timesheets_are_assigned_to_draft_invoices)) ||
                      (DB_Table_Jobs == ptbd_drag_drop.kind && 1 == ptbd_drag_drop.count)));
                    B32 is_hovering = HasPoint2F(spacer->clipped_rect, G_WindowMousePositionGet(ui->window));
                    F32 extra_padding = 56.0f*spacer->user_t;
                    padding += extra_padding;
                    spacer->size[Axis2_Y] = UI_SizePx(padding, 1.0f);
                    spacer->fg = Scale4F(spacer->fg, spacer->user_t);
                    if(spacer->user_t > 0.01f)
                        UI_Parent(spacer)
                        UI_Width(UI_SizeFill())
                        UI_Height(UI_SizeFill())
                        UI_Growth(-4.0f)
                        UI_CornerRadius(16.0f)
                        UI_BgCol(U4F(0.0f))
                        UI_FgCol(ColFromHex(0xeceff4ff))
                    {
                        UI_Node *new_invoice_region =
                            UI_NodeMakeFromFmt(UI_Flags_DrawStroke |
                                               UI_Flags_DrawDropShadow |
                                               UI_Flags_Pressable |
                                               UI_Flags_NavDefaultFilter,
                                               "invoice new invoice region %lld", invoice->id);
                        UI_Parent(new_invoice_region)
                        {
                            UI_FgCol(Scale4F(ui->fg_stack[0], spacer->user_t)) UI_Font(ui_font_bold)
                                UI_Label(S8("New invoice"));
                            
                            UI_Use use = UI_UseFromNode(new_invoice_region);
                            if(use.is_left_up && ptbd_drag_drop.count > 0)
                            {
                                if(DB_Table_Jobs == ptbd_drag_drop.kind)
                                {
                                    if(1 == ptbd_drag_drop.count)
                                    {
                                        I64 job_id = ptbd_drag_drop.data[0].id;
                                        M2C_NewInvoiceForJob(m2c, job_id);
                                        M2C_Reload(m2c);
                                    }
                                    else
                                    {
                                        // NOTE(tbt): should be unreachable anyway? no way to drag more than one job in the UI (yet)
                                        OS_TCtxErrorPush(S8("Drag a single job with uninvoiced hours here to start a new invoice."));
                                    }
                                }
                                else if(DB_Table_Timesheets == ptbd_drag_drop.kind)
                                {
                                    U64 timesheets_to_add_count = ptbd_drag_drop.count;
                                    I64 *timesheets_to_add = PushArray(scratch.arena, I64, timesheets_to_add_count);
                                    for(U64 payload_index = 0; payload_index < ptbd_drag_drop.count; payload_index += 1)
                                    {
                                        timesheets_to_add[payload_index] = ptbd_drag_drop.data[payload_index].id;
                                    }
                                    M2C_NewInvoiceForTimesheets(m2c, timesheets_to_add_count, timesheets_to_add);
                                    M2C_Reload(m2c);
                                }
                                else
                                {
                                    OS_TCtxErrorPush(S8("Drag a uninvoiced timesheets, or a job with uninvoiced hours, here to start a new invoice."));
                                }
                            }
                            is_hovering = is_hovering || use.is_hovering;
                        }
                        
                        if(all_dragged_timesheets_are_assigned_to_draft_invoices)
                        {
                            UI_Node *no_invoice_region =
                                UI_NodeMakeFromFmt(UI_Flags_DrawStroke |
                                                   UI_Flags_DrawDropShadow |
                                                   UI_Flags_Pressable |
                                                   UI_Flags_NavDefaultFilter,
                                                   "invoice no invoice region %lld", invoice->id);
                            UI_Parent(no_invoice_region)
                            {
                                UI_FgCol(Scale4F(ui->fg_stack[0], spacer->user_t)) UI_Font(ui_font_bold)
                                    UI_Label(S8("No invoice"));
                                
                                UI_Use use = UI_UseFromNode(no_invoice_region);
                                if(use.is_left_up && ptbd_drag_drop.count > 0)
                                {
                                    M2C_UndoContinuationBegin(m2c);
                                    for(U64 payload_index = 0; payload_index < ptbd_drag_drop.count; payload_index += 1)
                                    {
                                        DB_Row *timesheet = &ptbd_drag_drop.data[payload_index];
                                        M2C_WriteTimesheet(m2c, timesheet->id, timesheet->date, timesheet->employee_id, timesheet->job_id, timesheet->hours, timesheet->miles, timesheet->cost, timesheet->description, 0);
                                    }
                                    M2C_UndoContinuationEnd(m2c);
                                    M2C_Reload(m2c);
                                }
                                is_hovering = is_hovering || use.is_hovering;
                            }
                        }
                    }
                    
                    UI_Animate1F(&spacer->user_t, is_hovering && is_dragging_useable_rows, 15.0f);
                }
                
                UI_SetNextGrowth(-4.0f);
                UI_SetNextCornerRadius(16.0f);
                UI_SetNextChildrenLayout(Axis2_X);
                if(DB_InvoiceNumber_Draft == invoice->invoice_number)
                {
                    UI_SetNextTexture(ptbd_gradient_vertical);
                    UI_SetNextTextureRegion(Range2F(V2F(0.0f, 0.95f), V2F(1.0f, 0.15f)));
                    UI_SetNextBgCol(col);
                    UI_SetNextStrokeWidth(0.0f);
                }
                else if(DB_InvoiceNumber_WrittenOff == invoice->invoice_number)
                {
                    UI_SetNextStrokeWidth(4.0f);
                    UI_SetNextFgCol(ColFromHex(0xbf616aff));
                    UI_SetNextBgCol(U4F(0.0f));
                }
                else
                {
                    UI_SetNextStrokeWidth(0.0f);
                    UI_SetNextBgCol(col);
                }
                
                UI_SetNextHeight(UI_SizePx(height_per_row - padding, 1.0f));
                UI_Node *panel =
                    UI_NodeMakeFromFmt(UI_Flags_DrawFill |
                                       UI_Flags_DrawStroke |
                                       UI_Flags_DrawDropShadow |
                                       UI_Flags_Pressable |
                                       UI_Flags_AnimateSmoothLayout,
                                       "invoice %lld", invoice->id);
                
                UI_Animate1F(&panel->user_t, (invoice->id == ptbd_scroll_to[DB_Table_Invoices]), 15.0f);
                panel->growth += 4.0f*panel->user_t;
                panel->fg = ColMix(panel->fg, ColFromHex(0x81a1c1ff), panel->user_t);
                
                UI_Parent(panel) UI_Nav(UI_NavContext(&UI_NavDefault, panel, panel->key))
                {
                    UI_Spacer(UI_SizePx(8.0f, 1.0f));
                    
                    UI_Column()
                    {
                        UI_Spacer(UI_SizePx(8.0f, 1.0f));
                        
                        UI_Font(ui_font_bold)
                        {
                            if(DB_InvoiceNumber_WrittenOff == invoice->invoice_number)
                            {
                                UI_Label(S8("Drag timesheets\nhere to write off."));
                            }
                            else
                                UI_Height(UI_SizePx(40.0f, 1.0f))
                            {
                                UI_Row()
                                {
                                    if(DB_InvoiceNumber_Draft == invoice->invoice_number)
                                    {
                                        UI_NodeMakeFromFmt(UI_Flags_DrawText | UI_Flags_AnimateSmoothLayout, "Draft invoice##title %lld", invoice->id);
                                        
                                        UI_Width(UI_SizePx(32.0f, 1.0f))
                                            UI_Font(ui_font_icons)
                                            UI_HotFgCol(ptbd_grey)
                                            UI_ActiveFgCol(ColFromHex(0x88c0d0ff))
                                        {
                                            UI_Node *finish_button =
                                                UI_NodeMakeFromFmt(UI_Flags_DrawText |
                                                                   UI_Flags_DrawHotStyle |
                                                                   UI_Flags_DrawActiveStyle |
                                                                   UI_Flags_Pressable |
                                                                   UI_Flags_AnimateHot |
                                                                   UI_Flags_AnimateActive |
                                                                   UI_Flags_AnimateSmoothLayout,
                                                                   "7##finish invoice %lld",
                                                                   invoice->id);
                                            if(UI_UseFromNode(finish_button).is_pressed)
                                            {
                                                to_finish = invoice;
                                            }
                                            
                                            if(invoice_editor->id == invoice->id)
                                            {
                                                editing = invoice;
                                                
                                                UI_SetNextWidth(UI_SizeSum(1.0f));
                                                UI_SetNextFgCol(Scale4F(col, 0.5f));
                                                UI_SetNextBgCol(Scale4F(col, 0.25f));
                                                UI_SetNextCornerRadius(19.0f);
                                                UI_Node *row = UI_NodeMake(UI_Flags_DrawStroke | UI_Flags_DrawDropShadow, S8(""));
                                                UI_Parent(row) UI_Pad(UI_SizePx(8.0f, 1.0f))
                                                {
                                                    UI_Node *save_button =
                                                        UI_NodeMakeFromFmt(UI_Flags_DrawText |
                                                                           UI_Flags_DrawHotStyle |
                                                                           UI_Flags_DrawActiveStyle |
                                                                           UI_Flags_Pressable |
                                                                           UI_Flags_AnimateHot |
                                                                           UI_Flags_AnimateActive |
                                                                           UI_Flags_AnimateSmoothLayout,
                                                                           ")##save invoice %lld",
                                                                           invoice->id);
                                                    if(UI_UseFromNode(save_button).is_pressed)
                                                    {
                                                        should_save = True;
                                                    }
                                                    UI_Node *cancel_button =
                                                        UI_NodeMakeFromFmt(UI_Flags_DrawText |
                                                                           UI_Flags_DrawHotStyle |
                                                                           UI_Flags_DrawActiveStyle |
                                                                           UI_Flags_Pressable |
                                                                           UI_Flags_AnimateHot |
                                                                           UI_Flags_AnimateActive |
                                                                           UI_Flags_AnimateSmoothLayout,
                                                                           "*##cancel edit invoice %lld",
                                                                           invoice->id);
                                                    if(UI_UseFromNode(cancel_button).is_pressed)
                                                    {
                                                        should_close_editor = True;
                                                    }
                                                }
                                            }
                                            else
                                            {
                                                UI_Node *edit_button =
                                                    UI_NodeMakeFromFmt(UI_Flags_DrawText |
                                                                       UI_Flags_DrawHotStyle |
                                                                       UI_Flags_DrawActiveStyle |
                                                                       UI_Flags_Pressable |
                                                                       UI_Flags_AnimateHot |
                                                                       UI_Flags_AnimateActive |
                                                                       UI_Flags_AnimateSmoothLayout,
                                                                       "p##edit invoice %lld",
                                                                       invoice->id);
                                                if(UI_UseFromNode(edit_button).is_pressed)
                                                {
                                                    next_to_edit = invoice;
                                                    
                                                    S8 address_field_string = S8FromFmt(scratch.arena, "invoice %lld address", invoice->id);
                                                    UI_NodeStrings strings = UI_TextAndIdFromString(address_field_string);
                                                    UI_Key address_field_key = UI_KeyFromIdAndSaltStack(strings.id, ui->salt_stack);
                                                    panel->nav_default_keyboard_focus = address_field_key;
                                                }
                                            }
                                            
                                            UI_Node *delete_button =
                                                UI_NodeMakeFromFmt(UI_Flags_DrawText |
                                                                   UI_Flags_DrawHotStyle |
                                                                   UI_Flags_DrawActiveStyle |
                                                                   UI_Flags_Pressable |
                                                                   UI_Flags_AnimateHot |
                                                                   UI_Flags_AnimateActive |
                                                                   UI_Flags_AnimateSmoothLayout,
                                                                   "(##delete invoice %lld",
                                                                   invoice->id);
                                            if(UI_UseFromNode(delete_button).is_pressed)
                                            {
                                                M2C_DeleteRow(m2c, DB_Table_Invoices, invoice->id);
                                                M2C_Reload(m2c);
                                            }
                                        }
                                    }
                                    else
                                    {
                                        UI_LabelFromFmt("Invoice no. %lld", invoice->invoice_number);
                                    }
                                    
                                    UI_Width(UI_SizePx(32.0f, 1.0f))
                                        UI_Font(ui_font_icons)
                                        UI_HotFgCol(ptbd_grey)
                                        UI_ActiveFgCol(ColFromHex(0x88c0d0ff))
                                    {
                                        UI_Node *export_button =
                                            UI_NodeMakeFromFmt(UI_Flags_DrawText |
                                                               UI_Flags_DrawHotStyle |
                                                               UI_Flags_DrawActiveStyle |
                                                               UI_Flags_Pressable |
                                                               UI_Flags_AnimateHot |
                                                               UI_Flags_AnimateActive |
                                                               UI_Flags_AnimateSmoothLayout,
                                                               "0##export invoice %lld",
                                                               invoice->id);
                                        if(UI_UseFromNode(export_button).is_pressed)
                                        {
                                            M2C_ExportInvoice(m2c, invoice->id);
                                        }
                                    }
                                    
                                    UI_Spacer(UI_SizePx(8.0f, 1.0f));
                                }
                            }
                        }
                        
                        if(invoice->invoice_number >= DB_InvoiceNumber_Draft)
                            UI_Height(UI_SizeFromText(8.0f, 1.0f)) UI_Width(UI_SizeFromText(8.0f, 1.0f))
                        {
                            if(invoice_editor->id == invoice->id)
                            {
                                OS_TCtxErrorScopePush();
                                
                                V4F fg_col = Scale4F(col, 0.5f);
                                UI_Width(UI_SizeSum(1.0f))
                                    UI_Height(UI_SizeSum(1.0f))
                                    UI_Growth(-4.0f)
                                    UI_CornerRadius(12.0f)
                                    UI_BgCol(Scale4F(col, 0.25f))
                                {
                                    UI_Spacer(UI_SizePx(8.0f, 1.0f));
                                    UI_SetNextFgCol(fg_col);
                                    UI_Use address_field = UI_MultiLineEdit(S8FromFmt(scratch.arena, "invoice %lld address", invoice->id), &invoice_editor->address);
                                    UI_Spacer(UI_SizePx(8.0f, 1.0f));
                                    UI_SetNextFgCol(fg_col);
                                    UI_LineEdit(S8FromFmt(scratch.arena, "invoice %lld title", invoice->id), &invoice_editor->title);
                                    UI_Spacer(UI_SizeFill());
                                    UI_SetNextFgCol(fg_col);
                                    UI_MultiLineEdit(S8FromFmt(scratch.arena, "invoice %lld description", invoice->id), &invoice_editor->description);
                                    UI_Spacer(UI_SizePx(8.0f, 1.0f));
                                    UI_Row()
                                    {
                                        UI_Width(UI_SizeFromText(8.0f, 1.0f)) UI_Height(UI_SizeFromText(8.0f, 1.0f))
                                        {
                                            UI_Font(ui_font_bold) UI_Label(S8("TOTAL: "));
                                            UI_Width(UI_SizeFromText(2.0f, 1.0f))UI_Label(S8("£"));
                                        }
                                        
                                        UI_SetNextFgCol(fg_col);
                                        UI_Use total_field = UI_TextField(S8FromFmt(scratch.arena, "invoice %lld total", invoice->id), &invoice_editor->cost, UI_EditTextFilterHookNumeric, 0);
                                        
                                        F64 cost = S8Parse1F64(S8FromBuilder(invoice_editor->cost));
                                        if(IsNaN1F(cost))
                                        {
                                            OS_TCtxErrorPush(S8("Enter a valid number in the total field."));
                                            total_field.node->fg = ColFromHex(0xbf616aff);
                                            total_field.node->hot_fg = ColFromHex(0xbf616aff);
                                        }
                                        else if(Abs1F(cost - invoice->cost_calculated) >= 0.01f)
                                        {
                                            UI_Height(UI_SizePx(37.0f, 1.0f)) UI_FontSize(14)
                                            {
                                                UI_SetNextWidth(UI_SizeFromText(2.0f, 1.0f));
                                                if(UI_IconButton(S8("reset invoice total"), FONT_Icon_Ccw).is_pressed)
                                                {
                                                    invoice_editor->cost.len = 0;
                                                    S8BuilderAppendFmt(&invoice_editor->cost, "%.2f", invoice->cost_calculated);
                                                }
                                                UI_Width(UI_SizeFromText(4.0f, 1.0f))
                                                    UI_LabelFromFmt("(£%.2f)", invoice->cost_calculated);
                                            }
                                        }
                                    }
                                    
                                    if(invoice_editor->address.len == 0)
                                    {
                                        OS_TCtxErrorPush(S8("The address cannot be left blank."));
                                        address_field.node->fg = ColFromHex(0xbf616aff);
                                        address_field.node->hot_fg = ColFromHex(0xbf616aff);
                                    }
                                }
                                
                                editor_errors = OS_TCtxErrorScopePop(scratch.arena);
                            }
                            else
                            {
                                if(invoice->invoice_number > DB_InvoiceNumber_Draft)
                                {
                                    DateTime date = DateTimeFromDenseTime(invoice->date);
                                    UI_LabelFromFmt("%.2d/%.2d/%d", date.day + 1, date.mon + 1, date.year);
                                }
                                UI_Label(invoice->address);
                                UI_Label(invoice->title);
                                UI_Spacer(UI_SizeFill());
                                UI_Label(invoice->description);
                                UI_SetNextWidth(UI_SizeSum(1.0f));
                                UI_Row()
                                {
                                    UI_Font(ui_font_bold) UI_Label(S8("TOTAL:"));
                                    if(invoice->cost <= invoice->cost_calculated - 0.01f)
                                    {
                                        UI_SetNextBgCol(ColFromHex(0xbf616aff));
                                        UI_SetNextDefaultFlags(UI_Flags_DrawFill | UI_Flags_DrawDropShadow);
                                        UI_SetNextCornerRadius(16.0f);
                                    }
                                    else if(invoice->cost >= invoice->cost_calculated + 0.01f)
                                    {
                                        UI_SetNextBgCol(ColFromHex(0xa3be8cff));
                                        UI_SetNextDefaultFlags(UI_Flags_DrawFill | UI_Flags_DrawDropShadow);
                                        UI_SetNextCornerRadius(16.0f);
                                    }
                                    UI_LabelFromFmt("£%.2f", invoice->cost);
                                }
                                UI_Spacer(UI_SizePx(8.0f, 1.0f));
                            }
                        }
                        
                        UI_Spacer(UI_SizePx(16.0f, 1.0f));
                    }
                }
                
                UI_Use use = UI_UseFromNode(panel);
                if(use.is_pressed)
                {
                    *keyboard_focus_id = invoice->id;
                }
                else if(use.is_dragging && 0 == ptbd_drag_drop.count)
                {
                    PTBD_DragRow(DB_Table_Invoices, *invoice);
                }
                else if(use.is_left_up && ptbd_drag_drop.count > 0)
                {
                    if(DB_InvoiceNumber_Draft == invoice->invoice_number ||
                       DB_InvoiceNumber_WrittenOff == invoice->invoice_number)
                    {
                        B32 did_write = False;
                        
                        M2C_UndoContinuationBegin(m2c);
                        if(DB_Table_Timesheets == ptbd_drag_drop.kind)
                        {
                            for(U64 payload_index = 0; payload_index < ptbd_drag_drop.count; payload_index += 1)
                            {
                                DB_Row *timesheet = &ptbd_drag_drop.data[payload_index];
                                
                                if(DB_InvoiceNumber_Draft == timesheet->invoice_number ||
                                   DB_InvoiceNumber_WrittenOff == timesheet->invoice_number)
                                {
                                    M2C_WriteTimesheet(m2c,
                                                       timesheet->id,
                                                       timesheet->date,
                                                       timesheet->employee_id,
                                                       timesheet->job_id,
                                                       timesheet->hours,
                                                       timesheet->miles,
                                                       timesheet->cost,
                                                       timesheet->description,
                                                       invoice->id);
                                    did_write = True;
                                }
                            }
                        }
                        else if(DB_Table_Jobs == ptbd_drag_drop.kind)
                        {
                            I64 job_id = ptbd_drag_drop.data->id;
                            for(U64 row_index = 0; row_index < timesheets.rows_count; row_index += 1)
                            {
                                DB_Row *timesheet = &timesheets.rows[row_index];
                                if(timesheet->job_id == job_id && 0 == timesheet->invoice_id)
                                {
                                    M2C_WriteTimesheet(m2c,
                                                       timesheet->id,
                                                       timesheet->date,
                                                       timesheet->employee_id,
                                                       timesheet->job_id,
                                                       timesheet->hours,
                                                       timesheet->miles,
                                                       timesheet->cost,
                                                       timesheet->description,
                                                       invoice->id);
                                    did_write = True;
                                }
                            }
                        }
                        else
                        {
                            OS_TCtxErrorPush(S8("Drag timesheets, or a job with uninvoiced hours, here to add them."));
                        }
                        M2C_UndoContinuationEnd(m2c);
                        
                        if(did_write)
                        {
                            M2C_Reload(m2c);
                        }
                        else
                        {
                            OS_TCtxErrorPush(S8("Drag uninvoiced timesheets here to add to this invoice."));
                        }
                    }
                    else
                    {
                        if(DB_Table_Timesheets == ptbd_drag_drop.kind ||
                           DB_Table_Jobs == ptbd_drag_drop.kind)
                        {
                            OS_TCtxErrorPush(S8("You can't add timesheets to a finished invoice."));
                        }
                        else
                        {
                            OS_TCtxErrorPush(S8("Drag timesheets, or a job with uninvoiced hours, to a draft invoice to add them."));
                        }
                    }
                }
                
                if(use.is_pressed)
                {
                    UI_NavInterfaceStackPush(&panel->key);
                }
                else
                {
                    UI_Node *active = UI_NodeLookup(&ui->active);
                    if(UI_NodeIsChildOf(active, panel) &&
                       UI_KeyMatch(&ui->hot, &ui->active) && 
                       UI_KeyMatch(&active->nav_context.key, &panel->key) &&
                       !G_WindowKeyStateGet(ui->window, G_Key_MouseButtonLeft))
                    {
                        UI_NavInterfaceStackPush(&panel->key);
                    }
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
                        should_close_editor = True;
                    }
                    else if(G_EventQueueHasKeyDown(events, G_Key_S, G_ModifierKeys_Ctrl, True))
                    {
                        should_save = True;
                    }
                    
                    if(should_close_editor)
                    {
                        UI_NavInterfaceStackPop();
                    }
                }
            }
            
            if(0 != next_to_edit && 0 != editing)
            {
                should_save = True;
            }
            
            if(should_save)
            {
                if(0 == editor_errors.count)
                {
                    should_close_editor = True;
                    
                    F64 cost = S8Parse1F64(S8FromBuilder(invoice_editor->cost));
                    S8 address = S8FromBuilder(invoice_editor->address);
                    S8 title = S8FromBuilder(invoice_editor->title);
                    S8 description = S8FromBuilder(invoice_editor->description);
                    F64 cost_to_write = (Abs1F(cost - editing->cost_calculated) < 0.01f) ? -1.0 : cost;
                    M2C_WriteInvoice(m2c, invoice_editor->id, address, title, description, 0, 0, cost_to_write);
                    M2C_Reload(m2c);
                }
                
                OS_TCtxErrorScopeMerge(&editor_errors);
            }
            
            if(should_close_editor)
            {
                invoice_editor->title.len = 0;
                invoice_editor->address.len = 0;
                invoice_editor->description.len = 0;
                invoice_editor->cost.len = 0;
                invoice_editor->id = -1;
            }
            
            if(0 != to_finish)
            {
                M2C_WriteInvoice(m2c,
                                 to_finish->id,
                                 to_finish->address,
                                 to_finish->title,
                                 to_finish->description,
                                 DB_InvoiceNumber_Next,
                                 DenseTimeFromDateTime(OS_NowLTC()),
                                 to_finish->cost_raw);
                M2C_Reload(m2c);
            }
            
            if(0 != next_to_edit)
            {
                invoice_editor->id = next_to_edit->id;
                invoice_editor->address.len = 0;
                S8BuilderAppend(&invoice_editor->address, next_to_edit->address);
                invoice_editor->title.len = 0;
                S8BuilderAppend(&invoice_editor->title, next_to_edit->title);
                invoice_editor->description.len = 0;
                S8BuilderAppend(&invoice_editor->description, next_to_edit->description);
                invoice_editor->cost.len = 0;
                if(next_to_edit->cost > 0)
                {
                    S8BuilderAppendFmt(&invoice_editor->cost, "%.2f", next_to_edit->cost);
                }
                else
                {
                    S8BuilderAppend(&invoice_editor->cost, S8("auto"));
                }
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
    
    G_EventQueue *events = G_WindowEventQueueGet(ui->window);
    
    if(is_top_of_interface_stack)
    {
        F32 s = G_WindowScaleFactorGet(ui->window);
        
        if(G_EventQueueHasKeyDown(events, G_Key_Tab, 0, True) ||
           G_EventQueueHasKeyDown(events, G_Key_Down, 0, True))
        {
            U64 scroll_to_row_index = PTBD_InvoicesListNavNext(index, invoices, keyboard_focus_id);
            scroll_region->target_view_offset.y =
                0.5f*scroll_region->computed_size.y - (scroll_to_row_index + 0.5f)*height_per_row*s;
        }
        else if(G_EventQueueHasKeyDown(events, G_Key_Tab, G_ModifierKeys_Shift, True) ||
                G_EventQueueHasKeyDown(events, G_Key_Up, 0, True))
        {
            U64 scroll_to_row_index = PTBD_InvoicesListNavPrev(index, invoices, keyboard_focus_id);
            scroll_region->target_view_offset.y =
                0.5f*scroll_region->computed_size.y - (scroll_to_row_index + 0.5f)*height_per_row*s;
        }
    }
    
    ArenaTempEnd(scratch);
}

Function void
PTBD_DraggingClear(void)
{
    ptbd_drag_drop.data = 0;
    ptbd_drag_drop.count = 0;
    ArenaClear(ptbd_drag_drop.arena);
}

Function void
PTBD_DraggingBuildUI(void)
{
    F32 s = G_WindowScaleFactorGet(ui->window);
    
    V2F cur = G_WindowMousePositionGet(ui->window);
    
    for(U64 payload_index = 0; payload_index < ptbd_drag_drop.count; payload_index += 1)
    {
        DB_Row *row = &ptbd_drag_drop.data[payload_index];
        
        UI_SetNextWidth(UI_SizeNone());
        UI_SetNextHeight(UI_SizeNone());
        UI_SetNextFixedRect(RectMake2F(cur, Scale2F(V2F(150.0f, 60.0f), s)));
        cur.y += 56.0f*s;
        UI_SetNextCornerRadius(16.0f);
        if(DB_Table_Employees == ptbd_drag_drop.kind)
        {
            UI_SetNextBgCol(PTBD_ColourFromEmployeeId(row->id));
        }
        else if(0 == row->invoice_id)
        {
            UI_SetNextBgCol(ptbd_grey);
        }
        else if(DB_InvoiceNumber_WrittenOff == row->invoice_number)
        {
            UI_SetNextBgCol(ColFromHex(0xbf616aff));
        }
        else
        {
            if(DB_InvoiceNumber_Draft == row->invoice_number)
            {
                UI_SetNextTexture(ptbd_gradient_vertical);
                UI_SetNextTextureRegion(Range2F(V2F(0.0f, 0.99f), V2F(1.0f, 0.01f)));
            }
            UI_SetNextBgCol(PTBD_ColourFromInvoiceId(row->invoice_id));
        }
        UI_SetNextChildrenLayout(Axis2_Y);
        UI_Node *dragged = UI_NodeMake(UI_Flags_DrawFill | UI_Flags_DrawDropShadow | UI_Flags_FixedFinalRect, S8(""));
        UI_Parent(dragged) UI_Width(UI_SizeFromText(16.0f, 1.0f)) UI_Height(UI_SizeFromText(2.0f, 1.0f)) UI_FontSize(16)
        {
            UI_Spacer(UI_SizePx(6.0f, 1.0f));
            
            if(DB_Table_Jobs == ptbd_drag_drop.kind)
            {
                UI_Font(ui_font_bold) UI_Label(row->job_number);
                UI_Label(row->job_name);
            }
            else if(DB_Table_Invoices == ptbd_drag_drop.kind)
            {
                UI_SetNextFont(ui_font_bold);
                if(DB_InvoiceNumber_Draft == row->invoice_number)
                {
                    UI_Label(S8("Draft invoice"));
                    UI_Label(row->title);
                }
                else if(DB_InvoiceNumber_WrittenOff == row->invoice_number)
                {
                    UI_Label(S8("Written off"));
                    UI_Label(S8("N/A"));
                }
                else
                {
                    UI_LabelFromFmt("Invoice no. %lld", row->invoice_number);
                    UI_Label(row->title);
                }
            }
            else if(DB_Table_Timesheets == ptbd_drag_drop.kind)
            {
                UI_Spacer(UI_SizePx(6.0f, 1.0f));
                UI_SetNextWidth(UI_SizeSum(1.0f));
                UI_SetNextHeight(UI_SizeSum(1.0f));
                UI_Row()
                    UI_Width(UI_SizeFromText(2.0f, 1.0f))
                {
                    UI_Spacer(UI_SizePx(6.0f, 1.0f));
                    UI_LabelFromIcon(FONT_Icon_Clock);
                    F32 hours = Floor1F(row->hours);
                    F32 minutes = 60.0f*Fract1F(row->hours);
                    UI_LabelFromFmt("%02.0f:%.0f", hours, minutes);
                    UI_Spacer(UI_SizePx(8.0f, 1.0f));
                    UI_LabelFromIcon(FONT_Icon_Cab);
                    UI_LabelFromFmt("%.1f", row->miles);
                }
                UI_Label(row->description);
            }
            else if(DB_Table_Employees == ptbd_drag_drop.kind)
            {
                UI_Font(ui_font_bold) UI_Label(row->employee_name);
                UI_LabelFromFmt("£%.2f/hour", row->rate);
            }
        }
    }
}

Function void
PTBD_DragSelection(DB_Table kind, DB_Cache data, PTBD_Selection *selection)
{
    PTBD_DraggingClear();
    
    ptbd_drag_drop.kind = kind;
    
    for(PTBD_Selection *chunk = selection; 0 != chunk; chunk = chunk->next)
    {
        for(U64 id_index = 0; id_index < chunk->cap; id_index += 1)
        {
            DB_Row *t = DB_RowFromId(data, chunk->ids[id_index]);
            ptbd_drag_drop.count += (0 != t);
        }
    }
    
    ptbd_drag_drop.data = PushArray(ptbd_drag_drop.arena, DB_Row, ptbd_drag_drop.count);
    
    U64 payload_index = 0;
    for(PTBD_Selection *chunk = selection; 0 != chunk; chunk = chunk->next)
    {
        for(U64 id_index = 0; id_index < chunk->cap; id_index += 1)
        {
            DB_Row *t = DB_RowFromId(data, chunk->ids[id_index]);
            if(0 != t)
            {
                ptbd_drag_drop.data[payload_index] = *t;
                payload_index += 1;
            }
        }
    }
    
    ui->hot = UI_KeyNil();
    ui->active = UI_KeyNil();
}

Function void
PTBD_DragRow(DB_Table kind, DB_Row data)
{
    PTBD_DraggingClear();
    
    ptbd_drag_drop.kind = kind;
    ptbd_drag_drop.count += 1;
    ptbd_drag_drop.data = PushArray(ptbd_drag_drop.arena, DB_Row, ptbd_drag_drop.count);
    ptbd_drag_drop.data[0] = data;
    
    ui->hot = UI_KeyNil();
    ui->active = UI_KeyNil();
}

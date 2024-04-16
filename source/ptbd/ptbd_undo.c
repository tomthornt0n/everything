
Function void
PTBD_DeletedRows(DB_Table kind, DB_Cache data, U64 ids_count, I64 *ids, B32 is_continuation)
{
    ArenaTemp checkpoint = ArenaTempBegin(ptbd_undo_arena);
    
    U64 rows_count = 0;
    DB_Row *rows = PushArray(ptbd_undo_arena, DB_Row, ids_count);
    for(U64 id_index = 0; id_index < ids_count; id_index += 1)
    {
        DB_Row *row = DB_RowFromId(data, ids[id_index]);
        if(0 != row)
        {
            rows[rows_count] = *row;
            rows[rows_count].job_number = S8Clone(ptbd_undo_arena, row->job_number);
            rows[rows_count].job_name = S8Clone(ptbd_undo_arena, row->job_name);
            rows[rows_count].employee_name = S8Clone(ptbd_undo_arena, row->employee_name);
            rows[rows_count].description = S8Clone(ptbd_undo_arena, row->description);
            rows[rows_count].address = S8Clone(ptbd_undo_arena, row->address);
            rows[rows_count].title = S8Clone(ptbd_undo_arena, row->title);
            rows_count += 1;
        }
    }
    
    PTBD_UndoAction *action = PushArray(ptbd_undo_arena, PTBD_UndoAction, 1);
    action->next = ptbd_undo;
    action->checkpoint = checkpoint;
    action->is_continuation = is_continuation;
    action->table = kind;
    action->before.rows_count = rows_count;
    action->before.rows = rows;
    ptbd_undo = action;
    
    ptbd_redo = 0;
    ArenaClear(ptbd_redo_arena);
}

Function void
PTBD_EditedRow(DB_Table kind, DB_Row before, DB_Row after, B32 is_continuation)
{
    ArenaTemp checkpoint = ArenaTempBegin(ptbd_undo_arena);
    
    PTBD_UndoAction *action = PushArray(ptbd_undo_arena, PTBD_UndoAction, 1);
    action->next = ptbd_undo;
    action->checkpoint = checkpoint;
    action->is_continuation = is_continuation;
    action->table = kind;
    action->before.rows_count = 1;
    action->before.rows = PushArray(ptbd_undo_arena, DB_Row, 1);
    action->before.rows[0] = before;
    action->before.rows[0].job_number = S8Clone(ptbd_undo_arena, before.job_number);
    action->before.rows[0].job_name = S8Clone(ptbd_undo_arena, before.job_name);
    action->before.rows[0].employee_name = S8Clone(ptbd_undo_arena, before.employee_name);
    action->before.rows[0].description = S8Clone(ptbd_undo_arena, before.description);
    action->before.rows[0].address = S8Clone(ptbd_undo_arena, before.address);
    action->before.rows[0].title = S8Clone(ptbd_undo_arena, before.title);
    action->after.rows_count = 1;
    action->after.rows = PushArray(ptbd_undo_arena, DB_Row, 1);
    action->after.rows[0] = after;
    action->after.rows[0].job_number = S8Clone(ptbd_undo_arena, after.job_number);
    action->after.rows[0].job_name = S8Clone(ptbd_undo_arena, after.job_name);
    action->after.rows[0].employee_name = S8Clone(ptbd_undo_arena, after.employee_name);
    action->after.rows[0].description = S8Clone(ptbd_undo_arena, after.description);
    action->after.rows[0].address = S8Clone(ptbd_undo_arena, after.address);
    action->after.rows[0].title = S8Clone(ptbd_undo_arena, after.title);
    ptbd_undo = action;
    
    ptbd_redo = 0;
    ArenaClear(ptbd_redo_arena);
}

Function void
PTBD_InsertedNewRow(DB_Table kind, DB_Row row, B32 is_continuation)
{
    ArenaTemp checkpoint = ArenaTempBegin(ptbd_undo_arena);
    
    PTBD_UndoAction *action = PushArray(ptbd_undo_arena, PTBD_UndoAction, 1);
    action->next = ptbd_undo;
    action->checkpoint = checkpoint;
    action->is_continuation = is_continuation;
    action->table = kind;
    action->after.rows_count = 1;
    action->after.rows = PushArray(ptbd_undo_arena, DB_Row, 1);
    action->after.rows[0] = row;
    action->after.rows[0].job_number = S8Clone(ptbd_undo_arena, row.job_number);
    action->after.rows[0].job_name = S8Clone(ptbd_undo_arena, row.job_name);
    action->after.rows[0].employee_name = S8Clone(ptbd_undo_arena, row.employee_name);
    action->after.rows[0].description = S8Clone(ptbd_undo_arena, row.description);
    action->after.rows[0].address = S8Clone(ptbd_undo_arena, row.address);
    action->after.rows[0].title = S8Clone(ptbd_undo_arena, row.title);
    ptbd_undo = action;
    
    ptbd_redo = 0;
    ArenaClear(ptbd_redo_arena);
}

Function void
PTBD_Undo(PTBD_MsgQueue *m2c, PTBD_Inspectors *inspectors)
{
    OS_SemaphoreWait(ptbd_undo_lock);
    
    B32 did_something = False;
    
    for(;;)
    {
        PTBD_UndoAction *action = ptbd_undo;
        if(0 == action)
        {
            break;
        }
        else
        {
            M2C_UndoRestoreBegin(m2c);
            if(0 == action->before.rows_count)
            {
                ArenaTemp scratch = OS_TCtxScratchBegin(0, 0);
                U64 *ids = PushArray(scratch.arena, U64, action->after.rows_count);
                for(U64 row_index = 0; row_index < action->after.rows_count; row_index += 1)
                {
                    ids[row_index] = action->after.rows[row_index].id;
                }
                M2C_DeleteRows(m2c, action->table, action->after.rows_count, ids);
                did_something = True;
                ArenaTempEnd(scratch);
            }
            else
            {
                for(U64 row_index = 0; row_index < action->before.rows_count; row_index += 1)
                {
                    DB_Row *row = &action->before.rows[row_index];
                    if(DB_Table_Jobs == action->table)
                    {
                        M2C_WriteJob(m2c, row->id, row->job_number, row->job_name);
                        did_something = True;
                    }
                    else if(DB_Table_Timesheets == action->table)
                    {
                        M2C_WriteTimesheet(m2c, row->id, row->date, row->employee_id, row->job_id, row->hours, row->miles, row->cost_raw, row->description, row->invoice_id);
                        PTBD_InspectorFieldsFromTimesheet(PTBD_InspectorFromId(inspectors, row->id), row);
                        did_something = True;
                    }
                    else if(DB_Table_Employees == action->table)
                    {
                        M2C_WriteEmployee(m2c, row->id, row->employee_name, row->rate);
                        did_something = True;
                    }
                    else if(DB_Table_Invoices == action->table)
                    {
                        M2C_WriteInvoice(m2c, row->id, row->address, row->title, row->description, row->invoice_number, row->date, row->cost);
                        did_something = True;
                    }
                }
            }
            M2C_UndoRestoreEnd(m2c);
            
            ptbd_undo = action->next;
            ArenaTemp free_undo_to = action->checkpoint;
            ArenaTemp checkpoint = ArenaTempBegin(ptbd_redo_arena);
            PTBD_UndoAction *redo = PushArray(ptbd_redo_arena, PTBD_UndoAction, 1);
            redo->next = ptbd_redo;
            redo->checkpoint = checkpoint;
            redo->is_continuation = action->is_continuation;
            redo->table = action->table;
            PTBD_CopyDBCache(ptbd_redo_arena, &redo->before, &action->before);
            PTBD_CopyDBCache(ptbd_redo_arena, &redo->after, &action->after);
            ptbd_redo = redo;
            B32 is_continuation = action->is_continuation;
            ArenaTempEnd(free_undo_to);
            
            if(!is_continuation)
            {
                break;
            }
        }
    }
    
    if(did_something)
    {
        M2C_Reload(m2c);
    }
    
    OS_SemaphoreSignal(ptbd_undo_lock);
}

Function void
PTBD_Redo(PTBD_MsgQueue *m2c, PTBD_Inspectors *inspectors)
{
    OS_SemaphoreWait(ptbd_undo_lock);
    
    B32 did_something = False;
    
    for(;;)
    {
        PTBD_UndoAction *action = ptbd_redo;
        if(0 == action)
        {
            break;
        }
        else
        {
            M2C_UndoRestoreBegin(m2c);
            if(0 == action->after.rows_count)
            {
                ArenaTemp scratch = OS_TCtxScratchBegin(0, 0);
                U64 *ids = PushArray(scratch.arena, U64, action->before.rows_count);
                for(U64 row_index = 0; row_index < action->before.rows_count; row_index += 1)
                {
                    ids[row_index] = action->before.rows[row_index].id;
                }
                M2C_DeleteRows(m2c, action->table, action->before.rows_count, ids);
                did_something = True;
                ArenaTempEnd(scratch);
            }
            else
            {
                for(U64 row_index = 0; row_index < action->after.rows_count; row_index += 1)
                {
                    DB_Row *row = &action->after.rows[row_index];
                    if(DB_Table_Jobs == action->table)
                    {
                        M2C_WriteJob(m2c, row->id, row->job_number, row->job_name);
                        did_something = True;
                    }
                    else if(DB_Table_Timesheets == action->table)
                    {
                        M2C_WriteTimesheet(m2c, row->id, row->date, row->employee_id, row->job_id, row->hours, row->miles, row->cost_raw, row->description, row->invoice_id);
                        PTBD_InspectorFieldsFromTimesheet(PTBD_InspectorFromId(inspectors, row->id), row);
                        did_something = True;
                    }
                    else if(DB_Table_Employees == action->table)
                    {
                        M2C_WriteEmployee(m2c, row->id, row->employee_name, row->rate);
                        did_something = True;
                    }
                    else if(DB_Table_Invoices == action->table)
                    {
                        M2C_WriteInvoice(m2c, row->id, row->address, row->title, row->description, row->invoice_number, row->date, row->cost);
                        did_something = True;
                    }
                }
            }
            M2C_UndoRestoreEnd(m2c);
            
            ptbd_redo = action->next;
            ArenaTemp free_redo_to = action->checkpoint;
            ArenaTemp checkpoint = ArenaTempBegin(ptbd_undo_arena);
            PTBD_UndoAction *undo = PushArray(ptbd_undo_arena, PTBD_UndoAction, 1);
            undo->next = ptbd_undo;
            undo->checkpoint = checkpoint;
            undo->is_continuation = action->is_continuation;
            undo->table = action->table;
            PTBD_CopyDBCache(ptbd_undo_arena, &undo->before, &action->before);
            PTBD_CopyDBCache(ptbd_undo_arena, &undo->after, &action->after);
            ptbd_undo = undo;
            ArenaTempEnd(free_redo_to);
            
            if(0 == ptbd_redo || !ptbd_redo->is_continuation)
            {
                break;
            }
        }
    }
    
    if(did_something)
    {
        M2C_Reload(m2c);
    }
    
    OS_SemaphoreSignal(ptbd_undo_lock);
}

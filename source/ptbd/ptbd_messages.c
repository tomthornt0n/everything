
Function void
M2C_Reload(PTBD_MsgQueue *queue)
{
    OS_SemaphoreWait(queue->lock);
    {
        M2C_Kind kind = M2C_Kind_Reload;
        queue->write_pos += RingWrite(queue->buffer, sizeof(queue->buffer), queue->write_pos, &kind, sizeof(kind));
    }
    OS_SemaphoreSignal(queue->lock);
    
    OS_SemaphoreSignal(queue->sem);
}

Function void
M2C_Refresh(PTBD_MsgQueue *queue)
{
    OS_SemaphoreWait(queue->lock);
    {
        M2C_Kind kind = M2C_Kind_Refresh;
        queue->write_pos += RingWrite(queue->buffer, sizeof(queue->buffer), queue->write_pos, &kind, sizeof(kind));
    }
    OS_SemaphoreSignal(queue->lock);
    
    OS_SemaphoreSignal(queue->sem);
}

Function void
M2C_WriteTimesheet(PTBD_MsgQueue *queue,
                   I64 id,
                   DenseTime date,
                   I64 employee_id,
                   I64 job_id,
                   F64 hours,
                   F64 miles,
                   F64 cost,
                   S8 description,
                   I64 invoice_id)
{
    OS_SemaphoreWait(queue->lock);
    {
        M2C_Kind kind = M2C_Kind_WriteTimesheet;
        queue->write_pos += RingWrite(queue->buffer, sizeof(queue->buffer), queue->write_pos, &kind, sizeof(kind));
        queue->write_pos += RingWrite(queue->buffer, sizeof(queue->buffer), queue->write_pos, &id, sizeof(id));
        queue->write_pos += RingWrite(queue->buffer, sizeof(queue->buffer), queue->write_pos, &date, sizeof(date));
        queue->write_pos += RingWrite(queue->buffer, sizeof(queue->buffer), queue->write_pos, &employee_id, sizeof(employee_id));
        queue->write_pos += RingWrite(queue->buffer, sizeof(queue->buffer), queue->write_pos, &job_id, sizeof(job_id));
        queue->write_pos += RingWrite(queue->buffer, sizeof(queue->buffer), queue->write_pos, &cost, sizeof(cost));
        if(cost <= 0.0)
        {
            queue->write_pos += RingWrite(queue->buffer, sizeof(queue->buffer), queue->write_pos, &hours, sizeof(hours));
            queue->write_pos += RingWrite(queue->buffer, sizeof(queue->buffer), queue->write_pos, &miles, sizeof(miles));
        }
        queue->write_pos += RingWrite(queue->buffer, sizeof(queue->buffer), queue->write_pos, &description.len, sizeof(description.len));
        queue->write_pos += RingWrite(queue->buffer, sizeof(queue->buffer), queue->write_pos, description.buffer, description.len);
        queue->write_pos += RingWrite(queue->buffer, sizeof(queue->buffer), queue->write_pos, &invoice_id, sizeof(invoice_id));
    }
    OS_SemaphoreSignal(queue->lock);
    
    OS_SemaphoreSignal(queue->sem);
}

Function void
M2C_WriteJob(PTBD_MsgQueue *queue, I64 id, S8 number, S8 name)
{
    OS_SemaphoreWait(queue->lock);
    {
        M2C_Kind kind = M2C_Kind_WriteJob;
        queue->write_pos += RingWrite(queue->buffer, sizeof(queue->buffer), queue->write_pos, &kind, sizeof(kind));
        queue->write_pos += RingWrite(queue->buffer, sizeof(queue->buffer), queue->write_pos, &id, sizeof(id));
        queue->write_pos += RingWrite(queue->buffer, sizeof(queue->buffer), queue->write_pos, &number.len, sizeof(number.len));
        queue->write_pos += RingWrite(queue->buffer, sizeof(queue->buffer), queue->write_pos, number.buffer, number.len);
        queue->write_pos += RingWrite(queue->buffer, sizeof(queue->buffer), queue->write_pos, &name.len, sizeof(name.len));
        queue->write_pos += RingWrite(queue->buffer, sizeof(queue->buffer), queue->write_pos, name.buffer, name.len);
    }
    OS_SemaphoreSignal(queue->lock);
    
    OS_SemaphoreSignal(queue->sem);
}

Function void
M2C_WriteInvoice(PTBD_MsgQueue *queue, I64 id, S8 address, S8 title, S8 description, I64 number, DenseTime date, F64 cost)
{
    OS_SemaphoreWait(queue->lock);
    {
        M2C_Kind kind = M2C_Kind_WriteInvoice;
        queue->write_pos += RingWrite(queue->buffer, sizeof(queue->buffer), queue->write_pos, &kind, sizeof(kind));
        queue->write_pos += RingWrite(queue->buffer, sizeof(queue->buffer), queue->write_pos, &id, sizeof(id));
        queue->write_pos += RingWrite(queue->buffer, sizeof(queue->buffer), queue->write_pos, &address.len, sizeof(address.len));
        queue->write_pos += RingWrite(queue->buffer, sizeof(queue->buffer), queue->write_pos, address.buffer, address.len);
        queue->write_pos += RingWrite(queue->buffer, sizeof(queue->buffer), queue->write_pos, &title.len, sizeof(title.len));
        queue->write_pos += RingWrite(queue->buffer, sizeof(queue->buffer), queue->write_pos, title.buffer, title.len);
        queue->write_pos += RingWrite(queue->buffer, sizeof(queue->buffer), queue->write_pos, &description.len, sizeof(description.len));
        queue->write_pos += RingWrite(queue->buffer, sizeof(queue->buffer), queue->write_pos, description.buffer, description.len);
        queue->write_pos += RingWrite(queue->buffer, sizeof(queue->buffer), queue->write_pos, &number, sizeof(number));
        queue->write_pos += RingWrite(queue->buffer, sizeof(queue->buffer), queue->write_pos, &date, sizeof(date));
        queue->write_pos += RingWrite(queue->buffer, sizeof(queue->buffer), queue->write_pos, &cost, sizeof(cost));
    }
    OS_SemaphoreSignal(queue->lock);
    
    OS_SemaphoreSignal(queue->sem);
}

Function void
M2C_WriteEmployee(PTBD_MsgQueue *queue, I64 id, S8 name, F64 rate)
{
    OS_SemaphoreWait(queue->lock);
    {
        M2C_Kind kind = M2C_Kind_WriteEmployee;
        queue->write_pos += RingWrite(queue->buffer, sizeof(queue->buffer), queue->write_pos, &kind, sizeof(kind));
        queue->write_pos += RingWrite(queue->buffer, sizeof(queue->buffer), queue->write_pos, &id, sizeof(id));
        queue->write_pos += RingWrite(queue->buffer, sizeof(queue->buffer), queue->write_pos, &name.len, sizeof(name.len));
        queue->write_pos += RingWrite(queue->buffer, sizeof(queue->buffer), queue->write_pos, name.buffer, name.len);
        queue->write_pos += RingWrite(queue->buffer, sizeof(queue->buffer), queue->write_pos, &rate, sizeof(rate));
    }
    OS_SemaphoreSignal(queue->lock);
    
    OS_SemaphoreSignal(queue->sem);
}

Function void
M2C_NewInvoiceForTimesheets(PTBD_MsgQueue *queue, U64 count, I64 *ids)
{
    OS_SemaphoreWait(queue->lock);
    {
        M2C_Kind kind = M2C_Kind_NewInvoiceForTimesheets;
        queue->write_pos += RingWrite(queue->buffer, sizeof(queue->buffer), queue->write_pos, &kind, sizeof(kind));
        queue->write_pos += RingWrite(queue->buffer, sizeof(queue->buffer), queue->write_pos, &count, sizeof(count));
        queue->write_pos += RingWrite(queue->buffer, sizeof(queue->buffer), queue->write_pos, ids, count*sizeof(ids[0]));
    }
    OS_SemaphoreSignal(queue->lock);
    
    OS_SemaphoreSignal(queue->sem);
}

Function void
M2C_NewInvoiceForJob(PTBD_MsgQueue *queue, I64 id)
{
    OS_SemaphoreWait(queue->lock);
    {
        M2C_Kind kind = M2C_Kind_NewInvoiceForJob;
        queue->write_pos += RingWrite(queue->buffer, sizeof(queue->buffer), queue->write_pos, &kind, sizeof(kind));
        queue->write_pos += RingWrite(queue->buffer, sizeof(queue->buffer), queue->write_pos, &id, sizeof(id));
    }
    OS_SemaphoreSignal(queue->lock);
    
    OS_SemaphoreSignal(queue->sem);
}

Function void
M2C_DeleteRow(PTBD_MsgQueue *queue, DB_Table table, I64 id)
{
    OS_SemaphoreWait(queue->lock);
    {
        M2C_Kind kind = M2C_Kind_DeleteRecords;
        queue->write_pos += RingWrite(queue->buffer, sizeof(queue->buffer), queue->write_pos, &kind, sizeof(kind));
        queue->write_pos += RingWrite(queue->buffer, sizeof(queue->buffer), queue->write_pos, &table, sizeof(table));
        queue->write_pos += RingWrite(queue->buffer, sizeof(queue->buffer), queue->write_pos, (U64[]){1}, sizeof(U64));
        queue->write_pos += RingWrite(queue->buffer, sizeof(queue->buffer), queue->write_pos, &id, sizeof(id));
    }
    OS_SemaphoreSignal(queue->lock);
    
    OS_SemaphoreSignal(queue->sem);
}

Function void
M2C_DeleteRows(PTBD_MsgQueue *queue, DB_Table table, U64 ids_count, I64 *ids)
{
    OS_SemaphoreWait(queue->lock);
    {
        M2C_Kind kind = M2C_Kind_DeleteRecords;
        queue->write_pos += RingWrite(queue->buffer, sizeof(queue->buffer), queue->write_pos, &kind, sizeof(kind));
        queue->write_pos += RingWrite(queue->buffer, sizeof(queue->buffer), queue->write_pos, &table, sizeof(table));
        queue->write_pos += RingWrite(queue->buffer, sizeof(queue->buffer), queue->write_pos, &ids_count, sizeof(ids_count));
        queue->write_pos += RingWrite(queue->buffer, sizeof(queue->buffer), queue->write_pos, ids, sizeof(ids[0])*ids_count);
    }
    OS_SemaphoreSignal(queue->lock);
    
    OS_SemaphoreSignal(queue->sem);
}

Function void
M2C_DeleteSelectedRows(PTBD_MsgQueue *queue, DB_Table table, PTBD_Selection *selection)
{
    U64 count = 0;
    for(PTBD_Selection *chunk = selection; 0 != chunk; chunk = chunk->next)
    {
        for(U64 id_index = 0; id_index < chunk->cap; id_index += 1)
        {
            if(chunk->ids[id_index] > 0)
            {
                count += 1;
            }
        }
    }
    
    OS_SemaphoreWait(queue->lock);
    {
        M2C_Kind kind = M2C_Kind_DeleteRecords;
        queue->write_pos += RingWrite(queue->buffer, sizeof(queue->buffer), queue->write_pos, &kind, sizeof(kind));
        queue->write_pos += RingWrite(queue->buffer, sizeof(queue->buffer), queue->write_pos, &table, sizeof(table));
        queue->write_pos += RingWrite(queue->buffer, sizeof(queue->buffer), queue->write_pos, &count, sizeof(count));
        for(PTBD_Selection *chunk = selection; 0 != chunk; chunk = chunk->next)
        {
            for(U64 id_index = 0; id_index < chunk->cap; id_index += 1)
            {
                if(chunk->ids[id_index] > 0)
                {
                    queue->write_pos += RingWrite(queue->buffer, sizeof(queue->buffer), queue->write_pos, &chunk->ids[id_index], sizeof(chunk->ids[id_index]));
                }
            }
        }
    }
    OS_SemaphoreSignal(queue->lock);
    
    OS_SemaphoreSignal(queue->sem);
}

Function void
M2C_ExportInvoice(PTBD_MsgQueue *queue, I64 id)
{
    OS_SemaphoreWait(queue->lock);
    {
        M2C_Kind kind = M2C_Kind_ExportInvoice;
        queue->write_pos += RingWrite(queue->buffer, sizeof(queue->buffer), queue->write_pos, &kind, sizeof(kind));
        queue->write_pos += RingWrite(queue->buffer, sizeof(queue->buffer), queue->write_pos, &id, sizeof(id));
    }
    OS_SemaphoreSignal(queue->lock);
    
    OS_SemaphoreSignal(queue->sem);
}

Function void
M2C_SetSearch(PTBD_MsgQueue *queue, S8 search)
{
    OS_SemaphoreWait(queue->lock);
    {
        M2C_Kind kind = M2C_Kind_SetSearch;
        queue->write_pos += RingWrite(queue->buffer, sizeof(queue->buffer), queue->write_pos, &kind, sizeof(kind));
        queue->write_pos += RingWrite(queue->buffer, sizeof(queue->buffer), queue->write_pos, &search.len, sizeof(search.len));
        queue->write_pos += RingWrite(queue->buffer, sizeof(queue->buffer), queue->write_pos, search.buffer, search.len);
    }
    OS_SemaphoreSignal(queue->lock);
    
    OS_SemaphoreSignal(queue->sem);
}

Function void
M2C_UndoRestoreBegin(PTBD_MsgQueue *queue)
{
    OS_SemaphoreWait(queue->lock);
    {
        M2C_Kind kind = M2C_Kind_UndoRestoreBegin;
        queue->write_pos += RingWrite(queue->buffer, sizeof(queue->buffer), queue->write_pos, &kind, sizeof(kind));
    }
    OS_SemaphoreSignal(queue->lock);
    
    OS_SemaphoreSignal(queue->sem);
}

Function void
M2C_UndoRestoreEnd(PTBD_MsgQueue *queue)
{
    OS_SemaphoreWait(queue->lock);
    {
        M2C_Kind kind = M2C_Kind_UndoRestoreEnd;
        queue->write_pos += RingWrite(queue->buffer, sizeof(queue->buffer), queue->write_pos, &kind, sizeof(kind));
    }
    OS_SemaphoreSignal(queue->lock);
    
    OS_SemaphoreSignal(queue->sem);
}

Function void
M2C_UndoContinuationBegin(PTBD_MsgQueue *queue)
{
    OS_SemaphoreWait(queue->lock);
    {
        M2C_Kind kind = M2C_Kind_UndoContinuationBegin;
        queue->write_pos += RingWrite(queue->buffer, sizeof(queue->buffer), queue->write_pos, &kind, sizeof(kind));
    }
    OS_SemaphoreSignal(queue->lock);
    
    OS_SemaphoreSignal(queue->sem);
}

Function void
M2C_UndoContinuationEnd(PTBD_MsgQueue *queue)
{
    OS_SemaphoreWait(queue->lock);
    {
        M2C_Kind kind = M2C_Kind_UndoContinuationEnd;
        queue->write_pos += RingWrite(queue->buffer, sizeof(queue->buffer), queue->write_pos, &kind, sizeof(kind));
    }
    OS_SemaphoreSignal(queue->lock);
    
    OS_SemaphoreSignal(queue->sem);
}

Function void
M2C_DidCopyState(PTBD_MsgQueue *queue)
{
    OS_SemaphoreWait(queue->lock);
    {
        M2C_Kind kind = M2C_Kind_DidCopyState;
        queue->write_pos += RingWrite(queue->buffer, sizeof(queue->buffer), queue->write_pos, &kind, sizeof(kind));
    }
    OS_SemaphoreSignal(queue->lock);
    
    OS_SemaphoreSignal(queue->sem);
}

Function void
C2M_WroteNewRecord(PTBD_MsgQueue *queue, DB_Table table, I64 id)
{
    OS_SemaphoreWait(queue->lock);
    {
        C2M_Kind kind = C2M_Kind_WroteNewRecord;
        queue->write_pos += RingWrite(queue->buffer, sizeof(queue->buffer), queue->write_pos, &kind, sizeof(kind));
        queue->write_pos += RingWrite(queue->buffer, sizeof(queue->buffer), queue->write_pos, &table, sizeof(table));
        queue->write_pos += RingWrite(queue->buffer, sizeof(queue->buffer), queue->write_pos, &id, sizeof(id));
    }
    OS_SemaphoreSignal(queue->lock);
}

Function PTBD_Cmd
PTBD_CmdPop(Arena *arena, PTBD_MsgQueue *queue)
{
    PTBD_Cmd result = {0};
    
    OS_SemaphoreWait(queue->sem);
    
    OS_SemaphoreWait(queue->lock);
    {
        queue->read_pos += RingRead(queue->buffer, sizeof(queue->buffer), queue->read_pos, &result.kind, sizeof(result.kind));
        
        if(M2C_Kind_WriteTimesheet == result.kind)
        {
            queue->read_pos += RingRead(queue->buffer, sizeof(queue->buffer), queue->read_pos, &result.id, sizeof(result.id));
            queue->read_pos += RingRead(queue->buffer, sizeof(queue->buffer), queue->read_pos, &result.date, sizeof(result.date));
            queue->read_pos += RingRead(queue->buffer, sizeof(queue->buffer), queue->read_pos, &result.employee_id, sizeof(result.employee_id));
            queue->read_pos += RingRead(queue->buffer, sizeof(queue->buffer), queue->read_pos, &result.job_id, sizeof(result.job_id));
            queue->read_pos += RingRead(queue->buffer, sizeof(queue->buffer), queue->read_pos, &result.cost, sizeof(result.cost));
            if(result.cost <= 0.0)
            {
                queue->read_pos += RingRead(queue->buffer, sizeof(queue->buffer), queue->read_pos, &result.hours, sizeof(result.hours));
                queue->read_pos += RingRead(queue->buffer, sizeof(queue->buffer), queue->read_pos, &result.miles, sizeof(result.miles));
            }
            queue->read_pos += RingRead(queue->buffer, sizeof(queue->buffer), queue->read_pos, &result.description.len, sizeof(result.description.len));
            char *description = PushArray(arena, char, result.description.len);
            queue->read_pos += RingRead(queue->buffer, sizeof(queue->buffer), queue->read_pos, description, result.description.len);
            result.description.buffer = description;
            queue->read_pos += RingRead(queue->buffer, sizeof(queue->buffer), queue->read_pos, &result.invoice_id, sizeof(result.invoice_id));
        }
        else if(M2C_Kind_WriteJob == result.kind)
        {
            queue->read_pos += RingRead(queue->buffer, sizeof(queue->buffer), queue->read_pos, &result.id, sizeof(result.id));
            queue->read_pos += RingRead(queue->buffer, sizeof(queue->buffer), queue->read_pos, &result.job_number.len, sizeof(result.job_number.len));
            char *job_number = PushArray(arena, char, result.job_number.len);
            queue->read_pos += RingRead(queue->buffer, sizeof(queue->buffer), queue->read_pos, job_number, result.job_number.len);
            result.job_number.buffer = job_number;
            queue->read_pos += RingRead(queue->buffer, sizeof(queue->buffer), queue->read_pos, &result.job_name.len, sizeof(result.job_name.len));
            char *job_name = PushArray(arena, char, result.job_name.len);
            queue->read_pos += RingRead(queue->buffer, sizeof(queue->buffer), queue->read_pos, job_name, result.job_name.len);
            result.job_name.buffer = job_name;
        }
        else if(M2C_Kind_WriteInvoice == result.kind)
        {
            queue->read_pos += RingRead(queue->buffer, sizeof(queue->buffer), queue->read_pos, &result.id, sizeof(result.id));
            queue->read_pos += RingRead(queue->buffer, sizeof(queue->buffer), queue->read_pos, &result.address.len, sizeof(result.address.len));
            char *address = PushArray(arena, char, result.address.len);
            queue->read_pos += RingRead(queue->buffer, sizeof(queue->buffer), queue->read_pos, address, result.address.len);
            result.address.buffer = address;
            queue->read_pos += RingRead(queue->buffer, sizeof(queue->buffer), queue->read_pos, &result.title.len, sizeof(result.title.len));
            char *title = PushArray(arena, char, result.title.len);
            queue->read_pos += RingRead(queue->buffer, sizeof(queue->buffer), queue->read_pos, title, result.title.len);
            result.title.buffer = title;
            queue->read_pos += RingRead(queue->buffer, sizeof(queue->buffer), queue->read_pos, &result.description.len, sizeof(result.description.len));
            char *description = PushArray(arena, char, result.description.len);
            queue->read_pos += RingRead(queue->buffer, sizeof(queue->buffer), queue->read_pos, description, result.description.len);
            result.description.buffer = description;
            queue->read_pos += RingRead(queue->buffer, sizeof(queue->buffer), queue->read_pos, &result.invoice_number, sizeof(result.invoice_number));
            queue->read_pos += RingRead(queue->buffer, sizeof(queue->buffer), queue->read_pos, &result.date, sizeof(result.date));
            queue->read_pos += RingRead(queue->buffer, sizeof(queue->buffer), queue->read_pos, &result.cost, sizeof(result.cost));
        }
        else if(M2C_Kind_WriteEmployee == result.kind)
        {
            queue->read_pos += RingRead(queue->buffer, sizeof(queue->buffer), queue->read_pos, &result.id, sizeof(result.id));
            queue->read_pos += RingRead(queue->buffer, sizeof(queue->buffer), queue->read_pos, &result.employee_name.len, sizeof(result.employee_name.len));
            char *employee_name = PushArray(arena, char, result.employee_name.len);
            queue->read_pos += RingRead(queue->buffer, sizeof(queue->buffer), queue->read_pos, employee_name, result.employee_name.len);
            result.employee_name.buffer = employee_name;
            queue->read_pos += RingRead(queue->buffer, sizeof(queue->buffer), queue->read_pos, &result.rate, sizeof(result.rate));
        }
        else if(M2C_Kind_NewInvoiceForTimesheets == result.kind)
        {
            queue->read_pos += RingRead(queue->buffer, sizeof(queue->buffer), queue->read_pos, &result.ids_count, sizeof(result.ids_count));
            result.ids = PushArray(arena, U64, result.ids_count);
            queue->read_pos += RingRead(queue->buffer, sizeof(queue->buffer), queue->read_pos, result.ids, sizeof(result.ids[0])*result.ids_count);
        }
        else if(M2C_Kind_NewInvoiceForJob == result.kind)
        {
            queue->read_pos += RingRead(queue->buffer, sizeof(queue->buffer), queue->read_pos, &result.job_id, sizeof(result.job_id));
        }
        else if(M2C_Kind_DeleteRecords == result.kind)
        {
            queue->read_pos += RingRead(queue->buffer, sizeof(queue->buffer), queue->read_pos, &result.table, sizeof(result.table));
            queue->read_pos += RingRead(queue->buffer, sizeof(queue->buffer), queue->read_pos, &result.ids_count, sizeof(result.ids_count));
            result.ids = PushArray(arena, U64, result.ids_count);
            queue->read_pos += RingRead(queue->buffer, sizeof(queue->buffer), queue->read_pos, result.ids, sizeof(result.ids[0])*result.ids_count);
        }
        else if(M2C_Kind_ExportInvoice == result.kind)
        {
            queue->read_pos += RingRead(queue->buffer, sizeof(queue->buffer), queue->read_pos, &result.id, sizeof(result.id));
        }
        else if(M2C_Kind_SetSearch == result.kind)
        {
            queue->read_pos += RingRead(queue->buffer, sizeof(queue->buffer), queue->read_pos, &result.search.len, sizeof(result.search.len));
            char *search = PushArray(arena, char, result.search.len);
            queue->read_pos += RingRead(queue->buffer, sizeof(queue->buffer), queue->read_pos, search, result.search.len);
            result.search.buffer = search;
        }
    }
    OS_SemaphoreSignal(queue->lock);
    
    return result;
}

Function void
PTBD_PushUndoActionForWriteCmd(DB_Table table, DB_Cache data[DB_Table_MAX], I64 id, DB_Row new_row, PTBD_CmdUndoState *undo_state)
{
    if(*undo_state != PTBD_CmdUndoState_Restore)
    {
        DB_Row *old_row = DB_RowFromId(data[table], id);
        
        if(PTBD_CmdUndoState_Normal == *undo_state ||
           PTBD_CmdUndoState_ContinuationFirst == *undo_state)
        {
            OS_SemaphoreWait(ptbd_undo_lock);
        }
        
        if(0 == old_row)
        {
            PTBD_InsertedNewRow(table, new_row, (PTBD_CmdUndoState_Continuation == *undo_state));
        }
        else
        {
            PTBD_EditedRow(table, *old_row, new_row, (PTBD_CmdUndoState_Continuation == *undo_state));
        }
        
        if(PTBD_CmdUndoState_ContinuationFirst == *undo_state)
        {
            *undo_state = PTBD_CmdUndoState_Continuation;
        }
        
        if(PTBD_CmdUndoState_Normal == *undo_state)
        {
            OS_SemaphoreSignal(ptbd_undo_lock);
        }
    }
}

Function void
PTBD_CmdExec(PTBD_ArenasForCriticalState arenas,
             sqlite3 *db,
             OpaqueHandle lock,
             B32 volatile *is_next_state_dirty,
             PTBD_CriticalState *state,
             PTBD_Cmd cmd,
             PTBD_MsgQueue *c2m,
             PTBD_CmdUndoState *undo_state)
{
    if(M2C_Kind_Reload == cmd.kind)
    {
        OS_SemaphoreWait(lock);
        {
            OS_InterlockedExchange1I(is_next_state_dirty, True);
            PTBD_Reload(arenas.cache_arena, db, state->cache);
            PTBD_Refresh(arenas.view_arena, state);
        }
        OS_SemaphoreSignal(lock);
    }
    else if(M2C_Kind_Refresh == cmd.kind)
    {
        OS_SemaphoreWait(lock);
        {
            OS_InterlockedExchange1I(is_next_state_dirty, True);
            PTBD_Refresh(arenas.view_arena, state);
        }
        OS_SemaphoreSignal(lock);
    }
    else if(M2C_Kind_WriteTimesheet == cmd.kind)
    {
        I64 new_id = DB_WriteTimesheet(db, cmd.id, cmd.date, cmd.employee_id, cmd.job_id, cmd.hours, cmd.miles, cmd.cost, cmd.description, cmd.invoice_id);
        if(0 == cmd.id)
        {
            C2M_WroteNewRecord(c2m, DB_Table_Timesheets, new_id);
        }
        
        PTBD_ScrollToRow(DB_Table_Timesheets, new_id);
        
        DB_Row new_row =
        {
            .id = new_id,
            .date = cmd.date,
            .employee_id = cmd.employee_id,
            .job_id = cmd.job_id,
            .hours = cmd.hours,
            .miles = cmd.miles,
            .cost_raw = cmd.cost,
            .description = cmd.description,
            .invoice_id = cmd.invoice_id,
        };
        {
            DB_Row *job = DB_RowFromId(state->cache[DB_Table_Jobs], new_row.job_id);
            if(0 != job)
            {
                new_row.job_number = job->job_number;
                new_row.job_name = job->job_name;
            }
            DB_Row *employee = DB_RowFromId(state->cache[DB_Table_Employees], new_row.employee_id);
            if(0 != employee)
            {
                new_row.employee_name = employee->employee_name;
            }
        }
        PTBD_PushUndoActionForWriteCmd(DB_Table_Timesheets, state->cache, cmd.id, new_row, undo_state);
    }
    else if(M2C_Kind_WriteJob == cmd.kind)
    {
        I64 new_id = DB_WriteJob(db, cmd.id, cmd.job_number, cmd.job_name);
        if(0 == cmd.id)
        {
            C2M_WroteNewRecord(c2m, DB_Table_Jobs, new_id);
        }
        
        PTBD_ScrollToRow(DB_Table_Jobs, new_id);
        
        DB_Row new_row =
        {
            .id = new_id,
            .job_number = cmd.job_number,
            .job_name = cmd.job_name,
        };
        PTBD_PushUndoActionForWriteCmd(DB_Table_Jobs, state->cache, cmd.id, new_row, undo_state);
    }
    else if(M2C_Kind_WriteInvoice == cmd.kind)
    {
        I64 new_invoice_number = cmd.invoice_number;
        if(DB_InvoiceNumber_Next == cmd.invoice_number)
        {
            new_invoice_number = DB_GetNextInvoiceNumber(db);
        }
        
        I64 new_id = DB_WriteInvoice(db, cmd.id, cmd.address, cmd.title, cmd.description, new_invoice_number, cmd.date, cmd.cost);
        if(0 == cmd.id)
        {
            C2M_WroteNewRecord(c2m, DB_Table_Invoices, new_id);
        }
        
        PTBD_ScrollToRow(DB_Table_Invoices, new_id);
        
        DB_Row new_row =
        {
            .id = new_id,
            .address = cmd.address,
            .title = cmd.title,
            .description = cmd.description,
            .invoice_number = new_invoice_number,
            .date = cmd.date,
            .cost = cmd.cost
        };
        PTBD_PushUndoActionForWriteCmd(DB_Table_Invoices, state->cache, cmd.id, new_row, undo_state);
    }
    else if(M2C_Kind_WriteEmployee == cmd.kind)
    {
        I64 new_id = DB_WriteEmployee(db, cmd.id, cmd.employee_name, cmd.rate);
        if(0 == cmd.id)
        {
            C2M_WroteNewRecord(c2m, DB_Table_Employees, new_id);
        }
        
        PTBD_ScrollToRow(DB_Table_Employees, new_id);
        
        DB_Row new_row =
        {
            .id = new_id,
            .employee_name = cmd.employee_name,
            .rate = cmd.rate,
        };
        PTBD_PushUndoActionForWriteCmd(DB_Table_Employees, state->cache, cmd.id, new_row, undo_state);
    }
    else if(M2C_Kind_NewInvoiceForTimesheets == cmd.kind)
    {
        ArenaTemp scratch = OS_TCtxScratchBegin(0, 0);
        
        S8List job_names = {0};
        for(U64 timesheet_index = 0; timesheet_index < cmd.ids_count; timesheet_index += 1)
        {
            I64 timesheet_id = cmd.ids[timesheet_index];
            DB_Row *timesheet = DB_RowFromId(state->cache[DB_Table_Timesheets], timesheet_id);
            if(!S8ListHas(job_names, timesheet->job_name, 0))
            {
                S8ListAppend(scratch.arena, &job_names, timesheet->job_name);
            }
        }
        
        S8 address = S8("Address line 1\nAddress line 2\nTown\nPostcode");
        S8 title = S8("New invoice");
        S8 description = S8ListJoinFormatted(scratch.arena, job_names, S8ListJoinFormat(.delimiter = S8("\n")));
        
        I64 new_id = DB_WriteInvoice(db, 0, address, title, description, DB_InvoiceNumber_Draft, 0, -1.0f);
        C2M_WroteNewRecord(c2m, DB_Table_Invoices, new_id);
        
        PTBD_ScrollToRow(DB_Table_Invoices, new_id);
        
        if(*undo_state != PTBD_CmdUndoState_Restore)
        {
            *undo_state = PTBD_CmdUndoState_ContinuationFirst;
            
            DB_Row new_row =
            {
                .id = new_id,
                .address = address,
                .title = title,
                .description = description,
                .invoice_number = DB_InvoiceNumber_Draft,
                .date = 0,
                .cost = -1.0f,
            };
            PTBD_PushUndoActionForWriteCmd(DB_Table_Invoices, state->cache, 0, new_row, undo_state);
            
            for(U64 timesheet_index = 0; timesheet_index < cmd.ids_count; timesheet_index += 1)
            {
                I64 timesheet_id = cmd.ids[timesheet_index];
                DB_Row *timesheet = DB_RowFromId(state->cache[DB_Table_Timesheets], timesheet_id);
                if(0 != timesheet)
                {
                    DB_WriteTimesheet(db, timesheet->id, timesheet->date, timesheet->employee_id, timesheet->job_id, timesheet->hours, timesheet->miles, timesheet->cost_raw, timesheet->description, new_id);
                    
                    DB_Row new_row = *timesheet;
                    new_row.invoice_id = new_id;
                    PTBD_PushUndoActionForWriteCmd(DB_Table_Timesheets, state->cache, timesheet->id, new_row, undo_state);
                }
            }
            
            *undo_state = PTBD_CmdUndoState_Normal;
            OS_SemaphoreSignal(ptbd_undo_lock);
        }
        
        ArenaTempEnd(scratch);
    }
    else if(M2C_Kind_NewInvoiceForJob == cmd.kind)
    {
        DB_Row *job = DB_RowFromId(state->cache[DB_Table_Jobs], cmd.job_id);
        
        S8 address = S8("Address line 1\nAddress line 2\nTown\nPostcode");
        S8 title = (0 == job) ? S8("New invoice") : job->job_name;
        S8 description = S8("Description of charges");
        
        I64 new_id = DB_WriteInvoice(db, 0, address, title, description, DB_InvoiceNumber_Draft, 0, -1.0f);
        C2M_WroteNewRecord(c2m, DB_Table_Invoices, new_id);
        
        PTBD_ScrollToRow(DB_Table_Invoices, new_id);
        
        if(*undo_state != PTBD_CmdUndoState_Restore)
        {
            *undo_state = PTBD_CmdUndoState_ContinuationFirst;
            
            DB_Row new_row =
            {
                .id = new_id,
                .address = address,
                .title = title,
                .description = description,
                .invoice_number = DB_InvoiceNumber_Draft,
                .date = 0,
                .cost = -1.0f,
            };
            PTBD_PushUndoActionForWriteCmd(DB_Table_Invoices, state->cache, 0, new_row, undo_state);
            
            for(U64 row_index = 0; row_index < state->cache[DB_Table_Timesheets].rows_count; row_index += 1)
            {
                DB_Row *timesheet = &state->cache[DB_Table_Timesheets].rows[row_index];
                if(timesheet->job_id == cmd.job_id && 0 == timesheet->invoice_id)
                {
                    DB_WriteTimesheet(db, timesheet->id, timesheet->date, timesheet->employee_id, timesheet->job_id, timesheet->hours, timesheet->miles, timesheet->cost_raw, timesheet->description, new_id);
                    
                    DB_Row new_row = *timesheet;
                    new_row.invoice_id = new_id;
                    PTBD_PushUndoActionForWriteCmd(DB_Table_Timesheets, state->cache, timesheet->id, new_row, undo_state);
                }
            }
            
            *undo_state = PTBD_CmdUndoState_Normal;
            OS_SemaphoreSignal(ptbd_undo_lock);
        }
    }
    else if(M2C_Kind_DeleteRecords == cmd.kind)
    {
        DB_Delete(db, cmd.table, cmd.ids_count, cmd.ids);
        
        PTBD_ScrollToRow(cmd.table, (cmd.ids_count > 0) ? cmd.ids[0] : 0);
        
        if(*undo_state != PTBD_CmdUndoState_Restore)
        {
            if(PTBD_CmdUndoState_Normal == *undo_state ||
               PTBD_CmdUndoState_ContinuationFirst == *undo_state)
            {
                OS_SemaphoreWait(ptbd_undo_lock);
            }
            
            PTBD_DeletedRows(cmd.table, state->cache[cmd.table], cmd.ids_count, cmd.ids, (PTBD_CmdUndoState_Continuation == *undo_state));
            
            if(PTBD_CmdUndoState_ContinuationFirst == *undo_state)
            {
                *undo_state = PTBD_CmdUndoState_Continuation;
            }
            
            if(PTBD_CmdUndoState_Normal == *undo_state)
            {
                OS_SemaphoreSignal(ptbd_undo_lock);
            }
        }
    }
    else if(M2C_Kind_ExportInvoice == cmd.kind)
    {
        ArenaTemp scratch = OS_TCtxScratchBegin(0, 0);
        
        S8List extensions = {0};
        S8Node extension_1 = { .next = 0, .string = S8("xlsx"), };
        S8ListAppendExplicit(&extensions, &extension_1);
        S8 filename = OS_FilenameFromSaveDialogue(scratch.arena, S8("Export Invoice"), extensions);
        
        DB_Row *invoice = DB_RowFromId(state->cache[DB_Table_Invoices], cmd.id); 
        
        if(0 != invoice && filename.len > 0)
        {
            PTBD_ExcelFileFromInvoice(invoice, filename);
        }
        
        OS_FileExec(filename, OS_FileExecVerb_Open);
        
        ArenaTempEnd(scratch);
    }
    else if(M2C_Kind_SetSearch == cmd.kind)
    {
        OS_SemaphoreWait(lock);
        {
            OS_InterlockedExchange1I(is_next_state_dirty, True);
            state->search.len = Min1U64(cmd.search.len, sizeof(state->search_buffer));
            state->search.buffer = state->search_buffer;
            MemoryCopy(state->search_buffer, cmd.search.buffer, state->search.len);
            PTBD_Refresh(arenas.view_arena, state);
        }
        OS_SemaphoreSignal(lock);
    }
    else if(M2C_Kind_UndoRestoreBegin == cmd.kind)
    {
        if(PTBD_CmdUndoState_Normal == *undo_state)
        {
            *undo_state = PTBD_CmdUndoState_Restore;
        }
        else
        {
            OS_TCtxErrorPush(S8("Could not begin undo state restore - already in other undo state"));
        }
    }
    else if(M2C_Kind_UndoRestoreEnd == cmd.kind)
    {
        if(PTBD_CmdUndoState_Restore == *undo_state)
        {
            *undo_state = PTBD_CmdUndoState_Normal;
        }
        else
        {
            OS_TCtxErrorPush(S8("Mismatched M2C_UndoRestoreBegin and M2C_UndoRestoreEnd"));
        }
    }
    else if(M2C_Kind_UndoContinuationBegin == cmd.kind)
    {
        if(PTBD_CmdUndoState_Normal == *undo_state)
        {
            *undo_state = PTBD_CmdUndoState_ContinuationFirst;
        }
        else
        {
            OS_TCtxErrorPush(S8("Could not begin undo continuation - already in other undo state"));
        }
    }
    else if(M2C_Kind_UndoContinuationEnd == cmd.kind)
    {
        if(PTBD_CmdUndoState_ContinuationFirst == *undo_state ||
           PTBD_CmdUndoState_Continuation == *undo_state)
        {
            *undo_state = PTBD_CmdUndoState_Normal;
            OS_SemaphoreSignal(ptbd_undo_lock);
        }
        else
        {
            OS_TCtxErrorPush(S8("Mismatched M2C_UndoContinuationBegin and M2C_UndoContinuationEnd"));
        }
    }
    else if(M2C_Kind_DidCopyState == cmd.kind)
    {
        OS_InterlockedExchange1I(is_next_state_dirty, False);
    }
}

Function void
PTBD_CmdThreadProc(void *user_data)
{
    PTBD_Application *app = user_data;
    
    ArenaTemp scratch = OS_TCtxScratchBegin(0, 0);
    
    PTBD_CmdUndoState undo_state = PTBD_CmdUndoState_Normal;
    
    while(app->is_running)
    {
        if(0 == app->db)
        {
            S8 db_filename = PTBD_DBFilename(scratch.arena);
            if(0 == db_filename.len)
            {
                S8List extensions = {0};
                S8ListAppendExplicit(&extensions, &(S8Node){ .string = S8("db"), });
                db_filename = OS_FilenameFromOpenDialogue(scratch.arena, S8("Open database"), extensions);
                
                S8 config_filename = PTBD_DBConfigFilename(scratch.arena);
                OS_FileWriteEntire(config_filename, db_filename);
            }
            app->db = DB_Open(db_filename);
        }
        
        PTBD_Cmd cmd = PTBD_CmdPop(scratch.arena, &app->m2c);
        PTBD_CmdExec(app->next_state_arenas, app->db, app->state_lock, &app->is_next_state_dirty, &app->next_state, cmd, &app->c2m, &undo_state);
        app->should_copy_state = app->is_next_state_dirty;
        G_DoNotWaitForEventsUntil(OS_TimeInMicroseconds() + 1000000ULL);
        ArenaTempEnd(scratch);
    }
}

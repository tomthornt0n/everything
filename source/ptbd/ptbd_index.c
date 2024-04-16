

Function PTBD_IndexCoord
PTBD_IndexNextFromCoord(PTBD_Index *index, PTBD_IndexCoord coord)
{
    PTBD_IndexCoord result = coord;
    
    result.index += 1;
    
    for(;;)
    {
        if(result.index < index->records[result.record].indices_count)
        {
            break;
        }
        else if(result.record + 1 < index->records_count)
        {
            result.record += 1;
            result.index = 0;
        }
        else
        {
            result.record = 0;
            result.index = 0;
        }
    }
    
    return result;
}

Function PTBD_IndexCoord
PTBD_IndexPrevFromCoord(PTBD_Index *index, PTBD_IndexCoord coord)
{
    PTBD_IndexCoord result = coord;
    
    result.index -= 1;
    
    for(;;)
    {
        if(result.index < index->records[result.record].indices_count)
        {
            break;
        }
        else if(result.record - 1 < index->records_count)
        {
            result.record -= 1;
            result.index = index->records[result.record].indices_count - 1;
        }
        else
        {
            result.record = index->records_count - 1;
            result.index = index->records[result.record].indices_count - 1;
        }
    }
    
    return result;
}

Function U64
PTBD_IndexCoordLookup(PTBD_Index *index, PTBD_IndexCoord coord)
{
    U64 result = 0;
    if(coord.record < index->records_count && coord.index < index->records[coord.record].indices_count)
    {
        result = index->records[coord.record].indices[coord.index];
    }
    return result;
}

Function PTBD_Index
PTBD_IndexMakeForTimelineHistogram(Arena *arena, DB_Cache data, S8 search)
{
    PTBD_Index result = {0};
    
    ArenaTemp scratch = OS_TCtxScratchBegin(&arena, 1);
    
    // NOTE(tbt): find range of dates
    Range1U64 date_range = Range1U64(~((U64)0), 0);
    for(U64 timesheet_index = 0; timesheet_index < data.rows_count; timesheet_index += 1)
    {
        DB_Row *timesheet = &data.rows[timesheet_index];
        date_range.min = Min1U64(timesheet->date, date_range.min);
        date_range.max = Max1U64(timesheet->date, date_range.max);
    }
    DateTime today = OS_NowLTC();
    today.msec = 0;
    today.sec = 0;
    today.min = 0;
    today.hour = 0;
    date_range.max = Max1U64(DenseTimeFromDateTime(today), date_range.max);
    
    // NOTE(tbt): allocate a record for each day
    if(date_range.max > date_range.min)
    {
        result.records = PushArray(arena, PTBD_IndexRecord, 1);
        for(DateTime t = DateTimeFromDenseTime(date_range.max);
            DenseTimeFromDateTime(t) >= date_range.min;
            t = YesterdayFromDateTime(t))
        {
            DenseTime key = DenseTimeFromDateTime(t);
            
            PTBD_IndexRecord *record = &result.records[result.records_count];
            
            record->indices_count = 0;
            record->indices = PushArray(scratch.arena, U64, 1);
            for(U64 timesheet_index = 0; timesheet_index < data.rows_count; timesheet_index += 1)
            {
                DB_Row *timesheet = &data.rows[timesheet_index];
                if(timesheet->date == key && PTBD_SearchHasRow(search, timesheet))
                {
                    record->indices[record->indices_count] = timesheet_index;
                    record->indices_count += 1;
                    PushArray(scratch.arena, U64, 1);
                }
            }
            ArenaPop(scratch.arena, sizeof(record->indices[0]));
            
            if(record->indices_count > 0 || 0 == search.len)
            {
                record->key = key;
                result.records_count += 1;
                PushArray(arena, PTBD_IndexRecord, 1);
            }
        }
        ArenaPop(arena, sizeof(result.records[0]));
        
        // NOTE(tbt): copy indices to result arena
        for(U64 record_index = 0; record_index < result.records_count; record_index += 1)
        {
            PTBD_IndexRecord *record = &result.records[record_index];
            if(record->indices_count > 0)
            {
                U64 *new_indices = PushArray(arena, U64, record->indices_count);
                MemoryCopy(new_indices, record->indices, sizeof(record->indices[0])*record->indices_count);
                record->indices = new_indices;
            }
            else
            {
                record->indices = 0;
            }
        }
        
        // NOTE(tbt): move disbursements to the end
        for(U64 record_index = 0; record_index < result.records_count; record_index += 1)
        {
            PTBD_IndexRecord *record = &result.records[record_index];
            U64 count = record->indices_count > 0 ? record->indices_count - 1 : 0;
            for(U64 timesheet_index = 0; timesheet_index < count;)
            {
                U64 row_index = record->indices[timesheet_index];
                if(data.rows[row_index].cost > 0.0f)
                {
                    MemoryMove(&record->indices[timesheet_index], &record->indices[timesheet_index + 1], (count - timesheet_index)*sizeof(record->indices[0]));
                    record->indices[count] = row_index;
                    count -= 1;
                }
                else
                {
                    timesheet_index += 1;
                }
            }
        }
    }
    
    ArenaTempEnd(scratch);
    
    return result;
}

Function PTBD_Index
PTBD_IndexMakeForInvoicingHistogram(Arena *arena, DB_Cache timesheets, DB_Cache jobs, S8 search)
{
    PTBD_Index result = {0};
    
    ArenaTemp scratch = OS_TCtxScratchBegin(&arena, 1);
    
    // NOTE(tbt): allocate a record for each job
    if(jobs.rows_count > 0)
    {
        result.records = PushArray(arena, PTBD_IndexRecord, 1);
        for(U64 job_index = 0; job_index < jobs.rows_count; job_index += 1)
        {
            I64 key = jobs.rows[job_index].id;
            
            PTBD_IndexRecord *record = &result.records[result.records_count];
            
            record->indices_count = 0;
            record->indices = PushArray(scratch.arena, U64, 1);
            for(U64 timesheet_index = 0; timesheet_index < timesheets.rows_count; timesheet_index += 1)
            {
                DB_Row *timesheet = &timesheets.rows[timesheet_index];
                if(timesheet->job_id == key && DB_InvoiceNumber_Draft == timesheet->invoice_number && PTBD_SearchHasRow(search, timesheet))
                {
                    record->indices[record->indices_count] = timesheet_index;
                    record->indices_count += 1;
                    PushArray(scratch.arena, U64, 1);
                }
            }
            ArenaPop(scratch.arena, sizeof(record->indices[0]));
            
            if(record->indices_count > 0)
            {
                record->key = job_index;
                result.records_count += 1;
                PushArray(arena, PTBD_IndexRecord, 1);
            }
        }
        ArenaPop(arena, sizeof(result.records[0]));
        
        // NOTE(tbt): copy indices to result arena
        for(U64 record_index = 0; record_index < result.records_count; record_index += 1)
        {
            PTBD_IndexRecord *record = &result.records[record_index];
            if(record->indices_count > 0)
            {
                U64 *new_indices = PushArray(arena, U64, record->indices_count);
                MemoryCopy(new_indices, record->indices, sizeof(record->indices[0])*record->indices_count);
                record->indices = new_indices;
            }
            else
            {
                record->indices = 0;
            }
        }
        
        // NOTE(tbt): move disbursements to the end
        for(U64 record_index = 0; record_index < result.records_count; record_index += 1)
        {
            PTBD_IndexRecord *record = &result.records[record_index];
            U64 count = record->indices_count > 0 ? record->indices_count - 1 : 0;
            for(U64 timesheet_index = 0; timesheet_index < count;)
            {
                U64 row_index = record->indices[timesheet_index];
                if(timesheets.rows[row_index].cost > 0.0f)
                {
                    MemoryMove(&record->indices[timesheet_index], &record->indices[timesheet_index + 1], (count - timesheet_index)*sizeof(record->indices[0]));
                    record->indices[count] = row_index;
                    count -= 1;
                }
                else
                {
                    timesheet_index += 1;
                }
            }
        }
    }
    
    ArenaTempEnd(scratch);
    
    return result;
}

Function PTBD_Index
PTBD_IndexMakeForBreakdownList(Arena *arena, DB_Cache timesheets, DB_Cache data, U64 link_member_offset, S8 search, B32 reverse)
{
    PTBD_Index result = {0};
    
    ArenaTemp scratch = OS_TCtxScratchBegin(&arena, 1);
    
    // NOTE(tbt): allocate a record for each row
    if(data.rows_count > 0)
    {
        result.records = PushArray(arena, PTBD_IndexRecord, 1);
        for(U64 row_index = 0; row_index < data.rows_count; row_index += 1)
        {
            U64 reverse_row_index = data.rows_count - 1 - row_index;
            U64 final_row_index = reverse ? reverse_row_index : row_index;
            
            DB_Row *row = &data.rows[final_row_index];
            
            B32 is_in_search = PTBD_SearchHasRow(search, row);
            ArenaTemp checkpoint = ArenaTempBegin(scratch.arena);
            
            I64 key = data.rows[final_row_index].id;
            
            PTBD_IndexRecord *record = &result.records[result.records_count];
            
            record->indices_count = 0;
            record->indices = PushArray(scratch.arena, U64, 1);
            for(U64 timesheet_index = 0; timesheet_index < timesheets.rows_count; timesheet_index += 1)
            {
                U64 reverse_timesheet_index = timesheets.rows_count - 1 - timesheet_index;
                U64 final_timesheet_index = reverse ? reverse_timesheet_index : timesheet_index;
                
                DB_Row *timesheet = &timesheets.rows[final_timesheet_index];
                if(*MemberAtOffset(timesheet, link_member_offset, I64) == key)
                {
                    B32 timesheet_is_in_search = PTBD_SearchHasRow(search, timesheet);
                    if(timesheet_is_in_search)
                    {
                        is_in_search = True;
                        record->indices[record->indices_count] = final_timesheet_index;
                        record->indices_count += 1;
                        PushArray(scratch.arena, U64, 1);
                    }
                }
            }
            ArenaPop(scratch.arena, sizeof(record->indices[0]));
            
            if(is_in_search)
            {
                record->key = final_row_index;
                result.records_count += 1;
                PushArray(arena, PTBD_IndexRecord, 1);
            }
            else
            {
                ArenaTempEnd(checkpoint);
            }
        }
        ArenaPop(arena, sizeof(result.records[0]));
        
        // NOTE(tbt): copy indices to result arena
        for(U64 record_index = 0; record_index < result.records_count; record_index += 1)
        {
            PTBD_IndexRecord *record = &result.records[record_index];
            if(record->indices_count > 0)
            {
                U64 *new_indices = PushArray(arena, U64, record->indices_count);
                MemoryCopy(new_indices, record->indices, sizeof(record->indices[0])*record->indices_count);
                record->indices = new_indices;
            }
            else
            {
                record->indices = 0;
            }
        }
    }
    
    ArenaTempEnd(scratch);
    
    return result;
}

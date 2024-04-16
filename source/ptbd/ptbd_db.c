
Function void
DB_ErrorPush(sqlite3 *db, B32 success, S8 message)
{
    if(!success)
    {
        OS_TCtxErrorPushFmt("%.*s: %s", FmtS8(message), sqlite3_errmsg(db));
    }
}

Function void
DB_ErrorPushFmt(sqlite3 *db, B32 success, const char *fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    if(!success)
    {
        ArenaTemp scratch = OS_TCtxScratchBegin(0, 0);
        S8 message = S8FromFmtV(scratch.arena, fmt, args);
        OS_TCtxErrorPushFmt("%.*s: %s", FmtS8(message), sqlite3_errmsg(db));
        ArenaTempEnd(scratch);
    }
    va_end(args);
}

Function S8
DB_S8FromDenseTime(Arena *arena, DenseTime time)
{
    DateTime date = DateTimeFromDenseTime(time);
    S8 result = S8FromFmt(arena, "%04d-%02d-%02d", date.year, date.mon + 1, date.day + 1);
    return result;
}

Function DenseTime
DB_DenseTimeFromCString(const char *string)
{
    DenseTime result = 0;
    
    if(0 != string)
    {
        DateTime date = {0};
        S8Split split = S8SplitMake(CStringAsS8(string), S8("-"), 0);
        S8SplitNext(&split); date.year = S8Parse1U64(split.current);
        S8SplitNext(&split); date.mon = S8Parse1U64(split.current) - 1;
        S8SplitNext(&split); date.day = S8Parse1U64(split.current) - 1;
        
        result = DenseTimeFromDateTime(date);
    }
    
    return result;
}

Function sqlite3 *
DB_Open(S8 filename)
{
    ArenaTemp scratch = OS_TCtxScratchBegin(0, 0);
    
    sqlite3 *result = 0;
    
    S8 db_filename = S8Clone(scratch.arena, filename); // NOTE(tbt): ensure null terminated
    sqlite3_open(db_filename.buffer, &result);
    
    S8 sql = S8("PRAGMA foreign_keys = ON");
    sqlite3_stmt *statement = 0;
    sqlite3_prepare_v2(result, sql.buffer, sql.len, &statement, 0);
    DB_ErrorPush(result, (0 != statement), S8("Error enabling foreign keys. While compiling 'PRAGMA foreign_keys = ON'"));
    I32 rc = sqlite3_step(statement);
    while(SQLITE_ROW == rc)
    {
        rc = sqlite3_step(statement);
    }
    DB_ErrorPush(result, (SQLITE_DONE == rc), S8("Error enabling foreign keys. While executing PRAGMA"));
    
    ArenaTempEnd(scratch);
    
    return result;
}

Function void
DB_GetJobs(Arena *arena, sqlite3 *db, DB_Cache *result)
{
    // NOTE(tbt): initialse result struct
    result->rows_count = 0;
    result->rows = PushArray(arena, DB_Row, 1);
    
    ArenaTemp scratch = OS_TCtxScratchBegin(&arena, 1);
    
    // NOTE(tbt): query
    S8 sql = S8("SELECT"
                "  jobs.id,"
                "  number,"
                "  name,"
                "  sums.total_hours,"
                "  sums.total_miles,"
                "  sums.total_cost "
                "FROM"
                "  jobs"
                "  LEFT JOIN"
                "    (SELECT job, SUM(hours) as total_hours, SUM(miles) as total_miles, SUM(MAX(cost, 0)) as total_cost FROM timesheets LEFT JOIN jobs ON jobs.id = job GROUP BY job"
                "    ) AS sums"
                "  ON sums.job = jobs.id "
                "ORDER BY jobs.id ASC");
    //S8 sql = S8("SELECT jobs.id, number, name, 1, 1, 1 FROM jobs ORDER BY jobs.id ASC");
    
    // NOTE(tbt): compile statement
    Persist sqlite3_stmt *statement = 0;
    if(0 == statement)
    {
        sqlite3_prepare_v2(db, sql.buffer, sql.len, &statement, 0);
        DB_ErrorPush(db, (0 != statement), S8("Error retrieving jobs. While compiling SELECT statement"));
    }
    sqlite3_reset(statement);
    
    // NOTE(tbt): execute
    I32 rc = sqlite3_step(statement);
    while(SQLITE_ROW == rc)
    {
        const char *number = (const char *)sqlite3_column_text(statement, 1);
        const char *name = (const char *)sqlite3_column_text(statement, 2);
        
        result->rows[result->rows_count].id = sqlite3_column_int64(statement, 0);
        result->rows[result->rows_count].job_id = result->rows[result->rows_count].id;
        result->rows[result->rows_count].job_number = S8Clone(scratch.arena, CStringAsS8(number));
        result->rows[result->rows_count].job_name = S8Clone(scratch.arena, CStringAsS8(name));
        result->rows[result->rows_count].hours = sqlite3_column_double(statement, 3);
        result->rows[result->rows_count].miles = sqlite3_column_double(statement, 4);
        result->rows[result->rows_count].cost = sqlite3_column_double(statement, 5);
        
        result->rows_count += 1;
        PushArray(arena, DB_Row, 1);
        rc = sqlite3_step(statement);
    }
    DB_ErrorPush(db, (SQLITE_DONE == rc), S8("Error retrieving jobs. While executing SELECT statement"));
    
    ArenaPop(arena, sizeof(DB_Row));
    
    // NOTE(tbt): copy strings to result arena
    for(U64 row_index = 0; row_index < result->rows_count; row_index += 1)
    {
        result->rows[row_index].job_number = S8Clone(arena, result->rows[row_index].job_number);
        result->rows[row_index].job_name = S8Clone(arena, result->rows[row_index].job_name);
    }
    
    ArenaTempEnd(scratch);
}

Function void
DB_GetTimesheets(Arena *arena, sqlite3 *db, DB_Cache *result)
{
    // NOTE(tbt): initialse result struct
    result->rows_count = 0;
    result->rows = PushArray(arena, DB_Row, 1);
    
    ArenaTemp scratch = OS_TCtxScratchBegin(&arena, 1);
    
    // NOTE(tbt): query
    Persist S8 sql =
        S8Initialiser("SELECT"
                      "  timesheets.id,"
                      "  date(timesheets.date, 'unixepoch'),"
                      "  employee,"
                      "  job,"
                      "  hours,"
                      "  miles,"
                      "  timesheets.cost,"
                      "  timesheets.description,"
                      "  invoices.id,"
                      "  jobs.number,"
                      "  jobs.name,"
                      "  employees.name,"
                      "  invoices.address,"
                      "  invoices.title,"
                      "  invoices.number "
                      "FROM"
                      "  timesheets"
                      "  INNER JOIN employees ON timesheets.employee = employees.id"
                      "  INNER JOIN jobs ON timesheets.job = jobs.id"
                      "  LEFT JOIN invoices ON timesheets.invoice = invoices.id "
                      "ORDER BY"
                      "  timesheets.id ASC");
    
    // NOTE(tbt): compile statement
    Persist sqlite3_stmt *statement = 0;
    if(0 == statement)
    {
        sqlite3_prepare_v2(db, sql.buffer, sql.len, &statement, 0);
        DB_ErrorPush(db, (0 != statement), S8("Error retrieving timesheets. While compiling SELECT statement"));
    }
    sqlite3_reset(statement);
    
    // NOTE(tbt): execute
    I32 rc = sqlite3_step(statement);
    while(SQLITE_ROW == rc)
    {
        const char *date = (const char *)sqlite3_column_text(statement, 1);
        const char *description = (const char *)sqlite3_column_text(statement, 7);
        const char *job_number = (const char *)sqlite3_column_text(statement, 9);
        const char *job_name = (const char *)sqlite3_column_text(statement, 10);
        const char *employee_name = (const char *)sqlite3_column_text(statement, 11);
        const char *address = (const char *)sqlite3_column_text(statement, 12);
        const char *title = (const char *)sqlite3_column_text(statement, 13);
        
        result->rows[result->rows_count].id = sqlite3_column_int64(statement, 0);
        result->rows[result->rows_count].date = DB_DenseTimeFromCString(date);
        result->rows[result->rows_count].employee_id = sqlite3_column_int64(statement, 2);
        result->rows[result->rows_count].job_id = sqlite3_column_int64(statement, 3);
        result->rows[result->rows_count].hours = sqlite3_column_double(statement, 4);
        result->rows[result->rows_count].miles = sqlite3_column_double(statement, 5);
        result->rows[result->rows_count].cost_raw = sqlite3_column_double(statement, 6);
        result->rows[result->rows_count].cost = Max1F(result->rows[result->rows_count].cost_raw, 0.0f);
        result->rows[result->rows_count].cost_calculated = result->rows[result->rows_count].cost;
        result->rows[result->rows_count].description = S8Clone(scratch.arena, CStringAsS8(description));
        result->rows[result->rows_count].invoice_id = sqlite3_column_int64(statement, 8);
        result->rows[result->rows_count].job_number = S8Clone(scratch.arena, CStringAsS8(job_number));
        result->rows[result->rows_count].job_name = S8Clone(scratch.arena, CStringAsS8(job_name));
        result->rows[result->rows_count].employee_name = S8Clone(scratch.arena, CStringAsS8(employee_name));
        result->rows[result->rows_count].address = S8Clone(scratch.arena, CStringAsS8(address));
        result->rows[result->rows_count].title = S8Clone(scratch.arena, CStringAsS8(title));
        result->rows[result->rows_count].invoice_number = sqlite3_column_int64(statement, 14);
        
        // NOTE(tbt): verify
        if(result->rows[result->rows_count].cost > 0.0f)
        {
            if(result->rows[result->rows_count].hours > 0.0f ||
               result->rows[result->rows_count].miles > 0.0f)
            {
                OS_TCtxErrorPushFmt("Timesheet %llu has non-zero cost (disbursement) but still has non-zero hours or miles.", result->rows[result->rows_count].id);
            }
        }
        else
        {
            if(result->rows[result->rows_count].hours <= 0.0f ||
               result->rows[result->rows_count].employee_id == 0)
            {
                OS_TCtxErrorPushFmt("Timesheet %llu has zero cost (timesheet) but also has zero hours or employee id.", result->rows[result->rows_count].id);
            }
        }
        
        result->rows_count += 1;
        PushArray(arena, DB_Row, 1);
        rc = sqlite3_step(statement);
    }
    DB_ErrorPush(db, (SQLITE_DONE == rc), S8("Error retrieving timesheets. While executing SELECT statement"));
    
    ArenaPop(arena, sizeof(DB_Row));
    
    // NOTE(tbt): copy strings to result arena
    for(U64 row_index = 0; row_index < result->rows_count; row_index += 1)
    {
        result->rows[row_index].description = S8Clone(arena, result->rows[row_index].description);
        result->rows[row_index].job_number = S8Clone(arena, result->rows[row_index].job_number);
        result->rows[row_index].job_name = S8Clone(arena, result->rows[row_index].job_name);
        result->rows[row_index].employee_name = S8Clone(arena, result->rows[row_index].employee_name);
        result->rows[row_index].address = S8Clone(arena, result->rows[row_index].address);
        result->rows[row_index].title = S8Clone(arena, result->rows[row_index].title);
    }
    
    ArenaTempEnd(scratch);
}

Function void
DB_GetEmployees(Arena *arena, sqlite3 *db, DB_Cache *result)
{
    // NOTE(tbt): initialse result struct
    result->rows_count = 0;
    result->rows = PushArray(arena, DB_Row, 1);
    
    ArenaTemp scratch = OS_TCtxScratchBegin(&arena, 1);
    
    // NOTE(tbt): query
    S8 sql = S8("SELECT * FROM employees ORDER BY id ASC");
    
    // NOTE(tbt): compile statement
    Persist sqlite3_stmt *statement = 0;
    if(0 == statement)
    {
        sqlite3_prepare_v2(db, sql.buffer, sql.len, &statement, 0);
        DB_ErrorPush(db, (0 != statement), S8("Error retrieving employees. While compiling SELECT statement"));
    }
    sqlite3_reset(statement);
    
    // NOTE(tbt): execute
    I32 rc = sqlite3_step(statement);
    while(SQLITE_ROW == rc)
    {
        const char *name = (const char *)sqlite3_column_text(statement, 1);
        
        result->rows[result->rows_count].id = sqlite3_column_int64(statement, 0);
        result->rows[result->rows_count].employee_id = result->rows[result->rows_count].id;
        result->rows[result->rows_count].employee_name = S8Clone(scratch.arena, CStringAsS8(name));
        result->rows[result->rows_count].rate = sqlite3_column_double(statement, 2);
        
        result->rows_count += 1;
        PushArray(arena, DB_Row, 1);
        rc = sqlite3_step(statement);
    }
    DB_ErrorPush(db, (SQLITE_DONE == rc), S8("Error retrieving employees. While executing SELECT statement"));
    
    ArenaPop(arena, sizeof(DB_Row));
    
    // NOTE(tbt): copy strings to result arena
    for(U64 row_index = 0; row_index < result->rows_count; row_index += 1)
    {
        result->rows[row_index].employee_name = S8Clone(arena, result->rows[row_index].employee_name);
    }
    
    ArenaTempEnd(scratch);
}

Function void
DB_GetInvoices(Arena *arena, sqlite3 *db, DB_Cache *result)
{
    // NOTE(tbt): initialse result struct
    result->rows_count = 0;
    result->rows = PushArray(arena, DB_Row, 1);
    
    ArenaTemp scratch = OS_TCtxScratchBegin(&arena, 1);
    
    // NOTE(tbt): query
    Persist S8 sql =
        S8Initialiser("SELECT"
                      "  id,"
                      "  address,"
                      "  title,"
                      "  description,"
                      "  number,"
                      "  date(date, 'unixepoch'),"
                      "  calculated_costs.calculated_cost,"
                      "  cost "
                      "FROM"
                      "  invoices "
                      "  LEFT JOIN"
                      "  (SELECT timesheets.invoice, SUM(CASE WHEN timesheets.cost > 0.0 THEN timesheets.cost ELSE hours*rate END) as calculated_cost FROM invoices INNER JOIN timesheets ON timesheets.invoice = invoices.id INNER JOIN employees ON employees.id = timesheets.employee GROUP BY invoices.id"
                      "  ) AS calculated_costs "
                      "ON calculated_costs.invoice = invoices.id "
                      "ORDER BY invoices.id ASC");
    
    // NOTE(tbt): compile statement
    Persist sqlite3_stmt *statement = 0;
    if(0 == statement)
    {
        sqlite3_prepare_v2(db, sql.buffer, sql.len, &statement, 0);
        DB_ErrorPush(db, (0 != statement), S8("Error retrieving invoices. While compiling SELECT statement"));
    }
    sqlite3_reset(statement);
    
    // NOTE(tbt): execute
    I32 rc = sqlite3_step(statement);
    while(SQLITE_ROW == rc)
    {
        const char *address = (const char *)sqlite3_column_text(statement, 1);
        const char *title = (const char *)sqlite3_column_text(statement, 2);
        const char *description = (const char *)sqlite3_column_text(statement, 3);
        const char *date = (const char *)sqlite3_column_text(statement, 5);
        
        result->rows[result->rows_count].id = sqlite3_column_int64(statement, 0);
        result->rows[result->rows_count].invoice_id = result->rows[result->rows_count].id;
        result->rows[result->rows_count].address = S8Clone(scratch.arena, CStringAsS8(address));
        result->rows[result->rows_count].title = S8Clone(scratch.arena, CStringAsS8(title));
        result->rows[result->rows_count].description = S8Clone(scratch.arena, CStringAsS8(description));
        result->rows[result->rows_count].invoice_number = sqlite3_column_int64(statement, 4);
        result->rows[result->rows_count].date = DB_DenseTimeFromCString(date);
        result->rows[result->rows_count].cost_raw = sqlite3_column_double(statement, 7);
        result->rows[result->rows_count].cost_calculated = sqlite3_column_double(statement, 6);
        result->rows[result->rows_count].cost = (result->rows[result->rows_count].cost_raw > 0.0f) ? result->rows[result->rows_count].cost_raw : result->rows[result->rows_count].cost_calculated;
        
        result->rows_count += 1;
        PushArray(arena, DB_Row, 1);
        rc = sqlite3_step(statement);
    }
    DB_ErrorPush(db, (SQLITE_DONE == rc), S8("Error retrieving invoices. While executing SELECT statement"));
    
    ArenaPop(arena, sizeof(DB_Row));
    
    // NOTE(tbt): copy strings to result arena
    for(U64 row_index = 0; row_index < result->rows_count; row_index += 1)
    {
        result->rows[row_index].address = S8Clone(arena, result->rows[row_index].address);
        result->rows[row_index].title = S8Clone(arena, result->rows[row_index].title);
        result->rows[row_index].description = S8Clone(arena, result->rows[row_index].description);
    }
    
    ArenaTempEnd(scratch);
}

Function I64
DB_GetWrittenOffInvoiceId(sqlite3 *db)
{
    I64 result = 0;
    
    // NOTE(tbt): query
    S8 sql = S8("SELECT id FROM invoices WHERE number = ?");
    
    // NOTE(tbt): compile statement
    Persist sqlite3_stmt *statement = 0;
    if(0 == statement)
    {
        sqlite3_prepare_v2(db, sql.buffer, sql.len, &statement, 0);
        DB_ErrorPush(db, (0 != statement), S8("Error retrieving invoice id. While compiling SELECT statement"));
    }
    sqlite3_reset(statement);
    sqlite3_bind_int(statement, 1, DB_InvoiceNumber_WrittenOff);
    
    // NOTE(tbt): execute
    I32 rc = sqlite3_step(statement);
    while(SQLITE_ROW == rc)
    {
        result = sqlite3_column_int64(statement, 0);
        rc = sqlite3_step(statement);
    }
    DB_ErrorPush(db, (SQLITE_DONE == rc), S8("Error retrieving invoice id. While executing SELECT statement"));
    
    return result;
}

Function I64
DB_GetNextInvoiceNumber(sqlite3 *db)
{
    I64 result = 0;
    
    // NOTE(tbt): query
    S8 sql = S8("SELECT MAX(number) FROM invoices");
    
    // NOTE(tbt): compile statement
    Persist sqlite3_stmt *statement = 0;
    if(0 == statement)
    {
        sqlite3_prepare_v2(db, sql.buffer, sql.len, &statement, 0);
        DB_ErrorPush(db, (0 != statement), S8("Error retrieving invoice number. While compiling SELECT statement"));
    }
    sqlite3_reset(statement);
    
    // NOTE(tbt): execute
    I32 rc = sqlite3_step(statement);
    while(SQLITE_ROW == rc)
    {
        result = sqlite3_column_int64(statement, 0) + 1;
        rc = sqlite3_step(statement);
    }
    DB_ErrorPush(db, (SQLITE_DONE == rc), S8("Error retrieving invoice number. While executing SELECT statement"));
    
    return result;
}

Function I64
DB_GetDefaultJobId(sqlite3 *db)
{
    I64 result = 0;
    
    // NOTE(tbt): query
    S8 sql = S8("select MAX(id) FROM jobs");
    
    // NOTE(tbt): compile statement
    Persist sqlite3_stmt *statement = 0;
    if(0 == statement)
    {
        sqlite3_prepare_v2(db, sql.buffer, sql.len, &statement, 0);
        DB_ErrorPush(db, (0 != statement), S8("Error looking up the default job. While compiling SELECT statement"));
    }
    sqlite3_reset(statement);
    
    // NOTE(tbt): execute
    I32 rc = sqlite3_step(statement);
    while(SQLITE_ROW == rc)
    {
        result = sqlite3_column_int64(statement, 0);
        rc = sqlite3_step(statement);
    }
    DB_ErrorPush(db, (SQLITE_DONE == rc), S8("Error looking up the default job. While executing SELECT statement"));
    
    return result;
}

Function I64
DB_GetDefaultEmployeeId(sqlite3 *db)
{
    I64 result = 0;
    
    // NOTE(tbt): query
    S8 sql = S8("select MIN(id) FROM employees");
    
    // NOTE(tbt): compile statement
    Persist sqlite3_stmt *statement = 0;
    if(0 == statement)
    {
        sqlite3_prepare_v2(db, sql.buffer, sql.len, &statement, 0);
        DB_ErrorPush(db, (0 != statement), S8("Error looking up the default employee. While compiling SELECT statement"));
    }
    sqlite3_reset(statement);
    
    // NOTE(tbt): execute
    I32 rc = sqlite3_step(statement);
    while(SQLITE_ROW == rc)
    {
        result = sqlite3_column_int64(statement, 0);
        rc = sqlite3_step(statement);
    }
    DB_ErrorPush(db, (SQLITE_DONE == rc), S8("Error looking up the default employee. While executing SELECT statement"));
    
    return result;
}

Function U64
DB_GetUninvoicedTimesheetsForJob(Arena *arena, sqlite3 *db, I64 job_id, I64 **timesheet_ids)
{
    // NOTE(tbt): query
    S8 sql = S8("SELECT id FROM timesheets WHERE job = ? AND invoice IS NULL");
    
    // NOTE(tbt): compile statement
    Persist sqlite3_stmt *statement = 0;
    if(0 == statement)
    {
        sqlite3_prepare_v2(db, sql.buffer, sql.len, &statement, 0);
        DB_ErrorPush(db, (0 != statement), S8("Error getting uninvoiced timesheets for job. While compiling SELECT statement"));
    }
    sqlite3_reset(statement);
    sqlite3_bind_int(statement, 1, job_id);
    
    // NOTE(tbt): execute
    U64 count = 0;
    I64 *result = PushArray(arena, I64, 1);
    I32 rc = sqlite3_step(statement);
    while(SQLITE_ROW == rc)
    {
        I64 timesheet_id = sqlite3_column_int64(statement, 0);
        result[count] = timesheet_id;
        count += 1;
        PushArray(arena, I64, 1);
        rc = sqlite3_step(statement);
    }
    ArenaPop(arena, sizeof(result[0]));
    DB_ErrorPush(db, (SQLITE_DONE == rc), S8("Error getting uninvoiced timesheets for job. While executing SELECT statement"));
    
    // NOTE(tbt): write result out
    *timesheet_ids = result;
    return count;
}

Function I64
DB_WriteJob(sqlite3 *db, I64 id, S8 number, S8 name)
{
    I64 result = id;
    
    Persist sqlite3_stmt *statement = 0;
    if(0 == statement)
    {
        S8 sql = S8("INSERT INTO jobs (id, number, name)"
                    "  VALUES (?, ?, ?)"
                    "  ON CONFLICT (id) DO"
                    "    UPDATE SET number = ?, name = ?"
                    "  RETURNING id");
        sqlite3_prepare_v2(db, sql.buffer, sql.len, &statement, 0);
        DB_ErrorPush(db, (0 != statement), S8("Could not insert job. While jobs insert statement"));
    }
    
    sqlite3_reset(statement);
    if(0 == id) { sqlite3_bind_null(statement, 1); }
    else        { sqlite3_bind_int64(statement, 1, id); }
    sqlite3_bind_text(statement, 2, number.buffer, number.len, SQLITE_STATIC);
    sqlite3_bind_text(statement, 3, name.buffer, name.len, SQLITE_STATIC);
    sqlite3_bind_text(statement, 4, number.buffer, number.len, SQLITE_STATIC);
    sqlite3_bind_text(statement, 5, name.buffer, name.len, SQLITE_STATIC);
    
    I32 rc = sqlite3_step(statement);
    while(SQLITE_ROW == rc)
    {
        result = sqlite3_column_int64(statement, 0);
        rc = sqlite3_step(statement);
    }
    DB_ErrorPush(db, (SQLITE_DONE == rc), S8("Could not insert job. While executing insert statement"));
    
    return result;
}

Function I64
DB_WriteTimesheet(sqlite3 *db, I64 id, DenseTime date, I64 employee_id, I64 job_id, F64 hours, F64 miles, F64 cost, S8 description, I64 invoice_id)
{
    ArenaTemp scratch = OS_TCtxScratchBegin(0, 0);
    
    I64 result = id;
    
    S8 date_string = DB_S8FromDenseTime(scratch.arena, date);
    if(0 == employee_id)
    {
        employee_id = DB_GetDefaultEmployeeId(db);
    }
    if(0 == job_id)
    {
        job_id = DB_GetDefaultJobId(db);
    }
    
    Persist sqlite3_stmt *statement = 0;
    if(0 == statement)
    {
        Persist S8 sql =
            S8Initialiser("INSERT INTO timesheets (id, date, employee, job, hours, miles, cost, description, invoice)"
                          "  VALUES (?, unixepoch(?), ?, ?, ?, ?, ?, ?, ?)"
                          "  ON CONFLICT (id) DO"
                          "    UPDATE SET date = unixepoch(?), employee = ?, job = ?, hours = ?, miles = ?, cost = ?, description = ?, invoice = ?"
                          "  RETURNING id");
        sqlite3_prepare_v2(db, sql.buffer, sql.len, &statement, 0);
        DB_ErrorPush(db, (0 != statement), S8("Could not write timesheet. While compiling insert statement"));
    }
    
    sqlite3_reset(statement);
    if(0 == id) { sqlite3_bind_null(statement, 1); }
    else        { sqlite3_bind_int64(statement, 1, id); }
    sqlite3_bind_text(statement, 2, date_string.buffer, date_string.len, SQLITE_STATIC);
    sqlite3_bind_int64(statement, 3, employee_id);
    sqlite3_bind_int64(statement, 4, job_id);
    sqlite3_bind_double(statement, 5, hours);
    sqlite3_bind_double(statement, 6, miles);
    sqlite3_bind_double(statement, 7, cost);
    sqlite3_bind_text(statement, 8, description.buffer, description.len, SQLITE_STATIC);
    if(0 == invoice_id) { sqlite3_bind_null(statement, 9); }
    else                { sqlite3_bind_int64(statement, 9, invoice_id); }
    sqlite3_bind_text(statement, 10, date_string.buffer, date_string.len, SQLITE_STATIC);
    sqlite3_bind_int64(statement, 11, employee_id);
    sqlite3_bind_int64(statement, 12, job_id);
    sqlite3_bind_double(statement, 13, hours);
    sqlite3_bind_double(statement, 14, miles);
    sqlite3_bind_double(statement, 15, cost);
    sqlite3_bind_text(statement, 16, description.buffer, description.len, SQLITE_STATIC);
    if(0 == invoice_id) { sqlite3_bind_null(statement, 17); }
    else                { sqlite3_bind_int64(statement, 17, invoice_id); }
    
    I32 rc = sqlite3_step(statement);
    while(SQLITE_ROW == rc)
    {
        result = sqlite3_column_int64(statement, 0);
        rc = sqlite3_step(statement);
    }
    DB_ErrorPush(db, (SQLITE_DONE == rc), S8("Could not write timesheet. While executing insert statement"));
    
    ArenaTempEnd(scratch);
    
    return result;
}

Function I64
DB_WriteEmployee(sqlite3 *db, I64 id, S8 name, F64 rate)
{
    I64 result = id;
    
    Persist sqlite3_stmt *statement = 0;
    if(0 == statement)
    {
        Persist S8 sql =
            S8Initialiser("INSERT INTO employees (id, name, rate)"
                          "  VALUES (?, ?, ?)"
                          "  ON CONFLICT (id) DO"
                          "    UPDATE SET name = ?, rate = ?"
                          "  RETURNING id");
        sqlite3_prepare_v2(db, sql.buffer, sql.len, &statement, 0);
        DB_ErrorPush(db, (0 != statement), S8("Could not write employee. While compiling insert statement"));
    }
    
    sqlite3_reset(statement);
    if(0 == id) { sqlite3_bind_null(statement, 1); }
    else        { sqlite3_bind_int64(statement, 1, id); }
    sqlite3_bind_text(statement, 2, name.buffer, name.len, SQLITE_STATIC);
    sqlite3_bind_double(statement, 3, rate);
    sqlite3_bind_text(statement, 4, name.buffer, name.len, SQLITE_STATIC);
    sqlite3_bind_double(statement, 5, rate);
    
    I32 rc = sqlite3_step(statement);
    while(SQLITE_ROW == rc)
    {
        result = sqlite3_column_int64(statement, 0);
        rc = sqlite3_step(statement);
    }
    DB_ErrorPush(db, (SQLITE_DONE == rc), S8("Could not write employee. While executing insert statement"));
    
    return result;
}

Function I64
DB_WriteInvoice(sqlite3 *db, I64 id, S8 address, S8 title, S8 description, I64 number, DenseTime date, F64 cost)
{
    ArenaTemp scratch = OS_TCtxScratchBegin(0, 0);
    
    I64 result = id;
    
    S8 date_string = DB_S8FromDenseTime(scratch.arena, date);
    if(DB_InvoiceNumber_Next == number)
    {
        number = DB_GetNextInvoiceNumber(db);
    }
    
    Persist sqlite3_stmt *statement = 0;
    if(0 == statement)
    {
        Persist S8 sql =
            S8Initialiser("INSERT INTO invoices (id, address, title, description, number, date, cost)"
                          "  VALUES (?, ?, ?, ?, ?, unixepoch(?), ?)"
                          "  ON CONFLICT (id) DO"
                          "    UPDATE SET address = ?, title = ?, description = ?, number = ?, date = unixepoch(?), cost = ?"
                          "  RETURNING id");
        sqlite3_prepare_v2(db, sql.buffer, sql.len, &statement, 0);
        DB_ErrorPush(db, (0 != statement), S8("Could not write invoice. While compiling insert statement"));
    }
    
    sqlite3_reset(statement);
    if(0 == id) { sqlite3_bind_null(statement, 1); }
    else        { sqlite3_bind_int64(statement, 1, id); }
    sqlite3_bind_text(statement, 2, address.buffer, address.len, SQLITE_STATIC);
    sqlite3_bind_text(statement, 3, title.buffer, title.len, SQLITE_STATIC);
    sqlite3_bind_text(statement, 4, description.buffer, description.len, SQLITE_STATIC);
    sqlite3_bind_int64(statement, 5, number);
    if(0 == date) { sqlite3_bind_null(statement, 6); }
    else              { sqlite3_bind_text(statement, 6, date_string.buffer, date_string.len, SQLITE_STATIC); }
    sqlite3_bind_double(statement, 7, cost);
    sqlite3_bind_text(statement, 8, address.buffer, address.len, SQLITE_STATIC);
    sqlite3_bind_text(statement, 9, title.buffer, title.len, SQLITE_STATIC);
    sqlite3_bind_text(statement, 10, description.buffer, description.len, SQLITE_STATIC);
    sqlite3_bind_int64(statement, 11, number);
    if(0 == date) { sqlite3_bind_null(statement, 12); }
    else          { sqlite3_bind_text(statement, 12, date_string.buffer, date_string.len, SQLITE_STATIC); }
    sqlite3_bind_double(statement, 13, cost);
    
    I32 rc = sqlite3_step(statement);
    while(SQLITE_ROW == rc)
    {
        result = sqlite3_column_int64(statement, 0);
        rc = sqlite3_step(statement);
    }
    DB_ErrorPush(db, (SQLITE_DONE == rc), S8("Could not write invoice. While executing insert statement"));
    
    ArenaTempEnd(scratch);
    
    return result;
}

Function void
DB_Delete(sqlite3 *db, DB_Table table, U64 ids_count, I64 *ids)
{
    ArenaTemp scratch = OS_TCtxScratchBegin(0, 0);
    
    // NOTE(tbt): generate query
    S8 table_string = DB_S8FromTable(table);
    S8List ids_list = {0};
    for(U64 id_index = 0; id_index < ids_count; id_index += 1)
    {
        S8ListAppendFmt(scratch.arena, &ids_list, "%lld", ids[id_index]);
    }
    S8ListJoinFormat format =
    {
        .before = S8("("),
        .delimiter = S8(", "),
        .after = S8(")"),
    };
    S8 ids_string = S8ListJoinFormatted(scratch.arena, ids_list, format);
    S8 sql = S8FromFmt(scratch.arena, "DELETE FROM %.*s WHERE id IN %.*s", FmtS8(table_string), FmtS8(ids_string));
    
    // NOTE(tbt): compile statement
    sqlite3_stmt *statement = 0;
    sqlite3_prepare_v2(db, sql.buffer, sql.len, &statement, 0);
    DB_ErrorPushFmt(db, (0 != statement), "Could not delete %.*s. While compiling %.*s", FmtS8(table_string),  FmtS8(sql));
    
    // NOTE(tbt): execute
    I32 rc = sqlite3_step(statement);
    while(SQLITE_ROW == rc)
    {
        rc = sqlite3_step(statement);
    }
    DB_ErrorPushFmt(db, (SQLITE_DONE == rc), "Could not delete %.*s. While compiling %.*s", FmtS8(table_string),  FmtS8(sql));
    
    sqlite3_finalize(statement);
    ArenaTempEnd(scratch);
}

Function DB_Row *
DB_RowFromId(DB_Cache data, I64 id)
{
    DB_Row *result = 0;
    
    if(data.rows_count > 0 && id > 0)
    {
        U64 l = 0;
        U64 r = data.rows_count - 1;
        
        while(l <= r && 0 == result)
        {
            U64 mid = (r + l) / 2;
            if(id < data.rows[mid].id)
            {
                r = mid - 1;
            }
            else if(id > data.rows[mid].id)
            {
                l = mid + 1;
            }
            else
            {
                result = &data.rows[mid];
            }
        }
    }
    
    return result;
}

Function S8
DB_S8FromTable(DB_Table table)
{
    Assert(table < DB_Table_MAX);
    ReadOnlyVar Persist S8 lut[DB_Table_MAX] =
    {
        S8Initialiser("jobs"),
        S8Initialiser("timesheets"),
        S8Initialiser("employees"),
        S8Initialiser("invoices"),
    };
    S8 result = lut[table];
    return result;
}


Function B32
DB_TimesheetIsDisbursement(DB_Row *row)
{
    B32 result = (row->cost_raw > 0.0f);
    return result;
}


Function void
DB_ThreadProc(void *user_data)
{
    OS_ThreadNameSet(S8(ApplicationNameString ": db"));
    
    ArenaTemp scratch = OS_TCtxScratchBegin(0, 0);
    
    for(;;)
    {
        U64 msg_len = 0;
        void *msg = MsgRingDequeue(scratch.arena, &db.u2w, &msg_len);
        
        if(0 == msg_len)
        {
            break;
        }
        
        U8 *ptr = msg;
        
        DB_U2WMsgKind msg_kind;
        CopyAndIncrement(&msg_kind, ptr, sizeof(msg_kind), ptr);
        
        S8 filename = {0};
        {
            CopyAndIncrement(&filename.len, ptr, sizeof(filename.len), ptr);
            filename.buffer = ptr;
            ptr += filename.len;
        }
        
        S8 view_name = {0};
        {
            CopyAndIncrement(&view_name.len, ptr, sizeof(view_name.len), ptr);
            view_name.buffer = ptr;
            ptr += view_name.len;
        }
        
        if(DB_U2WMsgKind_Read == msg_kind)
        {
            // NOTE(tbt): compile statement
            S8 sql = S8("SELECT data FROM near WHERE filename = ? AND view_name = ?");
            ThreadLocalVar Persist sqlite3_stmt *statement = 0;
            if(0 == statement)
            {
                sqlite3_prepare_v2(db.sqlite, sql.buffer, sql.len, &statement, 0);
                // TODO(tbt): report error
            }
            sqlite3_reset(statement);
            sqlite3_bind_text(statement, 1, filename.buffer, filename.len, SQLITE_STATIC);
            sqlite3_bind_text(statement, 2, view_name.buffer, view_name.len, SQLITE_STATIC);
            
            // NOTE(tbt): execute
            I32 rc = sqlite3_step(statement);
            if(SQLITE_DONE == rc)
            {
            }
            else if(SQLITE_ROW == rc)
            {
                const void *buffer = sqlite3_column_blob(statement, 0);
                U64 len = sqlite3_column_bytes(statement, 0);
                DB_CacheNodeWrite(filename, view_name, buffer, len);
                
                rc = sqlite3_step(statement);
                if(SQLITE_DONE != rc)
                {
                    // TODO(tbt): report error
                }
            }
            else
            {
                // TODO(tbt): report error
            }
        }
        else if(DB_U2WMsgKind_Write == msg_kind)
        {
            U64 len;
            void *buffer;
            {
                CopyAndIncrement(&len, ptr, sizeof(len), ptr);
                buffer = ptr;
                ptr += len;
            }
            
            //-NOTE(tbt): compile statements
            S8 update_sql = S8("UPDATE near SET data = ? WHERE filename = ? AND view_name = ? RETURNING id");
            ThreadLocalVar Persist sqlite3_stmt *update = 0;
            if(0 == update)
            {
                sqlite3_prepare_v2(db.sqlite, update_sql.buffer, update_sql.len, &update, 0);
                // TODO(tbt): report error
            }
            sqlite3_reset(update);
            sqlite3_bind_blob64(update, 1, buffer, len, SQLITE_STATIC);
            sqlite3_bind_text(update, 2, filename.buffer, filename.len, SQLITE_STATIC);
            sqlite3_bind_text(update, 3, view_name.buffer, view_name.len, SQLITE_STATIC);
            
            S8 insert_sql = S8("INSERT INTO near (filename, view_name, data) VALUES (?, ?, ?) RETURNING id");
            ThreadLocalVar Persist sqlite3_stmt *insert = 0;
            if(0 == insert)
            {
                sqlite3_prepare_v2(db.sqlite, insert_sql.buffer, insert_sql.len, &insert, 0);
                // TODO(tbt): report error
            }
            sqlite3_reset(insert);
            sqlite3_bind_text(insert, 1, filename.buffer, filename.len, SQLITE_STATIC);
            sqlite3_bind_text(insert, 2, view_name.buffer, view_name.len, SQLITE_STATIC);
            sqlite3_bind_blob64(insert, 3, buffer, len, SQLITE_STATIC);
            
            I64 id = 0;
            
            //-NOTE(tbt): try update
            I32 rc = sqlite3_step(update);
            if(SQLITE_DONE == rc)
            {
            }
            else if(SQLITE_ROW == rc)
            {
                id = sqlite3_column_int64(update, 0);
                
                rc = sqlite3_step(update);
                if(SQLITE_DONE != rc)
                {
                    // TODO(tbt): report error
                }
            }
            else
            {
                // TODO(tbt): report error
                const char *msg = sqlite3_errmsg(db.sqlite);
            }
            
            //-NOTE(tbt): insert if not
            if(0 == id)
            {
                I32 rc = sqlite3_step(insert);
                if(SQLITE_DONE == rc)
                {
                }
                else if(SQLITE_ROW == rc)
                {
                    id = sqlite3_column_int64(insert, 0);
                    
                    rc = sqlite3_step(insert);
                    if(SQLITE_DONE != rc)
                    {
                        // TODO(tbt): report error
                    }
                }
                else
                {
                    // TODO(tbt): report error
                    const char *msg = sqlite3_errmsg(db.sqlite);
                }
            }
        }
        
        ArenaTempEnd(scratch);
    }
}

Function OpaqueHandle
DB_HashFromFilenameAndViewName(S8 filename, S8 view_name)
{
    OpaqueHandle result = {0};
    ArenaTemp scratch = OS_TCtxScratchBegin(0, 0);
    S8List list = {0};
    S8ListAppend(scratch.arena, &list, filename);
    S8ListAppend(scratch.arena, &list, view_name);
    S8 joined = S8ListJoin(scratch.arena, list);
    MurmurHash3_x64_128(joined.buffer, joined.len, 0, result.a);
    ArenaTempEnd(scratch);
    return result;
}

Function B32
DB_CacheNodeIsNil(DB_CacheNode *node)
{
    B32 result = (0 == node || &db_cache_node_nil == node);
    return result;
}

Function void
DB_CacheNodeWrite(S8 filename, S8 view_name, const void *buffer, U64 len)
{
    OpaqueHandle hash = DB_HashFromFilenameAndViewName(filename, view_name);
    U64 index = hash.a[0] % DB_CacheBucketsCount;
    U64 stripe_index = index % DB_CacheStripesCount;
    OpaqueHandle lock = db.stripes[stripe_index];
    
    OS_SemaphoreWait(lock);
    
    DB_CacheNode *node = &db_cache_node_nil;
    for(DB_CacheNode *n = db.buckets[index]; !DB_CacheNodeIsNil(n); n = n->hash_next)
    {
        if(OpaqueHandleMatch(hash, n->hash) &&
           S8Match(n->filename, filename, 0) &&
           S8Match(n->view_name, view_name, 0))
        {
            node = n;
            break;
        }
    }
    
    if(!DB_CacheNodeIsNil(node))
    {
        ArenaPopTo(node->arena, node->arena_watermark);
        node->buffer = ArenaPush(node->arena, len, 2*sizeof(void *));
        node->len = len;
        MemoryCopy(node->buffer, buffer, len);
    }
    
    OS_SemaphoreSignal(lock);
}

Function void *
DB_Read(Arena *arena, S8 filename, S8 view_name, U64 min_size, U64 alignment)
{
    ArenaTemp scratch = OS_TCtxScratchBegin(&arena, 1);
    
    //-NOTE(tbt): check in cache first
    OpaqueHandle hash = DB_HashFromFilenameAndViewName(filename, view_name);
    U64 index = hash.a[0] % DB_CacheBucketsCount;
    U64 stripe_index = index % DB_CacheStripesCount;
    OpaqueHandle lock = db.stripes[stripe_index];
    
    OS_SemaphoreWait(lock);
    
    DB_CacheNode *node = &db_cache_node_nil;
    for(DB_CacheNode *n = db.buckets[index]; !DB_CacheNodeIsNil(n); n = n->hash_next)
    {
        if(OpaqueHandleMatch(hash, n->hash) &&
           S8Match(n->filename, filename, 0) &&
           S8Match(n->view_name, view_name, 0))
        {
            node = n;
            break;
        }
    }
    
    //-NOTE(tbt): add an empty node to the cache if not found
    if(DB_CacheNodeIsNil(node))
    {
        Arena *node_arena = ArenaAlloc(Megabytes(4));
        DB_CacheNode *node = PushArray(node_arena, DB_CacheNode, 1);
        node->arena = node_arena;
        node->hash = hash;
        node->filename = S8Clone(node->arena, filename);
        node->view_name = S8Clone(node->arena, view_name);
        node->arena_watermark = node->arena->alloc_position;
        node->buffer = 0;
        node->len = 0;
        node->hash_next = db.buckets[index];
        db.buckets[index] = node;
    }
    
    OS_SemaphoreSignal(lock);
    
    //-NOTE(tbt): dispatch to worker thread if not in cache
    if(DB_CacheNodeIsNil(node))
    {
        U64 msg_len = sizeof(DB_U2WMsgKind) + 2*sizeof(U64) + filename.len + view_name.len;
        U8 *msg = PushArray(scratch.arena, U8, msg_len);
        {
            U8 *ptr = msg;
            CopyAndIncrement(ptr, (DB_U2WMsgKind[]){ DB_U2WMsgKind_Read }, sizeof(DB_U2WMsgKind), ptr);
            CopyAndIncrement(ptr, &filename.len, sizeof(filename.len), ptr);
            CopyAndIncrement(ptr, filename.buffer, filename.len, ptr);
            CopyAndIncrement(ptr, &view_name.len, sizeof(view_name.len), ptr);
            CopyAndIncrement(ptr, view_name.buffer, view_name.len, ptr);
            Assert(ptr == msg + msg_len);
        }
        MsgRingEnqueue(&db.u2w, msg, msg_len);
    }
    
    //-NOTE(tbt): construct result
    void *result = ArenaPush(arena, Max1U64(min_size, node->len), alignment);
    MemoryCopy(result, node->buffer, node->len);
    
    ArenaTempEnd(scratch);
    
    return result;
}

Function void 
DB_Write(S8 filename, S8 view_name, void *buffer, U64 len)
{
    ArenaTemp scratch = OS_TCtxScratchBegin(0, 0);
    
    I64 id = 0;
    
    //-NOTE(tbt): write to cache to hide latency
    DB_CacheNodeWrite(filename, view_name, buffer, len);
    
    //-NOTE(tbt): dispatch to worker thread to actually write to DB
    U64 msg_len = sizeof(DB_U2WMsgKind) + 3*sizeof(U64) + filename.len + view_name.len + len;
    U8 *msg = PushArray(scratch.arena, U8, msg_len);
    {
        U8 *ptr = msg;
        CopyAndIncrement(ptr, (DB_U2WMsgKind[]){ DB_U2WMsgKind_Write }, sizeof(DB_U2WMsgKind), ptr);
        CopyAndIncrement(ptr, &filename.len, sizeof(filename.len), ptr);
        CopyAndIncrement(ptr, filename.buffer, filename.len, ptr);
        CopyAndIncrement(ptr, &view_name.len, sizeof(view_name.len), ptr);
        CopyAndIncrement(ptr, view_name.buffer, view_name.len, ptr);
        CopyAndIncrement(ptr, &len, sizeof(len), ptr);
        CopyAndIncrement(ptr, buffer, len, ptr);
        Assert(ptr == msg + msg_len);
    }
    MsgRingEnqueue(&db.u2w, msg, msg_len);
    
    ArenaTempEnd(scratch);
}

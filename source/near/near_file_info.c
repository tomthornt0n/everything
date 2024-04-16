
Function B32
FI_NodeIsNil (FI_Node *node)
{
    B32 result = (0 == node || &fi_nil_node == node);
    return result;
}

Function void
FI_ThreadProc(void *user_data)
{
    OS_ThreadNameSet(S8(ApplicationNameString ": fi"));
    
    ArenaTemp scratch = OS_TCtxScratchBegin(0, 0);
    
    for(;;)
    {
        S8 filename = {0};
        filename.buffer = MsgRingDequeue(scratch.arena, &fi.u2w, &filename.len);
        
        if(0 == filename.len)
        {
            break;
        }
        
        FI_Node *volatile *bucket = FI_BucketFromFilename(filename);
        if(FI_NodeIsNil(FI_NodeFromBucketAndFilename(filename, *bucket)))
        {
            Arena *arena = ArenaAlloc(Megabytes(10));
            FI_Node *node = PushArray(arena, FI_Node, 1);
            node->arena = arena;
            node->filename = S8Clone(arena, filename);
            node->props = OS_FilePropertiesFromFilename(filename);
            if(node->props.flags & OS_FilePropertiesFlags_IsDirectory)
            {
                OS_FileIterator *fi = OS_FileIteratorBegin(scratch.arena, filename);
                while(OS_FileIteratorNext(scratch.arena, fi))
                {
                    S8ListAppend(arena, &node->children_filenames, fi->current_full_path);
                }
                OS_FileIteratorEnd(fi);
            }
            node->texture = R2D_TextureNil();
            
            OS_TCtxErrorScopePush();
            IMG_Data decoded_image = IMG_DataFromFilename(scratch.arena, filename);
            if(decoded_image.dimensions.x > 0 &&
               decoded_image.dimensions.y > 0)
            {
                node->texture = R2D_TextureAlloc(decoded_image.pixels, decoded_image.dimensions);
            }
            OS_TCtxErrorScopePop(0);
            
            node->hash_next = OS_InterlockedExchange1Ptr(bucket, node);
        }
        
        ArenaTempEnd(scratch);
    }
}

Function FI_Node *volatile *
FI_BucketFromFilename(S8 filename)
{
    U64 hash[2];
    MurmurHash3_x64_128(filename.buffer, filename.len, 0, hash);
    U64 bucket_index = hash[0] % ArrayCount(fi.buckets);
    FI_Node *volatile *result = &fi.buckets[bucket_index];
    return result;
}

Function FI_Node *
FI_NodeFromBucketAndFilename(S8 filename, FI_Node *bucket_first)
{
    FI_Node *result = &fi_nil_node;
    for(FI_Node *node = bucket_first; !FI_NodeIsNil(node); node = node->hash_next)
    {
        if(S8Match(filename, node->filename, 0))
        {
            result = node;
            break;
        }
    }
    return result;
}

Function FI_Node *
FI_InfoFromFilename(S8 filename)
{
    FI_Node *volatile *bucket = FI_BucketFromFilename(filename);
    FI_Node *result = FI_NodeFromBucketAndFilename(filename, *bucket);
    if(FI_NodeIsNil(result))
    {
        MsgRingEnqueue(&fi.u2w, filename.buffer, filename.len);
    }
    return result;
}

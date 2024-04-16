
Function MsgRing
MsgRingAlloc(Arena *arena, U64 cap)
{
    MsgRing result = {0};
    result.cap = cap;
    result.buffer = PushArray(arena, U8, cap);
    result.sem = OS_SemaphoreAlloc(0);
    result.lock = OS_SemaphoreAlloc(1);
    return result;
}

Function void
MsgRingRelease(MsgRing *ring)
{
    OS_SemaphoreRelease(ring->sem);
    OS_SemaphoreRelease(ring->lock);
}

Function B32
MsgRingEnqueue(MsgRing *ring, const void *buffer, U64 len)
{
    B32 result = False;
    
    OS_SemaphoreWait(ring->lock);
    if(ring->write_pos + len + sizeof(U64) < ring->cap + ring->read_pos)
    {
        result = True;
        ring->write_pos += RingWrite(ring->buffer, ring->cap, ring->write_pos, &len, sizeof(len));
        ring->write_pos += RingWrite(ring->buffer, ring->cap, ring->write_pos, buffer, len);
    }
    OS_SemaphoreSignal(ring->lock);
    
    if(result)
    {
        OS_SemaphoreSignal(ring->sem);
    }
    
    return result;
}

Function void *
MsgRingDequeue(Arena *arena, MsgRing *ring, U64 *len_out)
{
    void *result = 0;
    
    OS_SemaphoreWait(ring->sem);
    
    OS_SemaphoreWait(ring->lock);
    
    U64 len = 0;
    if(ring->read_pos + sizeof(len) <= ring->write_pos)
    {
        ring->read_pos += RingRead(ring->buffer, ring->cap, ring->read_pos, &len, sizeof(len));
    }
    
    if(len > 0 && ring->read_pos + len <= ring->write_pos)
    {
        result = PushArray(arena, U8, len);
        ring->read_pos += RingRead(ring->buffer, ring->cap, ring->read_pos, result, len);
    }
    
    OS_SemaphoreSignal(ring->lock);
    
    *len_out = len;
    return result;
}

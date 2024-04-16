
typedef struct MsgRing MsgRing;
struct MsgRing
{
    OpaqueHandle sem;
    OpaqueHandle lock;
    U8 *buffer;
    U64 cap;
    U64 read_pos;
    U64 write_pos;
};

Function MsgRing MsgRingAlloc (Arena *arena, U64 cap);
Function void MsgRingRelease (MsgRing *ring);

Function B32 MsgRingEnqueue (MsgRing *ring, const void *buffer, U64 len);
Function void *MsgRingDequeue (Arena *arena, MsgRing *ring, U64 *len_out);

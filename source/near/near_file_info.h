
typedef struct FI_Node FI_Node;
struct FI_Node
{
    Arena *arena;
    FI_Node *hash_next;
    S8 filename;
    OS_FileProperties props;
    S8List children_filenames;
    OpaqueHandle texture;
};

ReadOnlyVar Global FI_Node fi_nil_node =
{
    .hash_next = &fi_nil_node,
};

typedef struct FI_Shared FI_Shared;
struct FI_Shared
{
    FI_Node *volatile buckets[4096];
    OpaqueHandle thread;
    MsgRing u2w;
};

Global FI_Shared fi = {0};

// NOTE(tbt): helpers
Function B32 FI_NodeIsNil (FI_Node *node);

// NOTE(tbt): low level hash table lookups
Function FI_Node *volatile *FI_BucketFromFilename (S8 filename);
Function FI_Node *FI_NodeFromBucketAndFilename (S8 filename, FI_Node *bucket_first);

// NOTE(tbt): main API
Function FI_Node *FI_InfoFromFilename (S8 filename);

// NOTE(tbt): worker thread
Function void FI_ThreadProc (void *user_data);

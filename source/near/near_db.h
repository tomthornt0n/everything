
#include "external/sqlite3.h"

typedef struct DB_CacheNode DB_CacheNode;
struct DB_CacheNode
{
    DB_CacheNode *hash_next;
    Arena *arena;
    OpaqueHandle hash;
    S8 filename;
    S8 view_name;
    U64 arena_watermark;
    void *buffer;
    U64 len;
};

ReadOnlyVar Global DB_CacheNode db_cache_node_nil =
{
    .hash_next = &db_cache_node_nil,
};

enum
{
    DB_CacheBucketsCount = 4096,
    DB_CacheStripesCount = 64,
};

typedef U8 DB_U2WMsgKind;
enum DB_U2WMsgKind
{
    DB_U2WMsgKind_Read,
    DB_U2WMsgKind_Write,
};

typedef struct DB_Shared DB_Shared;
struct DB_Shared
{
    sqlite3 *sqlite;
    DB_CacheNode *buckets[DB_CacheBucketsCount];
    OpaqueHandle stripes[DB_CacheStripesCount];
    OpaqueHandle thread;
    MsgRing u2w;
};

Global DB_Shared db;

Function OpaqueHandle DB_HashFromFilenameAndViewName (S8 filename, S8 view_name);

Function B32 DB_CacheNodeIsNil (DB_CacheNode *node);
Function void DB_CacheNodeWrite (S8 filename, S8 view_name, const void *buffer, U64 len);

Function void *DB_Read (Arena *arena, S8 filename, S8 view_name, U64 min_size, U64 alignment);
Function void  DB_Write (S8 filename, S8 view_name, void *buffer, U64 len);

#define DB_ReadStruct(A, F, V, T) DB_Read((A), (F), (V), sizeof(T), _Alignof(T))
#define DB_WriteStruct(F, V, D) DB_Write((F), (V), &(D), sizeof(D))

Function void DB_ThreadProc (void *user_data);

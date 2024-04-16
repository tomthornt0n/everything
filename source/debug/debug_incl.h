
typedef enum
{
    DBG_SymKind_NONE,
    DBG_SymKind_Function,
    DBG_SymKind_Global,
    DBG_SymKind_MAX,
} DBG_SymKind;

typedef struct DBG_Sym DBG_Sym;
struct DBG_Sym
{
    DBG_Sym *next;
    DBG_SymKind kind;
    S8 name;
    U64 virtual_address;
    U64 len;
};

typedef struct DBG_SymTable DBG_SymTable;
struct DBG_SymTable
{
    DBG_Sym **buckets;
    U64 cap;
};

Function OpaqueHandle DBG_InfoAlloc                (S8 filename);
Function void         DBG_InfoRelease              (OpaqueHandle info);
Function S8List       DBG_InfoS8ListFromCompilands (Arena *arena, OpaqueHandle info);

Function DBG_SymTable DBG_SymTableMake                       (Arena *arena, U64 cap);
Function void         DBG_SymTableAddAllSymbolsFromCompiland (Arena *arena, DBG_SymTable *table, OpaqueHandle info, S8 compiland_name);

Function DBG_Sym *DBG_SymFromAddress (DBG_SymTable table, void *address);

typedef struct DBG_StackTrace DBG_StackTrace;
struct DBG_StackTrace
{
    void **stack;
    U64 count;
};

Function DBG_StackTrace DBG_StackTraceFromWalkingStack     (Arena *arena);
Function S8List         DBG_S8ListFromStackTraceAndSymbols (Arena *arena, DBG_StackTrace stack_trace, DBG_SymTable *table);


#if BuildOSWindows
# include "debug_w32.c"
#else
# include "no debug backend for current platform"
#endif

Function DBG_Sym *
DBG_SymFromAddress(DBG_SymTable table, void *address)
{
    U64 va = IntFromPtr(address);
    
    U64 hash[2]  = {0};
    MurmurHash3_x64_128(&va, sizeof(va), 0, hash);
    U64 bucket_index = (hash[0] % table.cap);
    
    DBG_Sym *sym = table.buckets[bucket_index];
    while(0 != sym && sym->virtual_address != va)
    {
        sym = sym->next;
    }
    
    return sym;
}

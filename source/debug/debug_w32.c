
#define COBJMACROS
#define WIN32_LEAN_AND_MEAN
#include <dia2.h>
#include <initguid.h>

#pragma comment (lib, "OleAut32.lib")

DEFINE_GUID(CLSID_DiaSource,    0xe6756135, 0x1e65, 0x4d17, 0x85, 0x76, 0x61, 0x07, 0x61, 0x39, 0x8c, 0x3c);
DEFINE_GUID(IID_IDiaDataSource, 0x79F1BB5F, 0xB66E, 0x48e5, 0xB6, 0xA9, 0x15, 0x45, 0xC3, 0x23, 0xCA, 0x3D);

typedef BOOL WINAPI W32_DllGetClassObjectProc (REFCLSID, REFIID, LPVOID *);

typedef struct W32_DebugInfo W32_DebugInfo;
struct W32_DebugInfo
{
    W32_DebugInfo *next_free;
    U64 generation;
    
    IDiaDataSource *source;
    IDiaSession *session;
    IDiaSymbol *exe;
};

ThreadLocalVar Global HMODULE w32_dia_module = 0;
ThreadLocalVar Global W32_DllGetClassObjectProc *w32_dia_get_class_object = 0;

ThreadLocalVar Global W32_DebugInfo *w32_debug_info_free_list = 0;

Function W32_DebugInfo *
W32_DebugInfoFromHandle(OpaqueHandle handle)
{
    W32_DebugInfo *result = 0;
    
    W32_DebugInfo *p = PtrFromInt(handle.p);
    if(0 != p && p->generation == handle.g)
    {
        result = p;
    }
    
    return result;
}

Function OpaqueHandle
DBG_InfoAlloc(S8 filename)
{
    ArenaTemp scratch = OS_TCtxScratchBegin(0, 0);
    
    if(0 == w32_dia_module)
    {
        w32_dia_module = LoadLibraryW(L"MSDIA140");
    }
    
    if(0 == w32_dia_module)
    {
        OS_TCtxErrorPush(S8("Could not load msdia140.dll"));
    }
    else
    {
        w32_dia_get_class_object = (W32_DllGetClassObjectProc *)GetProcAddress(w32_dia_module, "DllGetClassObject");
    }
    
    IClassFactory *class_factory;
    if(0 == w32_dia_get_class_object)
    {
        OS_TCtxErrorPush(S8("Could not load 'DllGetClassObject' from msdia140.dll"));
    }
    else
    {
        HRESULT hr = w32_dia_get_class_object(&CLSID_DiaSource, &IID_IClassFactory, &class_factory);
        if(!SUCCEEDED(hr))
        {
            class_factory = 0;
            OS_TCtxErrorPush(S8("Could not get class object from msdia140.dll"));
        }
    }
    
    IDiaDataSource *source;
    if(0 != class_factory)
    {
        HRESULT hr = IClassFactory_CreateInstance(class_factory, 0, &IID_IDiaDataSource, (void **)&source);
        if(!SUCCEEDED(hr))
        {
            OS_TCtxErrorPush(S8("Could not create DIA data source"));
            source = 0;
        }
    }
    
    B32 did_load_data = False;
    if(0 != source)
    {
        S16 filename_s16 = S16FromS8(scratch.arena, filename);
        HRESULT hr = IDiaDataSource_loadDataFromPdb(source, filename_s16.buffer);
        if(SUCCEEDED(hr))
        {
            did_load_data = True;
        }
        else
        {
            hr = IDiaDataSource_loadDataForExe(source, filename_s16.buffer, 0, 0);
            if(SUCCEEDED(hr))
            {
                did_load_data = True;
            }
            else
            {
                OS_TCtxErrorPushFmt("Could not load data for PDB or EXE '%.*s'", FmtS8(filename));
            }
        }
    }
    
    IDiaSession *session;
    if(did_load_data)
    {
        HRESULT hr = IDiaDataSource_openSession(source, &session);
        if(!SUCCEEDED(hr))
        {
            OS_TCtxErrorPush(S8("Could not open a DIA session"));
            session = 0;
        }
    }
    
    IDiaSymbol *globals;
    if(0 != session)
    {
        HRESULT hr = IDiaSession_get_globalScope(session, &globals);
        if(!SUCCEEDED(hr))
        {
            OS_TCtxErrorPush(S8("Could not get global scope from DIA session"));
            globals = 0;
        }
    }
    
    OpaqueHandle result = {0};
    if(0 != globals)
    {
        W32_DebugInfo *info = w32_debug_info_free_list;
        if(0 == info)
        {
            info = PushArray(OS_TCtxGet()->permanent_arena, W32_DebugInfo, 1);
        }
        else
        {
            w32_debug_info_free_list = w32_debug_info_free_list->next_free;
        }
        info->generation += 1;
        
        info->source = source;
        info->session = session;
        info->exe = globals;
        
        result.p = IntFromPtr(info);
        result.g = info->generation;
    }
    
    ArenaTempEnd(scratch);
    
    return result;
}

Function void
DBG_InfoRelease(OpaqueHandle info)
{
    W32_DebugInfo *i = W32_DebugInfoFromHandle(info);
    if(0 == i)
    {
        OS_TCtxErrorPush(S8("Invalid handle passed to DBG_InfoRelease"));
    }
    else
    {
        IDiaSymbol_Release(i->exe);
        IDiaSession_Release(i->session);
        IDiaDataSource_Release(i->source);
        
        i->next_free = w32_debug_info_free_list;
        w32_debug_info_free_list = i;
    }
}

Function S8List
DBG_InfoS8ListFromCompilands(Arena *arena, OpaqueHandle info)
{
    S8List result = {0};
    
    W32_DebugInfo *i = W32_DebugInfoFromHandle(info);
    if(0 == i)
    {
        OS_TCtxErrorPush(S8("Invalid handle passed to DBG_InfoS8ListFromCompilands"));
    }
    else
    {
        IDiaEnumSymbols *children_enumerator;
        HRESULT hr = IDiaSymbol_findChildren(i->exe, SymTagCompiland, 0, nsNone, &children_enumerator);
        if(SUCCEEDED(hr))
        {
            IDiaSymbol *child;
            ULONG celt = 0;
            for(;;)
            {
                HRESULT hr = IDiaEnumSymbols_Next(children_enumerator, 1, &child, &celt);
                if(!SUCCEEDED(hr) || (1 != celt))
                {
                    break;
                }
                
                BSTR name;
                HRESULT got_name = IDiaSymbol_get_name(child, &name);
                if(S_OK == got_name)
                {
                    S8ListAppend(arena, &result, S8FromS16(arena, CStringAsS16(name)));
                    SysFreeString(name);
                }
                
                IDiaSymbol_Release(child);
            }
        }
        else
        {
            OS_TCtxErrorPush(S8("Could not get enumerator for children of global scope"));
        }
    }
    
    return result;
}

Function DBG_SymTable
DBG_SymTableMake(Arena *arena, U64 cap)
{
    DBG_SymTable result = {0};
    result.cap = cap;
    result.buckets = PushArray(arena, DBG_Sym *, cap);
    return result;
}

Function void
DBG_SymTableAddAllSymbolsFromCompiland(Arena *arena, DBG_SymTable *table, OpaqueHandle info, S8 compiland_name)
{
    W32_DebugInfo *i = W32_DebugInfoFromHandle(info);
    if(0 == i)
    {
        OS_TCtxErrorPush(S8("Invalid handle passed to DBG_SymTableAddAllSymbolsFromCompiland"));
    }
    else
    {
        ArenaTemp scratch = OS_TCtxScratchBegin(&arena, 1);
        
        S16 compiland_name_s16 = S16FromS8(scratch.arena, compiland_name);
        
        IDiaSymbol *compiland = 0;
        {
            IDiaEnumSymbols *children_enumerator;
            HRESULT hr = IDiaSymbol_findChildren(i->exe, SymTagCompiland, compiland_name_s16.buffer, nsNone, &children_enumerator);
            if(SUCCEEDED(hr))
            {
                IDiaSymbol *child;
                ULONG celt = 0;
                for(;;)
                {
                    HRESULT hr = IDiaEnumSymbols_Next(children_enumerator, 1, &child, &celt);
                    if(SUCCEEDED(hr) && (1 == celt))
                    {
                        if(0 != compiland)
                        {
                            IDiaSymbol_Release(compiland);
                        }
                        compiland = child;
                    }
                    else
                    {
                        break;
                    }
                }
            }
            else
            {
                OS_TCtxErrorPush(S8("Could not get enumerator for children of global scope"));
            }
        }
        
        if(0 != compiland)
        {
            IDiaEnumSymbols *children_enumerator;
            HRESULT hr = IDiaSymbol_findChildren(compiland, SymTagFunction, 0, nsNone, &children_enumerator);
            if(SUCCEEDED(hr))
            {
                IDiaSymbol *child;
                ULONG celt = 0;
                for(;;)
                {
                    HRESULT hr = IDiaEnumSymbols_Next(children_enumerator, 1, &child, &celt);
                    if(!SUCCEEDED(hr) || (1 != celt))
                    {
                        break;
                    }
                    
                    DBG_SymKind kind = DBG_SymKind_NONE;
                    S8 name = {0};
                    U64 virtual_address = 0;
                    U64 len = 0;
                    
                    DWORD sym_tag;
                    HRESULT got_sym_tag = IDiaSymbol_get_symTag(child, &sym_tag);
                    if(S_OK == got_sym_tag)
                    {
                        if(SymTagFunction == sym_tag)
                        {
                            kind = DBG_SymKind_Function;
                        }
                        else if(SymTagData == sym_tag)
                        {
                            kind = DBG_SymKind_Global;
                        }
                    }
                    
                    BSTR sym_name;
                    HRESULT got_name = IDiaSymbol_get_name(child, &sym_name);
                    if(S_OK == got_name)
                    {
                        name = S8FromS16(arena, CStringAsS16(sym_name));
                        SysFreeString(sym_name);
                    }
                    
                    DWORD rva;
                    HRESULT got_virtual_address = IDiaSymbol_get_relativeVirtualAddress(child, &rva);
                    if(S_OK == got_virtual_address)
                    {
                        HMODULE base = GetModuleHandleW(0);
                        virtual_address = IntFromPtr(base) + rva;
                    }
                    
                    HRESULT got_len = IDiaSymbol_get_length(child, &len);
                    
                    if(DBG_SymKind_NONE != kind &&
                       name.len > 0 &&
                       virtual_address != 0 &&
                       S_OK == got_len)
                    {
                        U64 hash[2]  = {0};
                        MurmurHash3_x64_128(&virtual_address, sizeof(virtual_address), 0, hash);
                        U64 bucket_index = (hash[0] % table->cap);
                        
                        DBG_Sym *sym = table->buckets[bucket_index];
                        while(0 != sym && virtual_address != sym->virtual_address)
                        {
                            sym = sym->next;
                        }
                        
                        if(0 == sym)
                        {
                            sym = PushArray(arena, DBG_Sym, 1);
                            sym->next = table->buckets[bucket_index];
                            table->buckets[bucket_index] = sym;
                        }
                        
                        sym->kind = kind;
                        sym->name = name;
                        sym->virtual_address = virtual_address;
                        sym->len = len;
                    }
                    
                    IDiaSymbol_Release(child);
                }
            }
            else
            {
                OS_TCtxErrorPushFmt("Could not get enumerator for children of '%.*s'", FmtS8(compiland_name));
            }
        }
        
        ArenaTempEnd(scratch);
    }
}

Function DBG_StackTrace
DBG_StackTraceFromWalkingStack(Arena *arena)
{
    U64 capture_chunk_size = 8;
    
    DBG_StackTrace result = {0};
    result.stack = PushArray(arena, void *, capture_chunk_size);
    
    for(;;)
    {
        U64 n_frames_captured = RtlCaptureStackBackTrace(result.count, capture_chunk_size, &result.stack[result.count], 0);
        result.count += n_frames_captured;
        
        if(n_frames_captured == capture_chunk_size)
        {
            PushArray(arena, void *, capture_chunk_size);
        }
        else
        {
            break;
        }
    }
    
    return result;
}

Function S8List
DBG_S8ListFromStackTraceAndSymbols(Arena *arena, DBG_StackTrace stack_trace, DBG_SymTable *table)
{
    S8List result = {0};
    
    // TODO(tbt): get and use debug info for the line of the callsite
    
    for(U64 frame_index = 0; frame_index < stack_trace.count; frame_index += 1)
    {
        U64 return_address = IntFromPtr(stack_trace.stack[frame_index]);
        for(U64 bucket_index = 0; bucket_index < table->cap; bucket_index += 1)
        {
            // NOTE(tbt): lol
            for(DBG_Sym *symbol = table->buckets[bucket_index]; 0 != symbol; symbol = symbol->next)
            {
                if(DBG_SymKind_Function == symbol->kind &&
                   symbol->virtual_address <= return_address &&
                   return_address < symbol->virtual_address + symbol->len)
                {
                    S8ListAppend(arena, &result, symbol->name);
                    goto continue_next_frame;
                }
            }
        }
        S8ListAppend(arena, &result, S8("?"));
        continue_next_frame:;
    }
    
    return result;
}
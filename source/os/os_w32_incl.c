
//~NOTE(tbt): platform specific system headers

#define COBJMACROS
#define WIN32_LEAN_AND_MEAN

#pragma push_macro("Function")
#pragma push_macro("Persist")
#undef Function
#undef Persist

#include <windows.h>

#include <commctrl.h>
#include <commdlg.h>
#include <d3d11.h>
#include <d3dcompiler.h>
#include <intrin.h>
#include <shellapi.h>
#include <shlobj_core.h>
#include <shlwapi.h>
#include <thumbcache.h>
#include <timeapi.h>
#include <wincrypt.h>
#include <userenv.h>

#pragma pop_macro("Function")
#pragma pop_macro("Persist")

//~NOTE(tbt): libraries

#pragma comment (lib, "advapi32.lib")
#pragma comment (lib, "comdlg32.lib")
#pragma comment (lib, "gdi32.lib")
#pragma comment (lib, "kernel32.lib")
#pragma comment (lib, "ole32.lib")
#pragma comment (lib, "shell32.lib")
#pragma comment (lib, "shlwapi.lib")
#pragma comment (lib, "user32.lib")
#pragma comment (lib, "userenv.lib")
#pragma comment (lib, "uuid.lib")
#pragma comment (lib, "winmm.lib")

//~NOTE(tbt): os layer backend implementation files

#include "os_w32_internal.h"

#include "os_w32_internal.c"
#include "os_w32_core.c"
#include "os_w32_file_io.c"
#include "os_w32_thread.c"

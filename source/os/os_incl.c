
//~NOTE(tbt): platform specific backends

#if BuildOSWindows
# include "os_w32_incl.c"
#elif BuildOSMac
# include "os_posix_incl.c"
# include "os_mac_incl.m"
#elif BuildOSLinux
# include "os_posix_incl.c"
# include "os_linux_incl.c"
#else
# error "no backend for current platform"
#endif

//~NOTE(tbt): platform agnostic os-layer utility implementation files

#include "os_thread.c"
#include "os_core.c"

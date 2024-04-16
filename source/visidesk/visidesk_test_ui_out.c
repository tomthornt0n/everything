
#include "external/fonts.h"

#include "base/base_incl.h"
#include "os/os_incl.h"
#include "graphics/graphics_incl.h"
#include "renderer_2d/renderer_2d_incl.h"
#include "renderer_line/renderer_line_incl.h"
#include "font/font_incl.h"
#include "draw/draw_incl.h"
#include "visidesk_incl.h"
#include "ui/ui_incl.h"

#include "base/base_incl.c"
#include "os/os_incl.c"
#include "graphics/graphics_incl.c"
#include "renderer_2d/renderer_2d_incl.c"
#include "renderer_line/renderer_line_incl.c"
#include "font/font_incl.c"
#include "draw/draw_incl.c"
#include "ui/ui_incl.c"
#include "visidesk_incl.c"


Function void
WindowHookOpen(OpaqueHandle window)
{
    
}

Function void
WindowHookDraw(OpaqueHandle window)
{
    UI_Begin(G_WindowUserDataGet(window));
    
    UI_Row() UI_Height(UI_SizeFill())
    {
        UI_Width(UI_SizePct(1.000f, 0.0f)) UI_Column() UI_Width(UI_SizeFill())
        {
            UI_Height(UI_SizePct(0.144f, 0.0f)) UI_NodeMake(UI_Flags_DrawStroke, S8(""));
            UI_Height(UI_SizePct(0.856f, 0.0f)) UI_Row() UI_Height(UI_SizeFill())
            {
                UI_Width(UI_SizePct(0.532f, 0.0f)) UI_NodeMake(UI_Flags_DrawStroke, S8(""));
                UI_Width(UI_SizePct(0.468f, 0.0f)) UI_NodeMake(UI_Flags_DrawStroke, S8(""));
            }
        }
    }
    
    UI_End();
}


Function void
WindowHookClose(OpaqueHandle window)
{
    
}

EntryPoint
{
    OS_Init();
    G_Init();
    R2D_Init();
    FONT_Init();
    
    G_WindowHooks window_hooks =
    {
        WindowHookOpen,
        WindowHookDraw,
        WindowHookClose,
    };
    OpaqueHandle window = G_WindowOpen(S8("visidesk"), V2I(1024, 768), window_hooks, 0);
    
    UI_State *ui_state = UI_Init(window);
    G_WindowUserDataSet(window, ui_state);
    
    G_MainLoop();
    
    R2D_Cleanup();
    G_Cleanup();
    OS_Cleanup();
}
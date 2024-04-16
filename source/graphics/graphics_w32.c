
#pragma push_macro("Function")
#pragma push_macro("Persist")
#undef Function
#undef Persist

#include <windowsx.h>

#pragma pop_macro("Function")
#pragma pop_macro("Persist")

#define COBJMACROS
#include <d3d11.h>
#include <dxgi1_2.h>

#pragma comment (lib, "advapi32.lib")
#pragma comment (lib, "d3d11.lib")
#pragma comment (lib, "dxgi.lib")
#pragma comment (lib, "dxguid.lib")

#include "graphics_w32_resources.h"

typedef struct W32_Window W32_Window;
struct W32_Window
{
    Arena *arena;
    W32_Window *next;
    W32_Window *prev;
    
    HWND window_handle;
    IDXGISwapChain1 *swap_chain;
    ID3D11RenderTargetView *render_target_view;
    ID3D11Resource *render_target_texture;
    ID3D11DepthStencilView *depth_stencil_view;
    
    B32 should_close;
    G_EventQueue events;
    U64 is_key_down[(G_Key_MAX + 64) >> 6];
    F32 scale_factor;
    V2F prev_mouse_position;
    V2I dimensions;
    
    G_WindowHooks hooks;
    
    void *user_data;
    
    B32 is_vsync;
    G_CursorKind volatile cursor_kind;
    
    WINDOWPLACEMENT prev_window_placement;
    
    V4F clear_colour;
    
    B32 is_initialised;
};

typedef struct W32_GApp W32_GApp;
struct W32_GApp
{
    Arena *frame_arena;
    
    HANDLE instance_handle;
    ID3D11Device *device;
    ID3D11DeviceContext *device_ctx;
    
    W32_Window *windows_first;
    W32_Window *windows_last;
    
    B32 volatile should_wait_for_events;
    U64 volatile should_not_wait_for_events_until;
    
    U64 frame_start;
    U64 frame_time;
};

Global W32_GApp w32_g_app = {0};

Global OpaqueHandle w32_dark_mode_thread = {0};
Global B32 w32_is_dark_mode = False;

#define W32_WindowClassName ApplicationNameWString L"__WindowClass"

//~NOTE(tbt): internal functions

Function W32_Window *
W32_WindowFromHandle(OpaqueHandle window)
{
    ReadOnlyVar Persist W32_Window dummy = {0};
    W32_Window *result = PtrFromInt(window.p);
    if(0 == result)
    {
        result = &dummy;
    }
    return result;
}

Function OpaqueHandle
W32_HandleFromWindow(W32_Window *window)
{
    OpaqueHandle result = {0};
    result.p = IntFromPtr(window);
    return result;
}

Function void
W32_WindowPresent(W32_Window *window, B32 should_vsync)
{
    HRESULT hr = IDXGISwapChain_Present(window->swap_chain, !!should_vsync, 0);
    OS_TCtxErrorPushCond(!FAILED(hr), S8("DXGI error - Failed to present swapchain. Device lost?"));
    if(DXGI_STATUS_OCCLUDED == hr)
    {
        // NOTE(tbt): window is minimised, cannot vsync
        //            instead sleep for a little bit
        Sleep(10);
    }
}

Function void
W32_IsKeyDownSet(W32_Window *window, G_Key key, B32 is_down)
{
    if(is_down)
    {
        window->is_key_down[key >> 6] |= Bit(key & 0x3F);
    }
    else
    {
        window->is_key_down[key >> 6] &= ~Bit(key & 0x3F);
    }
}

Function void
W32_WindowRelease(W32_Window *window)
{
    if(0 == window->prev)
    {
        w32_g_app.windows_first = window->next;
    }
    else
    {
        window->prev->next = window->next;
    }
    if(0 == window->next)
    {
        w32_g_app.windows_last = window->prev;
    }
    else
    {
        window->next->prev = window->prev;
    }
    
    IDXGISwapChain1_Release(window->swap_chain);
    if(0 != window->render_target_view)
    {
        ID3D11RenderTargetView_Release(window->render_target_view);
        ID3D11Resource_Release(window->render_target_texture);
        ID3D11DepthStencilView_Release(window->depth_stencil_view);
    }
    
    DestroyWindow(window->window_handle);
    
    ArenaRelease(window->arena);
}

Function void
W32_DarkModeThreadProc(void *user_data)
{
    OS_ThreadNameSet(S8(ApplicationNameString ": W32 dark mode"));
    
    HKEY key = INVALID_HANDLE_VALUE;
    RegOpenKeyExW(HKEY_CURRENT_USER,
                  L"Software\\Microsoft\\Windows\\CurrentVersion\\Themes\\Personalize",
                  0, KEY_READ | KEY_NOTIFY,
                  &key);
    for(;;)
    {
        DWORD buffer = 0;
        DWORD buffer_size = sizeof(buffer);
        LSTATUS status =
            RegGetValueW(key, 0,
                         L"AppsUseLightTheme",
                         RRF_RT_DWORD, 0,
                         (void *)&buffer,
                         &buffer_size);
        if(ERROR_SUCCESS == status)
        {
            w32_is_dark_mode = !buffer;
        }
        
        RegNotifyChangeKeyValue(key, FALSE, REG_NOTIFY_CHANGE_LAST_SET, 0, FALSE);
    }
}

Function LRESULT
W32_WindowProc(HWND window_handle,
               UINT message,
               WPARAM w_param, LPARAM l_param)
{
    LRESULT result = 0;
    
    W32_Window *w = PtrFromInt(GetWindowLongPtrW(window_handle, GWLP_USERDATA));
    OpaqueHandle window = W32_HandleFromWindow(w);
    
    Persist B32 is_mouse_hover_active = False;
    Persist I32 active_pointer = -1;
    
    G_ModifierKeys modifiers = 0;
    if(GetKeyState(VK_CONTROL) & 0x8000)
    {
        modifiers |= G_ModifierKeys_Ctrl;
    }
    if(GetKeyState(VK_SHIFT) & 0x8000)
    {
        modifiers |= G_ModifierKeys_Shift;
    }
    if(GetKeyState(VK_MENU) & 0x8000)
    {
        modifiers |= G_ModifierKeys_Alt;
    }
    if(GetKeyState(VK_LWIN) & 0x8000)
    {
        modifiers |= G_ModifierKeys_Super;
    }
    
    switch(message)
    {
        case(WM_CLOSE):
        case(WM_QUIT):
        case(WM_DESTROY):
        {
            w->should_close = True;
        } break;
        
        case(WM_LBUTTONDOWN):
        case(WM_LBUTTONUP):
        {
            G_Event event =
            {
                .kind = G_EventKind_Key,
                .modifiers = modifiers,
                .key = G_Key_MouseButtonLeft,
                .is_down = (WM_LBUTTONDOWN == message),
                .position = V2F(GET_X_LPARAM(l_param), GET_Y_LPARAM(l_param)),
                .pressure = 1.0f,
            };
            G_EventQueuePush(&w->events, event);
            W32_IsKeyDownSet(w, event.key, event.is_down);
            
            if(WM_LBUTTONUP == message)
            {
                active_pointer = -1;
            }
        } break;
        
        case(WM_POINTERDOWN):
        case(WM_POINTERUP):
        {
            I32 pointer_id = GET_POINTERID_WPARAM(w_param);
            
            POINT position;
            position.x = GET_X_LPARAM(l_param);
            position.y = GET_Y_LPARAM(l_param);
            ScreenToClient(window_handle, &position);
            
            F32 pressure = 0.0f;
            POINTER_PEN_INFO pen_info;
            if(GetPointerPenInfo(pointer_id, &pen_info))
            {
                if(0 < pen_info.pressure)
                {
                    pressure = (F32)pen_info.pressure / 1024.0f;
                }
            }
            
            G_Event event =
            {
                .kind = G_EventKind_Key,
                .modifiers = modifiers,
                .key = G_Key_MouseButtonLeft,
                .is_down = (WM_POINTERDOWN == message),
                .position = V2F(position.x, position.y),
                .pressure = pressure,
            };
            G_EventQueuePush(&w->events, event);
            W32_IsKeyDownSet(w, event.key, event.is_down);
            
            if(WM_POINTERUP == message)
            {
                active_pointer= -1;
            }
            else if(-1 == active_pointer)
            {
                active_pointer = pointer_id;
            }
        } break;
        
        case(WM_LBUTTONDBLCLK):
        {
            G_Event event =
            {
                .kind = G_EventKind_Key,
                .modifiers = modifiers,
                .key = G_Key_MouseButtonDoubleClick,
                .is_down = True,
                .position = V2F(GET_X_LPARAM(l_param), GET_Y_LPARAM(l_param)),
            };
            G_EventQueuePush(&w->events, event);
        } break;
        
        case(WM_MBUTTONDOWN):
        case(WM_MBUTTONUP):
        {
            G_Event event =
            {
                .kind = G_EventKind_Key,
                .modifiers = modifiers,
                .key = G_Key_MouseButtonMiddle,
                .is_down = (WM_MBUTTONDOWN == message),
                .position = V2F(GET_X_LPARAM(l_param), GET_Y_LPARAM(l_param)),
            };
            G_EventQueuePush(&w->events, event);
            W32_IsKeyDownSet(w, event.key, event.is_down);
        } break;
        
        case(WM_RBUTTONDOWN):
        case(WM_RBUTTONUP):
        {
            G_Event event =
            {
                .kind = G_EventKind_Key,
                .modifiers = modifiers,
                .key = G_Key_MouseButtonRight,
                .is_down = (WM_RBUTTONDOWN == message),
                .position = V2F(GET_X_LPARAM(l_param), GET_Y_LPARAM(l_param)),
            };
            G_EventQueuePush(&w->events, event);
            W32_IsKeyDownSet(w, event.key, event.is_down);
        } break;
        
        case(WM_XBUTTONDOWN):
        case(WM_XBUTTONUP):
        {
            UINT button = GET_XBUTTON_WPARAM(w_param);
            G_Event event =
            {
                .kind = G_EventKind_Key,
                .modifiers = modifiers,
                .key = (XBUTTON1 == button) ? G_Key_MouseButtonForward : G_Key_MouseButtonBackward,
                .is_down = (WM_XBUTTONDOWN == message),
                .position = V2F(GET_X_LPARAM(l_param), GET_Y_LPARAM(l_param)),
            };
            G_EventQueuePush(&w->events, event);
            W32_IsKeyDownSet(w, event.key, event.is_down);
        } break;
        
        case(WM_POINTERUPDATE):
        {
            I32 pointer_id = GET_POINTERID_WPARAM(w_param);
            
            if(-1 == active_pointer || pointer_id == active_pointer)
            {
                POINT position;
                position.x = GET_X_LPARAM(l_param);
                position.y = GET_Y_LPARAM(l_param);
                SetCursorPos(position.x, position.y);
                ScreenToClient(window_handle, &position);
                
                F32 pressure = 0.0f;
                POINTER_PEN_INFO pen_info;
                if(GetPointerPenInfo(pointer_id, &pen_info))
                {
                    if(0 < pen_info.pressure)
                    {
                        pressure = (F32)pen_info.pressure / 1024.0f;
                    }
                }
                
                G_Event event =
                {
                    .kind = G_EventKind_MouseMove,
                    .modifiers = modifiers,
                    .position = V2F(position.x, position.y),
                    .pressure = pressure,
                };
                G_EventQueuePush(&w->events, event);
            }
        } break;
        
        case(WM_MOUSEMOVE):
        {
            if(-1 == active_pointer)
            {
                G_Event event =
                {
                    .kind = G_EventKind_MouseMove,
                    .modifiers = modifiers,
                    .position = V2F(GET_X_LPARAM(l_param), GET_Y_LPARAM(l_param)),
                    .pressure = 1.0f,
                };
                event.delta = Sub2F(event.position, w->prev_mouse_position);
                G_EventQueuePush(&w->events, event);
                
                w->prev_mouse_position = event.position;
                
                if(!is_mouse_hover_active)
                {
                    is_mouse_hover_active = True;
                    TRACKMOUSEEVENT track_mouse_event =
                    {
                        .cbSize = sizeof(track_mouse_event),
                        .dwFlags = TME_LEAVE,
                        .hwndTrack = window_handle,
                        .dwHoverTime = HOVER_DEFAULT,
                    };
                    TrackMouseEvent(&track_mouse_event);
                }
            }
        } break;
        
        case(WM_MOUSELEAVE):
        {
            is_mouse_hover_active = False;
            SetCursor(LoadCursorW(0, IDC_ARROW));
            G_Event event =
            {
                .kind = G_EventKind_MouseLeave,
                .modifiers = modifiers,
            };
            G_EventQueuePush(&w->events, event);
            
            for(G_Key key = 0; key < G_Key_MAX; key += 1)
            {
                if(w->is_key_down[key >> 6] & Bit(key & 0x3F))
                {
                    G_Event event =
                    {
                        .kind = G_EventKind_Key,
                        .modifiers = modifiers,
                        .key = key,
                        .is_down = False,
                    };
                    G_EventQueuePush(&w->events, event);
                }
            }
            MemorySet(w->is_key_down, 0, sizeof(w->is_key_down));
        } break;
        
        case(WM_MOUSEWHEEL):
        {
            V2F delta = V2F(0.0f, GET_WHEEL_DELTA_WPARAM(w_param) / 120.0f);
            G_Event event =
            {
                .kind = G_EventKind_MouseScroll,
                .modifiers = modifiers,
                .delta = delta,
            };
            G_EventQueuePush(&w->events, event);
        } break;
        
        case(WM_MOUSEHWHEEL):
        {
            V2F delta = V2F(GET_WHEEL_DELTA_WPARAM(w_param) / 120.0f, 0.0f);
            G_Event event =
            {
                .kind = G_EventKind_MouseScroll,
                .modifiers = modifiers,
                .delta = delta,
            };
            G_EventQueuePush(&w->events, event);
        } break;
        
        case(WM_SETCURSOR):
        {
            V2I window_dimensions = G_WindowDimensionsGet(window);
            V2F mouse_position = G_WindowMousePositionGet(window);
            
            B32 should_hide_cursor = False;
            
            if(is_mouse_hover_active &&
               mouse_position.x > 0 && mouse_position.x < window_dimensions.x &&
               mouse_position.y > 0 && mouse_position.y < window_dimensions.y)
            {
                switch(w->cursor_kind)
                {
                    default: break;
                    
                    case(G_CursorKind_HResize):
                    {
                        SetCursor(LoadCursorW(0, IDC_SIZEWE));
                    } break;
                    
                    case(G_CursorKind_VResize):
                    {
                        SetCursor(LoadCursorW(0, IDC_SIZENS));
                    } break;
                    
                    case(G_CursorKind_Resize):
                    {
                        SetCursor(LoadCursorW(0, IDC_SIZENWSE));
                    } break;
                    
                    case(G_CursorKind_Move):
                    {
                        SetCursor(LoadCursorW(0, IDC_SIZEALL));
                    } break;
                    
                    case(G_CursorKind_Loading):
                    {
                        SetCursor(LoadCursorW(0, IDC_WAIT));
                    } break;
                    
                    case(G_CursorKind_No):
                    {
                        SetCursor(LoadCursorW(0, IDC_NO));
                    } break;
                    
                    case(G_CursorKind_Default):
                    {
                        SetCursor(LoadCursorW(0, IDC_ARROW));
                    } break;
                    
                    case(G_CursorKind_IBeam):
                    {
                        SetCursor(LoadCursorW(0, IDC_IBEAM));
                    } break;
                    
                    case(G_CursorKind_Hidden):
                    {
                        should_hide_cursor = True;
                    } break;
                }
            }
            else
            {
                result = DefWindowProc(window_handle, message, w_param, l_param);
            }
            
            if(should_hide_cursor)
            {
                while(ShowCursor(FALSE) >= 0);
            }
            else
            {
                while(ShowCursor(TRUE) < 0);
            }
        } break;
        
        case(WM_SIZE):
        {
            V2I window_dimensions = V2I(LOWORD(l_param), HIWORD(l_param));
            
            G_Event event =
            {
                .kind = G_EventKind_WindowSize,
                .modifiers = modifiers,
                .prev_size = w->dimensions,
                .size = window_dimensions,
            };
            G_EventQueuePush(&w->events, event);
            
            w->dimensions = window_dimensions;
            
            if(0 != w->render_target_view)
            {
                ID3D11DeviceContext_ClearState(w32_g_app.device_ctx);
                ID3D11RenderTargetView_Release(w->render_target_view);
                ID3D11RenderTargetView_Release(w->depth_stencil_view);
                ID3D11Resource_Release(w->render_target_texture);
                w->render_target_view = 0;
                w->depth_stencil_view = 0;
                w->render_target_texture = 0;
            }
            if(window_dimensions.x > 0 &&
               window_dimensions.y > 0)
            {
                //-NOTE(tbt): resize swapchain
                IDXGISwapChain_ResizeBuffers(w->swap_chain, 0,
                                             window_dimensions.x,
                                             window_dimensions.y,
                                             DXGI_FORMAT_UNKNOWN, 0);
                
                //-NOTE(tbt): recreate render target
                D3D11_RENDER_TARGET_VIEW_DESC render_target_desc =
                {
                    .Format = DXGI_FORMAT_R8G8B8A8_UNORM,
                    .ViewDimension = D3D11_RTV_DIMENSION_TEXTURE2D,
                };
                ID3D11Texture2D *render_target;
                IDXGISwapChain_GetBuffer(w->swap_chain, 0,
                                         &IID_ID3D11Texture2D,
                                         (void **)&render_target);
                w->render_target_texture = (ID3D11Resource*)render_target;
                ID3D11Device_CreateRenderTargetView(w32_g_app.device,
                                                    w->render_target_texture,
                                                    &render_target_desc,
                                                    &w->render_target_view);
                
                //-NOTE(tbt): recreate depth stencil target
                D3D11_TEXTURE2D_DESC depth_stencil_texture_desc =
                {
                    .Width = window_dimensions.x,
                    .Height = window_dimensions.y,
                    .MipLevels = 1,
                    .ArraySize = 1,
                    .Format = DXGI_FORMAT_D24_UNORM_S8_UINT,
                    .SampleDesc = { .Count = 1, },
                    .Usage = D3D11_USAGE_DEFAULT,
                    .BindFlags = D3D11_BIND_DEPTH_STENCIL,
                };
                ID3D11Texture2D *depth_stencil;
                ID3D11Device_CreateTexture2D(w32_g_app.device,
                                             &depth_stencil_texture_desc, 0,
                                             &depth_stencil);
                ID3D11Device_CreateDepthStencilView(w32_g_app.device,
                                                    (ID3D11Resource*)depth_stencil,
                                                    0, &w->depth_stencil_view);
                ID3D11Texture2D_Release(depth_stencil);
            }
        } break;
        
        case(WM_PAINT):
        {
            if(w->is_initialised)
            {
                // TODO(tbt): do i need to do something with the frame timing here?
                //            seems to be pretty messed up regardless - is this a battle i want to get into?
                if(0 != w->render_target_view &&
                   0 != w->depth_stencil_view)
                {
                    ID3D11DeviceContext_ClearRenderTargetView(w32_g_app.device_ctx, w->render_target_view, w->clear_colour.elements);
                    ID3D11DeviceContext_ClearDepthStencilView(w32_g_app.device_ctx, w->depth_stencil_view, D3D11_CLEAR_DEPTH | D3D11_CLEAR_STENCIL, 1.0f, 0);
                }
                ArenaClear(w32_g_app.frame_arena);
                w->hooks.draw(W32_HandleFromWindow(w));
                W32_WindowPresent(w, False);
                
                ValidateRgn(window_handle, 0);
            }
        } break;
        
        case(WM_SYSKEYDOWN):
        case(WM_SYSKEYUP):
        case(WM_KEYDOWN):
        case(WM_KEYUP):
        {
            U64 vk = w_param;
            B32 was_down = !!(l_param & Bit(30));
            B32 is_down = !(l_param & Bit(31));
            
            G_Key key_input = G_Key_None;
            if((vk >= 'A' && vk <= 'Z') ||
               (vk >= '0' && vk <= '9'))
            {
                key_input = (vk >= 'A' && vk <= 'Z') ? G_Key_A + (vk - 'A') : G_Key_0 + (vk - '0');
            }
            else
            {
                if(vk == VK_ESCAPE)
                {
                    key_input = G_Key_Esc;
                }
                else if(vk >= VK_F1 && vk <= VK_F12)
                {
                    key_input = G_Key_F1 + vk - VK_F1;
                }
                else if(vk == VK_OEM_3)
                {
                    key_input = G_Key_GraveAccent;
                }
                else if(vk == VK_OEM_MINUS)
                {
                    key_input = G_Key_Minus;
                }
                else if(vk == VK_OEM_PLUS)
                {
                    key_input = G_Key_Equal;
                }
                else if(vk == VK_BACK)
                {
                    key_input = G_Key_Backspace;
                }
                else if(vk == VK_TAB)
                {
                    key_input = G_Key_Tab;
                }
                else if(vk == VK_SPACE)
                {
                    key_input = G_Key_Space;
                }
                else if(vk == VK_RETURN)
                {
                    key_input = G_Key_Enter;
                }
                else if(vk == VK_CONTROL)
                {
                    key_input = G_Key_Ctrl;
                    modifiers &= ~G_ModifierKeys_Ctrl;
                }
                else if(vk == VK_SHIFT)
                {
                    key_input = G_Key_Shift;
                    modifiers &= ~G_ModifierKeys_Shift;
                }
                else if(vk == VK_MENU)
                {
                    key_input = G_Key_Alt;
                    modifiers &= ~G_ModifierKeys_Alt;
                }
                else if(vk == VK_LWIN)
                {
                    key_input = G_Key_Super;
                    modifiers &= ~G_ModifierKeys_Super;
                }
                else if(vk == VK_UP)
                {
                    key_input = G_Key_Up;
                }
                else if(vk == VK_LEFT)
                {
                    key_input = G_Key_Left;
                }
                else if(vk == VK_DOWN)
                {
                    key_input = G_Key_Down;
                }
                else if(vk == VK_RIGHT)
                {
                    key_input = G_Key_Right;
                }
                else if(vk == VK_DELETE)
                {
                    key_input = G_Key_Delete;
                }
                else if(vk == VK_INSERT)
                {
                    key_input = G_Key_Insert;
                }
                else if(vk == VK_PRIOR)
                {
                    key_input = G_Key_PageUp;
                }
                else if(vk == VK_NEXT)
                {
                    key_input = G_Key_PageDown;
                }
                else if(vk == VK_HOME)
                {
                    key_input = G_Key_Home;
                }
                else if(vk == VK_END)
                {
                    key_input = G_Key_End;
                }
                else if(vk == VK_OEM_2)
                {
                    key_input = G_Key_ForwardSlash;
                }
                else if(vk == VK_OEM_PERIOD)
                {
                    key_input = G_Key_Period;
                }
                else if(vk == VK_OEM_COMMA)
                {
                    key_input = G_Key_Comma;
                }
                else if(vk == VK_OEM_7)
                {
                    key_input = G_Key_Quote;
                }
                else if(vk == VK_OEM_4)
                {
                    key_input = G_Key_LeftBracket;
                }
                else if(vk == VK_OEM_6)
                {
                    key_input = G_Key_RightBracket;
                }
                else if(vk == 186)
                {
                    key_input = G_Key_Colon;
                }
            }
            
            G_Event event =
            {
                .kind = G_EventKind_Key,
                .modifiers = modifiers,
                .key = key_input,
                .is_down = is_down,
                .is_repeat = is_down && was_down,
                .position = G_WindowMousePositionGet(W32_HandleFromWindow(w)),
            };
            G_EventQueuePush(&w->events, event);
            
            W32_IsKeyDownSet(w, event.key, event.is_down);
            
            result = DefWindowProc(window_handle, message, w_param, l_param);
        } break;
        
        case(WM_CHAR):
        {
            U32 char_input = w_param;
            if(VK_RETURN == char_input ||
               (char_input >= 32 &&
                char_input != VK_ESCAPE &&
                char_input != 127))
            {
                if(VK_RETURN == char_input)
                {
                    char_input = '\n';
                }
                G_Event event =
                {
                    .kind = G_EventKind_Char,
                    .modifiers = modifiers,
                    .codepoint = char_input,
                };
                G_EventQueuePush(&w->events, event);
            }
        } break;
        
        case(WM_DPICHANGED):
        {
            I32 dpi_x = LOWORD(w_param);
            I32 dpi_y = HIWORD(w_param);
            F32 scale_factor = (F32)dpi_x / 96.0f;
            w->scale_factor = scale_factor;
            
            G_Event event =
            {
                .kind = G_EventKind_DPIChange,
                .modifiers = modifiers,
                .size = V2I(dpi_x, dpi_y),
            };
            G_EventQueuePush(&w->events, event);
        } break;
        
        default:
        {
            result = DefWindowProc(window_handle, message, w_param, l_param);
        } break;
    }
    
    return result;
}

//~NOTE(tbt): initialisation and cleanup

Function void
G_Init(void)
{
    w32_g_app.instance_handle = GetModuleHandle(0);
    w32_g_app.frame_arena = ArenaAlloc(Gigabytes(2));
    
    w32_dark_mode_thread = OS_ThreadStart(W32_DarkModeThreadProc, 0);
    
    //-NOTE(tbt): register the class for windows to use
    WNDCLASSW window_class = 
    {
        .style = CS_HREDRAW | CS_VREDRAW | CS_DBLCLKS,
        .lpfnWndProc = W32_WindowProc,
        .hInstance = w32_g_app.instance_handle,
        .lpszClassName = W32_WindowClassName,
        .hCursor = LoadCursorW(0, IDC_ARROW),
        .hIcon = LoadIconW(w32_g_app.instance_handle, MAKEINTRESOURCE(W32_WindowIcon)),
    };
    RegisterClassW(&window_class);
    SetThreadDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);
    
    //-NOTE(tbt): setup D3D11 device
    UINT flags = D3D11_CREATE_DEVICE_SINGLETHREADED;
#if BuildModeDebug
    flags |= D3D11_CREATE_DEVICE_DEBUG;
#endif
    D3D_FEATURE_LEVEL levels[] = { D3D_FEATURE_LEVEL_11_0 };
    D3D11CreateDevice(0,
                      D3D_DRIVER_TYPE_HARDWARE,
                      0,
                      flags,
                      levels,
                      ArrayCount(levels),
                      D3D11_SDK_VERSION,
                      &w32_g_app.device,
                      0,
                      &w32_g_app.device_ctx);
    
    //-NOTE(tbt): D3D11 debugging
#if BuildModeDebug
    ID3D11InfoQueue *info;
    IProvideClassInfo_QueryInterface(w32_g_app.device, &IID_ID3D11InfoQueue, (void**)&info);
    ID3D11InfoQueue_SetBreakOnSeverity(info, D3D11_MESSAGE_SEVERITY_CORRUPTION, TRUE);
    ID3D11InfoQueue_SetBreakOnSeverity(info, D3D11_MESSAGE_SEVERITY_ERROR, TRUE);
    ID3D11InfoQueue_Release(info);
#endif
}

Function void
G_Cleanup(void)
{
    OS_ThreadStop(w32_dark_mode_thread);
    
    W32_Window *next = 0;
    for(W32_Window *w = w32_g_app.windows_first;
        0 != w;
        w = next)
    {
        w->hooks.close(W32_HandleFromWindow(w));
        W32_WindowRelease(w);
    }
    ArenaRelease(w32_g_app.frame_arena);
    
    ID3D11Device_Release(w32_g_app.device);
    ID3D11DeviceContext_Release(w32_g_app.device_ctx);
}

//~NOTE(tbt): graphical app

Function OpaqueHandle
G_WindowOpen(S8 title, V2I dimensions, G_WindowHooks hooks, void *user_data)
{
    OS_AssertMainThread();
    
    ArenaTemp scratch = OS_TCtxScratchBegin(0, 0);
    
    //-NOTE(tbt): allocate window
    Arena *arena = ArenaAlloc(Gigabytes(2));
    W32_Window *window = PushArray(arena, W32_Window, 1);
    OpaqueHandle result = W32_HandleFromWindow(window);
    window->arena = arena;
    window->hooks = hooks;
    window->scale_factor = 1.0f;
    S16 title_s16 = S16FromS8(scratch.arena, title);
    DWORD window_styles_ex = (WS_EX_APPWINDOW | WS_EX_NOREDIRECTIONBITMAP);
    DWORD window_styles = WS_OVERLAPPEDWINDOW;
    RECT window_rect = { 0, 0, dimensions.x, dimensions.y };
    AdjustWindowRect(&window_rect, window_styles, FALSE);
    G_WindowUserDataSet(result, user_data);
    
    //-NOTE(tbt): create native window
    window->window_handle = CreateWindowExW(window_styles_ex,
                                            W32_WindowClassName,
                                            title_s16.buffer,
                                            window_styles,
                                            CW_USEDEFAULT, CW_USEDEFAULT,
                                            window_rect.right - window_rect.left,
                                            window_rect.bottom - window_rect.top,
                                            0, 0, w32_g_app.instance_handle,
                                            0);
    SetWindowLongPtrW(window->window_handle, GWLP_USERDATA, IntFromPtr(window));
    window->scale_factor = (F32)GetDpiForWindow(window->window_handle) / 96.0f;
    
    //-NOTE(tbt): create DX swapchain
    IDXGIFactory2 *dxgi_factory = 0;
    {
        IDXGIDevice *dxgi_device = 0;
        ID3D11Device_QueryInterface(w32_g_app.device, &IID_IDXGIDevice, &dxgi_device);
        IDXGIAdapter *dxgi_adapter = 0;
        IDXGIDevice_GetAdapter(dxgi_device, &dxgi_adapter);
        IDXGIAdapter_GetParent(dxgi_adapter, &IID_IDXGIFactory, &dxgi_factory);
        IDXGIAdapter_Release(dxgi_adapter);
        IDXGIDevice_Release(dxgi_device);
    }
    DXGI_SWAP_CHAIN_DESC1 swap_chain_desc =
    {
        .Format = DXGI_FORMAT_R8G8B8A8_UNORM,
        .SampleDesc = {
            .Count = 1,
            .Quality = 0,
        },
        .BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT,
        .BufferCount = 2,
        .Scaling = DXGI_SCALING_STRETCH,
        .SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD,
    };
    IDXGIFactory2_CreateSwapChainForHwnd(dxgi_factory,
                                         (IUnknown *)w32_g_app.device,
                                         window->window_handle,
                                         &swap_chain_desc,
                                         0, 0, &window->swap_chain);
    IDXGIFactory2_Release(dxgi_factory);
    
    //-NOTE(tbt): show window
    ShowWindow(window->window_handle, SW_SHOW);
    UpdateWindow(window->window_handle);
    
    //-NOTE(tbt): call into user code
    hooks.open(result);
    window->is_initialised = True;
    
    //-NOTE(tbt): append to app windows list
    if(0 == w32_g_app.windows_last)
    {
        Assert(0 == w32_g_app.windows_first);
        w32_g_app.windows_first = window;
        w32_g_app.windows_last = window;
    }
    else
    {
        window->prev = w32_g_app.windows_last;
        w32_g_app.windows_last->next = window;
        w32_g_app.windows_last = window;
    }
    
    ArenaTempEnd(scratch);
    
    return result;
}

Function void
G_WindowClose(OpaqueHandle window)
{
    OS_AssertMainThread();
    
    W32_Window *w = W32_WindowFromHandle(window);
    w->should_close = True;
}

Function void
G_MainLoop(void)
{
    while(0 != w32_g_app.windows_first)
    {
        ArenaClear(w32_g_app.frame_arena);
        
        // NOTE(tbt): frame timing
        U64 now = OS_TimeInMicroseconds();
        w32_g_app.frame_time = now - w32_g_app.frame_start;
        w32_g_app.frame_start = now;
        
        // NOTE(tbt): block to wait for events if required
        if(w32_g_app.should_wait_for_events && (now > w32_g_app.should_not_wait_for_events_until))
        {
            WaitMessage();
        }
        
        // NOTE(tbt): update each window and call into user code
        for(W32_Window *w = w32_g_app.windows_first; 0 != w; w = w->next)
        {
            G_EventQueueClear(&w->events);
            
            MSG message;
            while(!w->should_close && PeekMessageW(&message, w->window_handle, 0, 0, PM_REMOVE))
            {
                TranslateMessage(&message);
                DispatchMessage(&message);
            }
            
            if(0 != w->render_target_view &&
               0 != w->depth_stencil_view)
            {
                ID3D11DeviceContext_ClearRenderTargetView(w32_g_app.device_ctx, w->render_target_view, w->clear_colour.elements);
                ID3D11DeviceContext_ClearDepthStencilView(w32_g_app.device_ctx, w->depth_stencil_view, D3D11_CLEAR_DEPTH | D3D11_CLEAR_STENCIL, 1.0f, 0);
            }
            
            w->hooks.draw(W32_HandleFromWindow(w));
        }
        
        // NOTE(tbt): cleanup windows which need to close
        {
            W32_Window *next = 0;
            for(W32_Window *w = w32_g_app.windows_first; 0 != w; w = next)
            {
                next = w->next;
                if(w->should_close)
                {
                    w->hooks.close(W32_HandleFromWindow(w));
                    W32_WindowRelease(w);
                    
                    // NOTE(tbt): go back to start of loop as the cleanup callback of the window could have closed another window
                    next = w32_g_app.windows_first;
                }
            }
        }
        
        // NOTE(tbt): present swapchains
        for(W32_Window *w = w32_g_app.windows_first; 0 != w; w = w->next)
        {
            W32_WindowPresent(w, w->is_vsync);
        }
    }
}

Function G_EventQueue *
G_WindowEventQueueGet(OpaqueHandle window)
{
    W32_Window *w = W32_WindowFromHandle(window);
    G_EventQueue *result = &w->events;
    return result;
}

Function void *
G_WindowUserDataGet(OpaqueHandle window)
{
    W32_Window *w = W32_WindowFromHandle(window);
    void *result = w->user_data;
    return result;
}

Function B32
G_WindowIsFullScreen(OpaqueHandle window)
{
    W32_Window *w = W32_WindowFromHandle(window);
    DWORD window_style = GetWindowLong(w->window_handle, GWL_STYLE);
    B32 result = !(window_style & WS_OVERLAPPEDWINDOW);
    return result;
}

Function F32
G_WindowScaleFactorGet(OpaqueHandle window)
{
    W32_Window *w = W32_WindowFromHandle(window);
    F32 result = w->scale_factor;
    return result;
}


Function V2I
G_WindowDimensionsGet(OpaqueHandle window)
{
    W32_Window *w = W32_WindowFromHandle(window);
    
    V2I result;
    RECT client_rect;
    GetClientRect(w->window_handle, &client_rect);
    result.x = client_rect.right - client_rect.left;
    result.y = client_rect.bottom - client_rect.top;
    return result;
}

Function Range2F
G_WindowClientRectGet(OpaqueHandle window)
{
    W32_Window *w = W32_WindowFromHandle(window);
    
    Range2F result = {0};
    RECT client_rect;
    GetClientRect(w->window_handle, &client_rect);
    result.max.x = (client_rect.right - client_rect.left);
    result.max.y = (client_rect.bottom - client_rect.top);
    return result;
}

Function V2F
G_WindowMousePositionGet(OpaqueHandle window)
{
    W32_Window *w = W32_WindowFromHandle(window);
    
    V2F result = {0};
    POINT mouse;
    GetCursorPos(&mouse);
    ScreenToClient(w->window_handle, &mouse);
    result.x = mouse.x;
    result.y = mouse.y;
    return result;
}

Function B32
G_WindowKeyStateGet(OpaqueHandle window, G_Key key)
{
    W32_Window *w = W32_WindowFromHandle(window);
    B32 result = !!(w->is_key_down[key >> 6] & Bit(key & 0x3F));
    return result;
}

Function G_ModifierKeys
G_WindowModifiersMaskGet(OpaqueHandle window)
{
    G_ModifierKeys result =
    (G_ModifierKeys_Ctrl *G_WindowKeyStateGet(window, G_Key_Ctrl) |
     G_ModifierKeys_Shift*G_WindowKeyStateGet(window, G_Key_Shift) |
     G_ModifierKeys_Alt  *G_WindowKeyStateGet(window, G_Key_Alt) |
     G_ModifierKeys_Super*G_WindowKeyStateGet(window, G_Key_Super));
    return result;
}

Function Arena *
G_WindowArenaGet(OpaqueHandle window)
{
    W32_Window *w = W32_WindowFromHandle(window);
    Arena *result = w->arena;
    return result;
}

Function U64
G_WindowFrameTimeGet(OpaqueHandle window)
{
    U64 result = w32_g_app.frame_time;
    return result;
}

Function Arena *
G_WindowFrameArenaGet(OpaqueHandle window)
{
    Arena *result = w32_g_app.frame_arena;
    return result;
}

Function void
G_WindowFullscreenSet(OpaqueHandle window, B32 is_fullscreen)
{
    W32_Window *w = W32_WindowFromHandle(window);
    
    DWORD window_style = GetWindowLong(w->window_handle, GWL_STYLE);
    
    if(is_fullscreen)
    {
        MONITORINFO monitor_info = { sizeof(monitor_info) };
        if(GetWindowPlacement(w->window_handle, &w->prev_window_placement) &&
           GetMonitorInfo(MonitorFromWindow(w->window_handle, MONITOR_DEFAULTTOPRIMARY), &monitor_info))
        {
            SetWindowLong(w->window_handle, GWL_STYLE,
                          window_style & ~WS_OVERLAPPEDWINDOW);
            
            SetWindowPos(w->window_handle, HWND_TOP,
                         monitor_info.rcMonitor.left,
                         monitor_info.rcMonitor.top,
                         monitor_info.rcMonitor.right -
                         monitor_info.rcMonitor.left,
                         monitor_info.rcMonitor.bottom -
                         monitor_info.rcMonitor.top,
                         SWP_NOOWNERZORDER | SWP_FRAMECHANGED);
        }
    }
    else
    {
        SetWindowLong(w->window_handle, GWL_STYLE,
                      window_style | WS_OVERLAPPEDWINDOW);
        SetWindowPlacement(w->window_handle, &w->prev_window_placement);
        SetWindowPos(w->window_handle, 0, 0, 0, 0, 0,
                     SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER |
                     SWP_NOOWNERZORDER | SWP_FRAMECHANGED);
    }
}

Function void
G_WindowVSyncSet(OpaqueHandle window, B32 is_vsync)
{
    W32_Window *w = W32_WindowFromHandle(window);
    w->is_vsync = is_vsync;
}

Function void
G_WindowCursorKindSet(OpaqueHandle window, G_CursorKind kind)
{
    W32_Window *w = W32_WindowFromHandle(window);
    if(kind != OS_InterlockedExchange1I(&w->cursor_kind, kind))
    {
        SendMessageW(w->window_handle, WM_SETCURSOR, (WPARAM)w->window_handle, 0);
    }
}

Function void
G_WindowMousePositionSet(OpaqueHandle window, V2F position)
{
    W32_Window *w = W32_WindowFromHandle(window);
    
    POINT pt = { position.x, position.y, };
    ClientToScreen(w->window_handle, &pt);
    SetCursorPos(pt.x, pt.y);
}

Function void
G_WindowUserDataSet(OpaqueHandle window, void *data)
{
    W32_Window *w = W32_WindowFromHandle(window);
    w->user_data = data;
}

Function B32
G_IsDarkMode(void)
{
    return w32_is_dark_mode;
}

Function void
G_ShouldWaitForEvents(B32 should_wait_for_events)
{
    OS_InterlockedExchange1I(&w32_g_app.should_wait_for_events, should_wait_for_events);
}

Function void
G_DoNotWaitForEventsUntil(U64 until)
{
    if(w32_g_app.should_wait_for_events)
    {
        OS_InterlockedExchange1U64(&w32_g_app.should_not_wait_for_events_until, until);
        
        for(W32_Window *w = w32_g_app.windows_first; 0 != w; w = w->next)
        {
            SendMessage(w->window_handle, WM_NULL, 0, 0);
        }
    }
}

Function void
G_WindowClearColourSet(OpaqueHandle window, V4F clear_colour)
{
    W32_Window *w = W32_WindowFromHandle(window);
    w->clear_colour = clear_colour;
}

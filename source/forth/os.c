
#define WIN32_LEAN_AND_MEAN
#define UNICODE
#define _UNICODE
#include <windows.h>
#include <windowsx.h>

#include <stdint.h>

enum {
    OS_CharQueueCap = 4096,
    
    OS_WindowSizeCap = 3840,
    OS_DefaultWindowWidth = 1024,
    OS_DefaultWindowHeight = 768,
};

enum {
    OS_Scale = 4,
};

HWND OS_WindowHandle = 0;
uint64_t OS_WindowShouldClose = 0;

HDC OS_DeviceContext = 0;

BITMAPINFO OS_BitmapInfo =
{
    .bmiHeader =
    {
        .biSize = sizeof(BITMAPINFOHEADER),
        .biWidth = OS_DefaultWindowWidth,
        .biHeight = OS_DefaultWindowHeight,
        .biPlanes = 1,
        .biBitCount = 32,
        .biCompression = BI_RGB,
    },
};

uint64_t OS_CharQueue[OS_CharQueueCap] = {0};
uint64_t OS_CharQueueRead = 0;
uint64_t OS_CharQueueWrite = 0;

uint32_t OS_Framebuffer[OS_WindowSizeCap*OS_WindowSizeCap] = {0};

LRESULT OS_WindowProc(HWND window_handle, UINT message, WPARAM w_param, LPARAM l_param) {
    LRESULT result = 0;
    switch(message) {
        default: {
            result = DefWindowProc(window_handle, message, w_param, l_param);
        } break;
        
        case WM_CHAR: {
            // TODO(tbt): doesn't handle overflow, probably not a problem with u64 index
            if(OS_CharQueueWrite < OS_CharQueueCap + OS_CharQueueRead) {
                uint64_t char_input = w_param;
                if(char_input == VK_RETURN ||
                   char_input == '\r') {
                    char_input = '\n';
                }
                OS_CharQueue[OS_CharQueueWrite % OS_CharQueueCap] = char_input; // TODO(tbt): handle surrogate pairs and convert to utf-32
                OS_CharQueueWrite += 1;
            }
        } break;
        
        case WM_SIZE: {
            int16_t width = LOWORD(l_param);
            if(width > OS_WindowSizeCap) {
                width = OS_WindowSizeCap;
            }
            int16_t height = HIWORD(l_param);
            if(height > OS_WindowSizeCap) {
                height = OS_WindowSizeCap;
            }
            OS_BitmapInfo.bmiHeader.biWidth = width / OS_Scale;
            OS_BitmapInfo.bmiHeader.biHeight = height / OS_Scale;
        } break;
        
        case WM_CLOSE:
        case WM_QUIT:
        case WM_DESTROY: {
            OS_WindowShouldClose = 1;
            result = DefWindowProc(window_handle, message, w_param, l_param);
        } break;
    }
    
    return result;
}

void OS_Init(void) {
    HINSTANCE instance_handle = GetModuleHandle(0);
    WNDCLASSW window_class = 
    {
        .style = CS_HREDRAW | CS_VREDRAW | CS_DBLCLKS,
        .lpfnWndProc = OS_WindowProc,
        .lpszClassName = L"forth",
        .hCursor = LoadCursorW(0, IDC_ARROW),
    };
    RegisterClassW(&window_class);
    SetThreadDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);
    DWORD window_styles_ex = 0;
    DWORD window_styles = WS_OVERLAPPEDWINDOW | WS_VISIBLE;
    RECT window_rect = { 0, 0, OS_DefaultWindowWidth, OS_DefaultWindowHeight, };
    AdjustWindowRect(&window_rect, window_styles, FALSE);
    OS_WindowHandle = CreateWindowExW(window_styles_ex,
                                      L"forth",
                                      L"Hello, world!",
                                      window_styles,
                                      CW_USEDEFAULT, CW_USEDEFAULT,
                                      window_rect.right - window_rect.left,
                                      window_rect.bottom - window_rect.top,
                                      0, 0, instance_handle, 0);
    OS_DeviceContext = GetDC(OS_WindowHandle);
}

uint64_t OS_UpdateEvents(void) {
    MSG message;
    while(!OS_WindowShouldClose && PeekMessageW(&message, OS_WindowHandle, 0, 0, PM_REMOVE)) {
        TranslateMessage(&message);
        DispatchMessage(&message);
    }
    return OS_WindowShouldClose;
}

void OS_Render(void) {
    StretchDIBits(OS_DeviceContext,
                  0, 0, OS_BitmapInfo.bmiHeader.biWidth*OS_Scale, OS_BitmapInfo.bmiHeader.biHeight*OS_Scale,
                  0, 0, OS_BitmapInfo.bmiHeader.biWidth, OS_BitmapInfo.bmiHeader.biHeight,
                  OS_Framebuffer, &OS_BitmapInfo,
                  DIB_RGB_COLORS, SRCCOPY);
    DwmFlush();
    memset(OS_Framebuffer, 0, OS_BitmapInfo.bmiHeader.biWidth*OS_BitmapInfo.bmiHeader.biHeight*4);
}


#import <Cocoa/Cocoa.h>
#import <Metal/Metal.h>
#import <QuartzCore/CAMetalLayer.h>
#import <QuartzCore/QuartzCore.h>
#import <Foundation/Foundation.h>
#include <ApplicationServices/ApplicationServices.h>

@interface MAC_AppDelegate : NSObject<NSApplicationDelegate>
- (void)applicationWillFinishLaunching:(NSNotification *)notification;
- (void)applicationDidFinishLaunching:(NSNotification *)notification;
- (BOOL)applicationShouldTerminateAfterLastWindowClosed:(NSApplication*)application;
@end

typedef struct MAC_Window MAC_Window;

@interface MAC_MetalView : NSView
@property(assign) CAMetalLayer *metal_layer;
@property(assign) MAC_Window *my_window;
@property(assign) NSTrackingArea *whole_window_tracking_area;
- (CALayer*)makeBackingLayer;
- (instancetype)initWithFrame:(CGRect)frame_rect;
- (void)mouseEntered:(NSEvent *)e;
- (void)mouseExited:(NSEvent *)e;
@end

@interface MAC_NSWindow : NSWindow
@property(assign) MAC_Window *my_window;
- (BOOL)canBecomeMainWindow;
- (BOOL)canBecomeKeyWindow;
- (BOOL)acceptsFirstResponder;
- (void)keyDown:(NSEvent *)e;
- (void)keyUp:(NSEvent *)e;
- (void)flagsChanged:(NSEvent *)e;
- (void)mouseDown:(NSEvent *)e;
- (void)mouseUp:(NSEvent *)e;
- (void)rightMouseDown:(NSEvent *)e;
- (void)rightMouseUp:(NSEvent *)e;
- (void)scrollWheel:(NSEvent *)e;
- (void)mouseMoved:(NSEvent *)e;
@end

@interface MAC_NSWindowDelegate : NSObject<NSWindowDelegate>
@property(assign) MAC_Window *my_window;
- (void)windowWillClose:(NSNotification *)notification;
- (void)windowDidResize:(NSNotification *)notification;
@end

struct MAC_Window
{
    Arena arena;
    MAC_Window *next;
    MAC_NSWindow *window;
    MAC_MetalView *metal_view;
    id<MTLRenderPipelineState> pipeline_state;
    id<MTLCommandQueue> command_queue;
    id<MTLRenderCommandEncoder> command_encoder;
    CVDisplayLinkRef display_link;
    B32 is_first_update;
    B32 is_fullscreen;
    G_WindowHooks hooks;
    void *user_data;
    V4F clear_colour;
    OS_ThreadContext tctx;
    G_CursorKind cursor_kind;
    B32 is_cursor_hidden;
    B32 is_mouse_in_content_view;
    OpaqueHandle events_lock;
    G_EventQueue events;
    G_EventQueue events_user;
    OpaqueHandle is_key_down_lock;
    U64 is_key_down[(G_Key_MAX + 64) >> 6];
    U64 is_key_down_user[(G_Key_MAX + 64) >> 6];
    G_ModifierKeys last_modifier_keys;
    Arena *volatile frame_arena;
    U64 frame_begin;
    U64 last_frame_begin;
    U64 frame_time;
};

typedef struct MAC_GraphicsApp MAC_GraphicsApp;
struct MAC_GraphicsApp
{
    id<MTLDevice> metal_device;
    MAC_Window *windows;
};

Global MAC_GraphicsApp mac_g_app = {0};

Function void
MAC_IsKeyDownSet(MAC_Window *window, G_Key key, B32 is_down)
{
    OS_SemaphoreWait(window->is_key_down_lock);
    if(is_down)
    {
        window->is_key_down[key >> 6] |= Bit(key & 0x3F);
    }
    else
    {
        window->is_key_down[key >> 6] &= ~Bit(key & 0x3F);
    }
    OS_SemaphoreSignal(window->is_key_down_lock);
}

static CVReturn
MAC_DisplayLinkCallback(CVDisplayLinkRef displayLink,
                        const CVTimeStamp *now,
                        const CVTimeStamp *outputTime,
                        CVOptionFlags flagsIn,
                        CVOptionFlags *flagsOut,
                        void *displayLinkContext)
{

    MAC_Window *w = displayLinkContext;

    w->last_frame_begin = w->frame_begin;
    w->frame_begin = OS_MicrosecondsGet();
    OS_InterlockedExchange1U64(&w->frame_time, w->frame_begin - w->last_frame_begin);
    
    if(w->is_first_update)
    {
        OS_ThreadContextMake(&w->tctx, -1);
        OS_TCtxSet(&w->tctx);

        w->command_queue = [mac_g_app.metal_device newCommandQueue];

        w->is_first_update = False;
    }


    if (0 != w->metal_view.metal_layer)
    {
        OS_SemaphoreWait(w->events_lock);
        MemoryCopy(&w->events_user, &w->events, sizeof(w->events));
        G_EventQueueClear(&w->events);
        OS_SemaphoreSignal(w->events_lock);

        OS_SemaphoreWait(w->is_key_down_lock);
        MemoryCopy(w->is_key_down_user, w->is_key_down, sizeof(w->is_key_down));
        OS_SemaphoreSignal(w->is_key_down_lock);

        id<CAMetalDrawable> drawable = [w->metal_view.metal_layer nextDrawable];

        id<MTLCommandBuffer> command_buffer = [w->command_queue commandBuffer];

        MTLRenderPassDescriptor *pass_desc = [MTLRenderPassDescriptor renderPassDescriptor];
        pass_desc.colorAttachments[0].texture = drawable.texture;
        pass_desc.colorAttachments[0].loadAction = MTLLoadActionClear;
        pass_desc.colorAttachments[0].storeAction = MTLStoreActionStore;
        pass_desc.colorAttachments[0].clearColor = MTLClearColorMake(w->clear_colour.r, w->clear_colour.g, w->clear_colour.b, w->clear_colour.a);
        
        w->command_encoder = [command_buffer renderCommandEncoderWithDescriptor:pass_desc];

        OpaqueHandle window_handle = { .p = IntFromPtr(w), };
        w->hooks.draw(window_handle);

        [w->command_encoder endEncoding];

        [command_buffer presentDrawable:drawable];

        [command_buffer commit];

        if(0 != w->frame_arena)
        {
            ArenaClear(w->frame_arena);
        }
    }

    return kCVReturnSuccess;
}

Function void
G_Init(void)
{
    mac_g_app.metal_device = MTLCreateSystemDefaultDevice();
  
    MAC_AppDelegate *app_delegate = [[MAC_AppDelegate alloc] init];
    [[NSApplication sharedApplication] setDelegate:app_delegate];
}

Function G_ModifierKeys
MAC_GModifierKeysFromNSEventModifierFlags(NSEventModifierFlags flags)
{
    G_ModifierKeys result = 0;
    if(flags & NSEventModifierFlagControl)
    {
        result |= G_ModifierKeys_Ctrl;
    }
    if(flags & NSEventModifierFlagShift)
    {
        result |= G_ModifierKeys_Shift;
    }
    if(flags & NSEventModifierFlagOption)
    {
        result |= G_ModifierKeys_Option;
    }
    if(flags & NSEventModifierFlagCommand)
    {
        result |= G_ModifierKeys_Cmd;
    }
    return result;
}

Function G_Key
MAC_GKeyFromKeyCode(unsigned short key_code)
{
    switch(key_code)
    {
        default: return G_Key_None;
        case(50):  return G_Key_GraveAccent;
        case(18):  return G_Key_1;
        case(19):  return G_Key_2;
        case(20):  return G_Key_3;
        case(21):  return G_Key_4;
        case(23):  return G_Key_5;
        case(22):  return G_Key_6;
        case(26):  return G_Key_7;
        case(28):  return G_Key_8;
        case(25):  return G_Key_9;
        case(29):  return G_Key_0;
        case(27):  return G_Key_Minus;
        case(24):  return G_Key_Equal;
        case(0):   return G_Key_A;
        case(11):  return G_Key_B;
        case(8):   return G_Key_C;
        case(2):   return G_Key_D;
        case(14):  return G_Key_E;
        case(3):   return G_Key_F;
        case(5):   return G_Key_G;
        case(4):   return G_Key_H;
        case(34):  return G_Key_I;
        case(38):  return G_Key_J;
        case(40):  return G_Key_K;
        case(37):  return G_Key_L;
        case(46):  return G_Key_M;
        case(45):  return G_Key_N;
        case(31):  return G_Key_O;
        case(35):  return G_Key_P;
        case(12):  return G_Key_Q;
        case(15):  return G_Key_R;
        case(1):   return G_Key_S;
        case(17):  return G_Key_T;
        case(32):  return G_Key_U;
        case(9):   return G_Key_V;
        case(13):  return G_Key_W;
        case(7):   return G_Key_X;
        case(16):  return G_Key_Y;
        case(6):   return G_Key_Z;
        case(49):  return G_Key_Space;
        case(47):  return G_Key_Period;
        case(44):  return G_Key_ForwardSlash;
        case(43):  return G_Key_Comma;
        case(39):  return G_Key_Quote;
        case(41):  return G_Key_Colon;
        case(33):  return G_Key_LeftBracket;
        case(30):  return G_Key_RightBracket;
        case(48):  return G_Key_Tab;
        case(122): return G_Key_F1;
        case(120): return G_Key_F2;
        case(99):  return G_Key_F3;
        case(118): return G_Key_F4;
        case(96):  return G_Key_F5;
        case(97):  return G_Key_F6;
        case(98):  return G_Key_F7;
        case(100): return G_Key_F8;
        case(101): return G_Key_F9;
        case(109): return G_Key_F10;
        case(103): return G_Key_F11;
        case(111): return G_Key_F12;
        case(51):  return G_Key_Backspace;
        case(117): return G_Key_Delete;
        case(114): return G_Key_Insert;
        case(53):  return G_Key_Esc;
        case(36):  return G_Key_Enter;
        case(126): return G_Key_Up;
        case(125): return G_Key_Down;
        case(123): return G_Key_Left;
        case(124): return G_Key_Right;
        case(116): return G_Key_PageUp;
        case(121): return G_Key_PageDown;
        case(115): return G_Key_Home;
        case(119): return G_Key_End;
        case(59):  return G_Key_Ctrl;
        case(56):  return G_Key_Shift;
        case(58):  return G_Key_Alt;
        case(55):  return G_Key_Super;
    }
}

Function OpaqueHandle
G_WindowOpen(S8 title, V2I dimensions, G_WindowHooks hooks, void *user_data)
{
    Arena *arena = ArenaMake();
    ArenaPush(arena, sizeof(MAC_Window) - sizeof(Arena));

    MAC_Window *w = (MAC_Window *)arena;

    NSString *title_ns_string = [[NSString alloc] initWithBytes:title.buffer length:title.len encoding:NSUTF8StringEncoding];

    NSRect content_rect = NSMakeRect(0.0f, 0.0f, dimensions.x, dimensions.y);
    w->metal_view = [[MAC_MetalView alloc] initWithFrame:content_rect];
    w->metal_view.my_window = w;
    
    MAC_NSWindowDelegate *delegate = [[MAC_NSWindowDelegate alloc] init];
    delegate.my_window = w;

    const int style = (NSWindowStyleMaskTitled |
                        NSWindowStyleMaskClosable |
                        NSWindowStyleMaskMiniaturizable |
                        NSWindowStyleMaskResizable);
    w->window = [[MAC_NSWindow alloc] initWithContentRect:content_rect
                                                styleMask:style
                                                  backing:NSBackingStoreBuffered
                                                    defer:YES];
    w->window.acceptsMouseMovedEvents = YES;
    w->window.my_window = w;
    w->is_first_update = True;
    w->user_data = user_data;
    w->hooks = hooks;
    w->events_lock = OS_SemaphoreMake(1);
    w->is_key_down_lock = OS_SemaphoreMake(1);
    [w->window setDelegate:delegate];
    [w->window setTitle:title_ns_string];
    [w->window setOpaque:YES];
    [w->window setContentView:w->metal_view];
    [w->window makeMainWindow];
    [w->window makeKeyAndOrderFront:nil];
    [w->window makeFirstResponder:nil];

    NSRect scaled_content_rect = [w->window convertRectToBacking:content_rect];
    CGSize scaled_content_size =
    {
        .width = NSWidth(scaled_content_rect),
        .height = NSHeight(scaled_content_rect),
    };
    [w->metal_view.metal_layer setDrawableSize:scaled_content_size];

    [title_ns_string release];

    //[w->window makeKeyAndOrderFront:self];

    CVDisplayLinkCreateWithActiveCGDisplays(&w->display_link);
    CVDisplayLinkSetOutputCallback(w->display_link, &MAC_DisplayLinkCallback, w);
    CVDisplayLinkSetCurrentCGDisplay(w->display_link, 0);
    CVDisplayLinkStart(w->display_link);

    w->next = mac_g_app.windows;
    mac_g_app.windows = w;

    OpaqueHandle result = { .p = IntFromPtr(w), };
    w->hooks.open(result);

    return result;
}

Function void
G_WindowClose(OpaqueHandle window)
{
    MAC_Window *w = PtrFromInt(window.p);
    [w->window close];
}

Function void
G_MainLoop(void)
{
    [[NSApplication sharedApplication] run];
}

Function void
G_Cleanup(void)
{
    return;
}

Function void
MAC_WindowUpdateCursorKind(MAC_Window *w)
{
    B32 should_hide = (G_CursorKind_Hidden == w->cursor_kind);

    if(should_hide)
    {
        if(!w->is_cursor_hidden)
        {
            [NSCursor hide];
            w->is_cursor_hidden = True;
        }
    }
    else
    {
        if(w->is_cursor_hidden)
        {
            [NSCursor unhide];
            w->is_cursor_hidden = False;
        }

        switch(w->cursor_kind)
        {
            default: break;

            case(G_CursorKind_Default):
            {
                [[NSCursor arrowCursor] set];
            } break;

            case(G_CursorKind_HResize):
            {
                [[NSCursor resizeLeftRightCursor] set];
            } break;

            case(G_CursorKind_VResize):
            {
                [[NSCursor resizeUpDownCursor] set];
            } break;

            case(G_CursorKind_Loading):
            {
                // TODO(tbt): AFAICT there is no way to set this yourself - is set automatically if the main thread is blocked long enough
            } break;

            case(G_CursorKind_No):
            {
                [[NSCursor operationNotAllowedCursor] set];
            } break;

            case(G_CursorKind_IBeam):
            {
                [[NSCursor IBeamCursor] set];
            } break;
        }
    }
}

@implementation MAC_NSWindow
- (BOOL)canBecomeMainWindow { return YES; }
- (BOOL)canBecomeKeyWindow { return YES; }
- (BOOL)acceptsFirstResponder { return YES; }

- (void)keyDown:(NSEvent *)e
{
    MAC_Window *w = self.my_window;

    unsigned short key_code = [e keyCode];
    G_Key key = MAC_GKeyFromKeyCode(key_code);
    
    NSEventModifierFlags flags = [e modifierFlags];
    G_ModifierKeys modifiers = MAC_GModifierKeysFromNSEventModifierFlags(flags);

    MAC_IsKeyDownSet(w, key, True);

    G_Event my_event =
    {
        .kind = G_EventKind_Key,
        .modifiers = modifiers,
        .key = key,
        .is_down = True,
    };
    OS_SemaphoreWait(w->events_lock);
    G_EventQueuePush(&w->events, my_event);
    OS_SemaphoreSignal(w->events_lock);

    ArenaTemp scratch = OS_TCtxScratchGet(0, 0);
    NSString *text = [e characters];
    NSUInteger len = [text length];
    unichar *buffer = ArenaPush(scratch.arena, len*sizeof(unichar));
    [text getCharacters:buffer range:NSMakeRange(0, len)];
    for(U64 codepoint_index = 0; codepoint_index < len; codepoint_index += 1)
    {
        if(isprint(buffer[codepoint_index]))
        {
            G_Event my_event =
            {
                .kind = G_EventKind_Char,
                .modifiers = modifiers,
                .codepoint = buffer[codepoint_index],
            };
            OS_SemaphoreWait(w->events_lock);
            G_EventQueuePush(&w->events, my_event);
            OS_SemaphoreSignal(w->events_lock);
        }
    }
    ArenaTempEnd(scratch);
}

- (void)keyUp:(NSEvent *)e
{
    MAC_Window *w = self.my_window;

    unsigned short key_code = [e keyCode];
    G_Key key = MAC_GKeyFromKeyCode(key_code);
    
    NSEventModifierFlags flags = [e modifierFlags];
    G_ModifierKeys modifiers = MAC_GModifierKeysFromNSEventModifierFlags(flags);

    MAC_IsKeyDownSet(w, key, False);

    G_Event my_event =
    {
        .kind = G_EventKind_Key,
        .modifiers = modifiers,
        .key = key,
        .is_down = False,
    };
    OS_SemaphoreWait(w->events_lock);
    G_EventQueuePush(&w->events, my_event);
    OS_SemaphoreSignal(w->events_lock);
}

- (void)flagsChanged:(NSEvent *)e
{
    MAC_Window *w = self.my_window;

    unsigned short key_code = [e keyCode];
    G_Key key = MAC_GKeyFromKeyCode(key_code);

    NSEventModifierFlags flags = [e modifierFlags];
    G_ModifierKeys modifiers = MAC_GModifierKeysFromNSEventModifierFlags(flags);

    G_ModifierKeys mod_key = G_ModifierFromKey(key);
    B32 is_down = !(w->last_modifier_keys & mod_key);
    w->last_modifier_keys = modifiers;

    MAC_IsKeyDownSet(w, key, is_down);

    G_Event my_event =
    {
        .kind = G_EventKind_Key,
        .modifiers = (modifiers & ~mod_key),
        .key = key,
        .is_down = is_down,
    };
    OS_SemaphoreWait(w->events_lock);
    G_EventQueuePush(&w->events, my_event);
    OS_SemaphoreSignal(w->events_lock);
}

- (void)mouseDown:(NSEvent *)e
{
    MAC_Window *w = self.my_window;

    if(w->is_mouse_in_content_view)
    {
        NSEventModifierFlags flags = [e modifierFlags];
        G_ModifierKeys modifiers = MAC_GModifierKeysFromNSEventModifierFlags(flags);

        MAC_IsKeyDownSet(w, G_Key_MouseButtonLeft, True);

        NSPoint location = [w->window convertPointToBacking:[e locationInWindow]];
        F32 window_height = NSHeight([w->metal_view bounds]);

        G_Event my_event =
        {
            .kind = G_EventKind_Key,
            .modifiers = modifiers,
            .key = G_Key_MouseButtonLeft,
            .is_down = True,
            .position = V2F(location.x, window_height - location.y),
        };
        OS_SemaphoreWait(w->events_lock);
        G_EventQueuePush(&w->events, my_event);
        OS_SemaphoreSignal(w->events_lock);
    }
}

- (void)mouseUp:(NSEvent *)e
{
    MAC_Window *w = self.my_window;

    if(w->is_mouse_in_content_view)
    {
        NSEventModifierFlags flags = [e modifierFlags];
        G_ModifierKeys modifiers = MAC_GModifierKeysFromNSEventModifierFlags(flags);

        MAC_IsKeyDownSet(w, G_Key_MouseButtonLeft, False);

        NSPoint location = [e locationInWindow];
        F32 window_height = NSHeight([w->metal_view bounds]);

        G_Key key = G_Key_MouseButtonLeft;
        if(2 == [e clickCount])
        {
            key = G_Key_MouseButtonDoubleClick;
        }

        G_Event my_event =
        {
            .kind = G_EventKind_Key,
            .modifiers = modifiers,
            .key = key,
            .is_down = False,
            .position = V2F(location.x, window_height - location.y),
        };
        OS_SemaphoreWait(w->events_lock);
        G_EventQueuePush(&w->events, my_event);
        OS_SemaphoreSignal(w->events_lock);
    }
}

- (void)rightMouseDown:(NSEvent *)e
{
    MAC_Window *w = self.my_window;

    if(w->is_mouse_in_content_view)
    {
        NSEventModifierFlags flags = [e modifierFlags];
        G_ModifierKeys modifiers = MAC_GModifierKeysFromNSEventModifierFlags(flags);

        MAC_IsKeyDownSet(w, G_Key_MouseButtonRight, True);

        NSPoint location = [w->window convertPointToBacking:[e locationInWindow]];
        F32 window_height = NSHeight([w->metal_view bounds]);

        G_Event my_event =
        {
            .kind = G_EventKind_Key,
            .modifiers = modifiers,
            .key = G_Key_MouseButtonRight,
            .is_down = True,
            .position = V2F(location.x, window_height - location.y),
        };
        OS_SemaphoreWait(w->events_lock);
        G_EventQueuePush(&w->events, my_event);
        OS_SemaphoreSignal(w->events_lock);
    }
}

- (void)rightMouseUp:(NSEvent *)e
{
    MAC_Window *w = self.my_window;

    if(w->is_mouse_in_content_view)
    {
        NSEventModifierFlags flags = [e modifierFlags];
        G_ModifierKeys modifiers = MAC_GModifierKeysFromNSEventModifierFlags(flags);

        MAC_IsKeyDownSet(w, G_Key_MouseButtonRight, False);

        NSPoint location = [w->window convertPointToBacking:[e locationInWindow]];
        F32 window_height = NSHeight([w->metal_view bounds]);
        
        G_Event my_event =
        {
            .kind = G_EventKind_Key,
            .modifiers = modifiers,
            .key = G_Key_MouseButtonRight,
            .is_down = False,
            .position = V2F(location.x, window_height - location.y),
        };
        OS_SemaphoreWait(w->events_lock);
        G_EventQueuePush(&w->events, my_event);
        OS_SemaphoreSignal(w->events_lock);
    }
}

- (void)scrollWheel:(NSEvent *)e
{
    MAC_Window *w = self.my_window;

    NSEventModifierFlags flags = [e modifierFlags];
    G_ModifierKeys modifiers = MAC_GModifierKeysFromNSEventModifierFlags(flags);

    // TODO(tbt): deltas probably get too big with flicks on the track pad
    //            clamp? some non-linear function to flatten it out a bit?
    G_Event my_event =
    {
        .kind = G_EventKind_MouseScroll,
        .modifiers = modifiers,
        .delta = V2F([e scrollingDeltaX]*0.5f, [e scrollingDeltaY]*0.5f),
    };
    OS_SemaphoreWait(w->events_lock);
    G_EventQueuePush(&w->events, my_event);
    OS_SemaphoreSignal(w->events_lock);
}

- (void)mouseMoved:(NSEvent *)e
{
    MAC_Window *w = self.my_window;

    NSEventModifierFlags flags = [e modifierFlags];
    G_ModifierKeys modifiers = MAC_GModifierKeysFromNSEventModifierFlags(flags);

    NSPoint location = [w->window convertPointToBacking:[e locationInWindow]];
    F32 window_height = NSHeight([w->metal_view bounds]);

    G_Event my_event =
    {
        .kind = G_EventKind_MouseMove,
        .modifiers = modifiers,
        .position = V2F(location.x, window_height - location.y),
    };
    OS_SemaphoreWait(w->events_lock);
    G_EventQueuePush(&w->events, my_event);
    OS_SemaphoreSignal(w->events_lock);
}

- (void)mouseDragged:(NSEvent *)e
{
    MAC_Window *w = self.my_window;

    NSEventModifierFlags flags = [e modifierFlags];
    G_ModifierKeys modifiers = MAC_GModifierKeysFromNSEventModifierFlags(flags);

    NSPoint location = [w->window convertPointToBacking:[e locationInWindow]];
    F32 window_height = NSHeight([w->metal_view bounds]);

    G_Event my_event =
    {
        .kind = G_EventKind_MouseMove,
        .modifiers = modifiers,
        .position = V2F(location.x, window_height - location.y),
    };
    OS_SemaphoreWait(w->events_lock);
    G_EventQueuePush(&w->events, my_event);
    OS_SemaphoreSignal(w->events_lock);
}

@end

@implementation MAC_NSWindowDelegate
- (void)windowWillClose:(NSNotification *)notification
{
    CVDisplayLinkStop(self.my_window->display_link);
    OpaqueHandle window_handle = { .p = IntFromPtr(self.my_window), };
    self.my_window->hooks.close(window_handle);
    ArenaDestroy(&self.my_window->arena);
}

- (void)windowDidResize:(NSNotification *)notification;
{
    MAC_Window *w = self.my_window;

    NSRect content_view_bounds = [w->window convertRectToBacking:[w->metal_view bounds]];
    CGSize content_view_size =
    {
        .width = NSWidth(content_view_bounds),
        .height = NSHeight(content_view_bounds),
    };

    [w->metal_view.metal_layer setDrawableSize:content_view_size];

    OpaqueHandle window_handle = { .p = IntFromPtr(w), };
    V2I window_dimensions = G_WindowDimensionsGet(window_handle);
    G_Event my_event =
    {
        .kind = G_EventKind_WindowSize,
        .size = V2I(content_view_size.width, content_view_size.height),
    };
    OS_SemaphoreWait(w->events_lock);
    G_EventQueuePush(&w->events, my_event);
    OS_SemaphoreSignal(w->events_lock);
}
@end

@implementation MAC_AppDelegate

-(void)applicationWillFinishLaunching:(NSNotification *)notification
{
    NSMenu *bar = [[NSMenu alloc] init];
    NSMenuItem *barItem = [[NSMenuItem alloc] init];
    NSMenu* menu = [[NSMenu alloc] init];
    NSMenuItem* quit = [[NSMenuItem alloc]
                          initWithTitle:[NSString stringWithFormat:@"Quit %s", Stringify(ApplicationName)]
                          action:@selector(terminate:)
                          keyEquivalent:@"q"];
    [bar addItem:barItem];
    [barItem setSubmenu:menu];
    [menu addItem:quit];
    
    NSApplication *app = [NSApplication sharedApplication];
    [app setMainMenu:bar];
    [app setActivationPolicy:NSApplicationActivationPolicyRegular];
}

-(void)applicationDidFinishLaunching:(NSNotification *)notification
{
    NSApplication *app = [NSApplication sharedApplication];
    [app activateIgnoringOtherApps:YES];
}

- (BOOL)applicationShouldTerminateAfterLastWindowClosed:(NSApplication*)application
{
    return YES;
}
@end

@implementation MAC_MetalView
+ (id)layerClass
{
    return [CAMetalLayer class];
}

- (CALayer*)makeBackingLayer
{
    CAMetalLayer* backing_layer = [CAMetalLayer layer];
    self.metal_layer = backing_layer;
    return self.metal_layer;
}

- (instancetype)initWithFrame:(CGRect)frame_rect
{
    if((self = [super initWithFrame:frame_rect]))
    {
        self.wantsLayer = YES;
        self.metal_layer.device = mac_g_app.metal_device;
        self.metal_layer.pixelFormat = MTLPixelFormatBGRA8Unorm;
    }
    return self;
}

- (void) viewWillMoveToWindow:(NSWindow *)newWindow
{
    self.whole_window_tracking_area = [[NSTrackingArea alloc] initWithRect:[self bounds] options: (NSTrackingMouseEnteredAndExited | NSTrackingActiveAlways) owner:self userInfo:nil];
    [self addTrackingArea:self.whole_window_tracking_area];
}

- (void)mouseEntered:(NSEvent *)e
{
   [super mouseEntered:e];

    MAC_Window *w = self.my_window;
    MAC_WindowUpdateCursorKind(w);

    w->is_mouse_in_content_view = True;
}

- (void)mouseExited:(NSEvent *)e
{
    [super mouseExited:e];
    
    MAC_Window *w = self.my_window;
    
    w->is_mouse_in_content_view = False;
    
    [[NSCursor arrowCursor] set];

    NSEventModifierFlags flags = [e modifierFlags];
    G_ModifierKeys modifiers = MAC_GModifierKeysFromNSEventModifierFlags(flags);

    OS_SemaphoreWait(w->is_key_down_lock);
    MemorySet(w->is_key_down, 0, sizeof(w->is_key_down));
    OS_SemaphoreSignal(w->is_key_down_lock);

    G_Event my_event =
    {
        .kind = G_EventKind_MouseLeave,
        .modifiers = modifiers,
    };
    OS_SemaphoreWait(w->events_lock);
    G_EventQueuePush(&w->events, my_event);
    OS_SemaphoreSignal(w->events_lock);
}

- (void)updateTrackingAreas
{
    [super updateTrackingAreas];
    [self removeTrackingArea:self.whole_window_tracking_area];
    [self.whole_window_tracking_area release];
    self.whole_window_tracking_area = [[NSTrackingArea alloc] initWithRect:[self bounds] options: (NSTrackingMouseEnteredAndExited | NSTrackingActiveAlways) owner:self userInfo:nil];
    [self addTrackingArea:self.whole_window_tracking_area];
}

@end

Function void
G_ShouldWaitForEvents(B32 should_wait_for_events)
{
    // TODO(tbt): right now never waits for events
}

Function void
G_DoNotWaitForEventsUntil(U64 until)
{
    // TODO(tbt): right now never waits for events
    return;
}

Function void
G_WindowClearColourSet(OpaqueHandle window, V4F clear_colour)
{
    MAC_Window *w = PtrFromInt(window.p);
    w->clear_colour = clear_colour;
}

Function void
G_WindowFullscreenSet(OpaqueHandle window, B32 is_fullscreen)
{
    MAC_Window *w = PtrFromInt(window.p);
    if(!!is_fullscreen != w->is_fullscreen)
    {
        [w->window toggleFullScreen:0];
    }
    OS_InterlockedExchange1I(&w->is_fullscreen, !!is_fullscreen);
}

Function void
G_WindowVSyncSet(OpaqueHandle window, B32 is_vsync)
{
    // NOTE(tbt): always vsync
    return;
}

Function void
G_WindowCursorKindSet(OpaqueHandle window, G_CursorKind cursor_kind)
{
    MAC_Window *w = PtrFromInt(window.p);
    if(w->cursor_kind != cursor_kind)
    {
        OS_InterlockedExchange1I(&w->cursor_kind, cursor_kind);
        MAC_WindowUpdateCursorKind(w);
    }
}

Function void
G_WindowMousePositionSet(OpaqueHandle window, V2F position)
{
    // TODO(tbt): figure out the different coordinate systems and make this work
    /*
    MAC_Window *w = PtrFromInt(window.p);

    NSPoint window_space = [w->window convertPointFromBacking:NSMakePoint(position.x, position.y)];
    NSPoint screen_space = [w->window convertPointToScreen:window_space];

    // NOTE(tbt): requires an accesibility controls permission - not ideal
    CGEventSourceRef source = CGEventSourceCreate(kCGEventSourceStateCombinedSessionState);
    CGEventRef mouse = CGEventCreateMouseEvent (0, kCGEventMouseMoved, screen_space, 0);
    CGEventPost(kCGHIDEventTap, mouse);
    CFRelease(mouse);
    CFRelease(source);
    */
}

Function void
G_WindowUserDataSet(OpaqueHandle window, void *data)
{
    MAC_Window *w = PtrFromInt(window.p);
    OS_InterlockedExchangePtr(&w->user_data, data);
}

Function B32
G_WindowIsFullscreen(OpaqueHandle window)
{
    MAC_Window *w = PtrFromInt(window.p);
    B32 result = w->is_fullscreen;
    return result;
}

Function U64
G_WindowFrameTimeGet(OpaqueHandle window)
{
    MAC_Window *w = PtrFromInt(window.p);
    U64 result = w->frame_time;
    return result;
}

Function Arena *
G_WindowFrameArenaGet(OpaqueHandle window)
{
    MAC_Window *w = PtrFromInt(window.p);
    if(0 == w->frame_arena)
    {
        Arena *frame_arena = ArenaMake();
        if(0 != OS_InterlockedCompareExchangePtr((void *volatile *)&w->frame_arena, frame_arena, 0))
        {
            ArenaDestroy(frame_arena);
        }
    }
    Arena *result = w->frame_arena;
    return result;
}

Function Arena *
G_WindowArenaGet(OpaqueHandle window)
{
    MAC_Window *w = PtrFromInt(window.p);
    Arena *result = &w->arena;
    return result;
}

Function G_EventQueue *
G_WindowEventQueueGet(OpaqueHandle window)
{
    MAC_Window *w = PtrFromInt(window.p);
    G_EventQueue *result = &w->events_user;
    return result;
}

Function void *
G_WindowUserDataGet(OpaqueHandle window)
{
    MAC_Window *w = PtrFromInt(window.p);
    void *result = w->user_data;
    return result;
}

Function F32
G_WindowScaleFactorGet(OpaqueHandle window)
{
    MAC_Window *w = PtrFromInt(window.p);
    F32 result = [w->window backingScaleFactor];
    return result;
}

Function V2I
G_WindowDimensionsGet(OpaqueHandle window)
{
    MAC_Window *w = PtrFromInt(window.p);
    NSRect content_view_bounds = [w->window convertRectToBacking:[w->metal_view bounds]];
    V2I result =
    {
        .x = NSWidth(content_view_bounds),
        .y = NSHeight(content_view_bounds),
    };
    return result;
}

Function Range2F
G_WindowClientRectGet(OpaqueHandle window)
{
    MAC_Window *w = PtrFromInt(window.p);
    NSRect content_view_bounds = [w->window convertRectToBacking:[w->metal_view bounds]];
    Range2F result =
    {
        .min = { .x = NSMinX(content_view_bounds), .y = NSMinY(content_view_bounds), },
        .max = { .x = NSMaxX(content_view_bounds), .y = NSMaxY(content_view_bounds), },
    };
    return result;
}

Function V2F
G_WindowMousePositionGet(OpaqueHandle window)
{
    MAC_Window *w = PtrFromInt(window.p);

    NSPoint point = [w->window convertPointToBacking:[w->window mouseLocationOutsideOfEventStream]];
    F32 height = NSHeight([w->window convertRectToBacking:[w->metal_view bounds]]);

    V2F result = { .x = point.x, .y = height - point.y, };
    return result;
}

Function B32
G_WindowKeyStateGet(OpaqueHandle window, G_Key key)
{
    MAC_Window *w = PtrFromInt(window.p);
    B32 result = !!(w->is_key_down_user[key >> 6] & Bit(key & 0x3F));
    return result;
}

Function G_ModifierKeys
G_WindowModifiersMaskGet(OpaqueHandle window)
{
    MAC_Window *w = PtrFromInt(window.p);
    G_ModifierKeys result = w->last_modifier_keys;
    return result;
}

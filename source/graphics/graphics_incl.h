
#if !defined(GRAPHICS_INCL_H)
#define GRAPHICS_INCL_H

//~NOTE(tbt): initialisation and cleanup

Function void G_Init    (void);
Function void G_Cleanup (void);

//~NOTE(tbt): key codes

typedef U32 G_ModifierKeys;
#define G_ModifierKeys_Ctrl   Bit(0)
#define G_ModifierKeys_Shift  Bit(1)
#define G_ModifierKeys_Alt    Bit(2)
#define G_ModifierKeys_Super  Bit(3)
#define G_ModifierKeys_Option G_ModifierKeys_Alt
#define G_ModifierKeys_Cmd    G_ModifierKeys_Super
#define G_ModifierKeys_Any    (~((G_ModifierKeys)0))

#define G_KeyDef(NAME, STRING, IS_REAL) G_Key_ ## NAME ,
typedef enum G_Key G_Key;
enum G_Key
{
#include "graphics_keylist.h"
};

Function S8             G_S8FromKey       (G_Key key);
Function G_ModifierKeys G_ModifierFromKey (G_Key key);

//~NOTE(tbt): event queues

typedef enum G_EventKind G_EventKind;
enum G_EventKind
{
    G_EventKind_None,
    G_EventKind_Key,
    G_EventKind_Char,
    G_EventKind_MouseMove,
    G_EventKind_MouseScroll,
    G_EventKind_MouseLeave,
    G_EventKind_WindowSize,
    G_EventKind_DPIChange,
    G_EventKind_MAX,
};

#define G_ScaleFactorFromDPI(D) ((F32)((D).x) / 96.0f)

#define G_S8FromEventKind(K) \
(G_EventKind_None        == (K) ? S8("None")          : \
G_EventKind_Key         == (K) ? S8("Key")           : \
G_EventKind_Char        == (K) ? S8("Char")          : \
G_EventKind_MouseMove   == (K) ? S8("Mouse Move")    : \
G_EventKind_MouseScroll == (K) ? S8("Mouse Scroll")  : \
G_EventKind_MouseLeave  == (K) ? S8("Mouse Leave")   : \
G_EventKind_WindowSize  == (K) ? S8("Window Resize") : \
G_EventKind_DPIChange  == (K) ? S8("DPI Change")     : \
S8("ERROR - NOT AN EVENT KIND"))

typedef struct G_Event G_Event;
struct G_Event
{
    B32 is_consumed;
    G_EventKind kind;
    G_ModifierKeys modifiers;
    G_Key key;
    B32 is_down;
    B32 is_repeat;
    V2I prev_size;
    V2I size;
    V2F position;
    V2F delta;
    U32 codepoint;
    F32 pressure;
};

Function void G_EventConsume (G_Event *event);

enum { G_EventQueueCap = 512, };

typedef struct G_EventQueue G_EventQueue;
struct G_EventQueue
{
    G_Event events[G_EventQueueCap];
    U64 events_count;
};

Function void G_EventQueuePush  (G_EventQueue *queue, G_Event event);
Function void G_EventQueueClear (G_EventQueue *queue);

Function G_Event *G_EventQueueIncrementNotConsumed (G_EventQueue *queue, G_Event *current);
Function G_Event *G_EventQueueFirstGet             (G_EventQueue *queue);
Function G_Event *G_EventQueueNextGet              (G_EventQueue *queue, G_Event *current);
#define G_EventQueueForEach(Q, V) G_Event *(V) = G_EventQueueFirstGet(Q); 0 != (V); (V) = G_EventQueueNextGet((Q), (V))

//~NOTE(tbt): event loop wrappers

Function G_Event *G_EventQueueFirstEventFromKind (G_EventQueue *queue, G_EventKind kind, B32 should_consume);
Function B32      G_EventQueueHasKeyEvent       (G_EventQueue *queue, G_Key key, G_ModifierKeys modifiers, B32 is_down, B32 is_repeat, B32 should_consume);
Function B32      G_EventQueueHasKeyDown        (G_EventQueue *queue, G_Key key, G_ModifierKeys modifiers, B32 should_consume);
Function B32      G_EventQueueHasKeyUp          (G_EventQueue *queue, G_Key key, G_ModifierKeys modifiers, B32 should_consume);
Function V2F      G_EventQueueSumDelta          (G_EventQueue *queue, G_EventKind filter, B32 should_consume);

//~NOTE(tbt): graphical app

typedef void G_WindowHookOpen  (OpaqueHandle window);
typedef void G_WindowHookDraw  (OpaqueHandle window);
typedef void G_WindowHookClose (OpaqueHandle window);

typedef struct G_WindowHooks G_WindowHooks;
struct G_WindowHooks
{
    G_WindowHookOpen *open;
    G_WindowHookDraw *draw;
    G_WindowHookClose *close;
};

typedef I32 G_CursorKind;
enum G_CursorKind
{
    G_CursorKind_Default, // NOTE(tbt): normal arrow
    G_CursorKind_HResize, // NOTE(tbt): double ended horizontal arrow
    G_CursorKind_VResize, // NOTE(tbt): double ended vertical arrow
    G_CursorKind_Resize,  // NOTE(tbt): double ended diagonal arrow
    G_CursorKind_Move,    // NOTE(tbt): 4 arrows
    G_CursorKind_Loading, // NOTE(tbt): hourglass or similar
    G_CursorKind_No,      // NOTE(tbt): slashed circle or similar
    G_CursorKind_IBeam,   // NOTE(tbt): 'I' shaped vertical bar
    G_CursorKind_Hidden,  // NOTE(tbt): no cursor
    
    G_CursorKind_MAX,
};

Function OpaqueHandle G_WindowOpen  (S8 title, V2I dimensions, G_WindowHooks hooks, void *user_data);
Function void         G_WindowClose (OpaqueHandle window);
Function void         G_MainLoop    (void);

Function B32    G_IsDarkMode              (void);
Function void   G_ShouldWaitForEvents     (B32 should_wait_for_events);
Function void   G_DoNotWaitForEventsUntil (U64 until);

Function void           G_WindowClearColourSet    (OpaqueHandle window, V4F clear_colour);
Function void           G_WindowFullscreenSet     (OpaqueHandle window, B32 is_fullscreen);
Function void           G_WindowVSyncSet          (OpaqueHandle window, B32 is_vsync);
Function void           G_WindowCursorKindSet     (OpaqueHandle window, G_CursorKind kind);
Function void           G_WindowMousePositionSet  (OpaqueHandle window, V2F position);
Function void           G_WindowUserDataSet       (OpaqueHandle window, void *data);
Function B32            G_WindowIsFullscreen      (OpaqueHandle window);
Function U64            G_WindowFrameTimeGet      (OpaqueHandle window);
Function Arena         *G_WindowFrameArenaGet     (OpaqueHandle window);
Function Arena         *G_WindowArenaGet          (OpaqueHandle window);
Function G_EventQueue  *G_WindowEventQueueGet     (OpaqueHandle window);
Function void          *G_WindowUserDataGet       (OpaqueHandle window);
Function F32            G_WindowScaleFactorGet    (OpaqueHandle window);
Function V2I            G_WindowDimensionsGet     (OpaqueHandle window);
Function Range2F        G_WindowClientRectGet     (OpaqueHandle window);
Function V2F            G_WindowMousePositionGet  (OpaqueHandle window);
Function B32            G_WindowKeyStateGet       (OpaqueHandle window, G_Key key);
Function G_ModifierKeys G_ModifiersMaskGet        (OpaqueHandle window);

#endif

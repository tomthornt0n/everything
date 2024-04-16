
#if !defined(UI_INCL_H)
#define UI_INCL_H

//~NOTE(tbt): control stack x-macros

enum { UI_ControlStackCap = 128, };

#define UI_ControlStackList \
UI_ControlStackDef(scale,                Scale,              F32,                  1.0f)                                      \
UI_ControlStackDef(parent,               Parent,             UI_Node *,            &ui->root)                                 \
UI_ControlStackDef(width,                Width,              UI_Size,              UI_Size(UI_SizeMode_PercentageOfAncestor, 1.0f, 0.0f)) \
UI_ControlStackDef(height,               Height,             UI_Size,              UI_Size(UI_SizeMode_PercentageOfAncestor, 1.0f, 0.0f)) \
UI_ControlStackDef(bg,                   BgCol,              V4F,                  ColFromHex(0xeceff4ff))                    \
UI_ControlStackDef(fg,                   FgCol,              V4F,                  ColFromHex(0x2e3440ff))                    \
UI_ControlStackDef(growth,               Growth,             F32,                  0.0f)                                      \
UI_ControlStackDef(stroke_width,         StrokeWidth,        F32,                  2.0f)                                      \
UI_ControlStackDef(corner_radius,        CornerRadius,       F32,                  4.0f)                                      \
UI_ControlStackDef(shadow_radius,        ShadowRadius,       F32,                  8.0f)                                      \
UI_ControlStackDef(hot_bg,               HotBgCol,           V4F,                  UI_BgColPeek())                            \
UI_ControlStackDef(hot_fg,               HotFgCol,           V4F,                  UI_FgColPeek())                            \
UI_ControlStackDef(hot_growth,           HotGrowth,          F32,                  UI_GrowthPeek() + 2.0f)                    \
UI_ControlStackDef(hot_stroke_width,     HotStrokeWidth,     F32,                  UI_StrokeWidthPeek())                      \
UI_ControlStackDef(hot_corner_radius,    HotCornerRadius,    F32,                  UI_CornerRadiusPeek())                     \
UI_ControlStackDef(hot_shadow_radius,    HotShadowRadius,    F32,                  UI_ShadowRadiusPeek())                     \
UI_ControlStackDef(hot_cursor_kind,      HotCursorKind,      G_CursorKind,         G_CursorKind_Default)                      \
UI_ControlStackDef(active_bg,            ActiveBgCol,        V4F,                  UI_HotBgColPeek())                         \
UI_ControlStackDef(active_fg,            ActiveFgCol,        V4F,                  UI_HotFgColPeek())                         \
UI_ControlStackDef(active_growth,        ActiveGrowth,       F32,                  UI_HotGrowthPeek()*0.5f)                   \
UI_ControlStackDef(active_stroke_width,  ActiveStrokeWidth,  F32,                  UI_HotStrokeWidthPeek())                   \
UI_ControlStackDef(active_corner_radius, ActiveCornerRadius, F32,                  UI_HotCornerRadiusPeek())                  \
UI_ControlStackDef(active_shadow_radius, ActiveShadowRadius, F32,                  UI_HotShadowRadiusPeek())                  \
UI_ControlStackDef(active_cursor_kind,   ActiveCursorKind,   G_CursorKind,         UI_HotCursorKindPeek())                    \
UI_ControlStackDef(children_layout,      ChildrenLayout,     Axis2,                Axis2_X)                                   \
UI_ControlStackDef(flags,                DefaultFlags,       UI_Flags,             0)                                         \
UI_ControlStackDef(font,                 Font,               OpaqueHandle,         ui_font)                                   \
UI_ControlStackDef(font_size,            FontSize,           I32,                  20)                                        \
UI_ControlStackDef(floating_position,    FloatingPosition,   V2F,                  V2F(0.0f, 0.0f))                           \
UI_ControlStackDef(texture,              Texture,            OpaqueHandle,         R2D_TextureNil())                          \
UI_ControlStackDef(texture_region,       TextureRegion,      Range2F,              r2d_entire_texture)                        \
UI_ControlStackDef(nav,                  Nav,                UI_NavContext,        UI_NavContext(&UI_NavDefault, &ui->root, UI_KeyNil())) \
UI_ControlStackDef(text_trunc_suffix,    TextTruncSuffix,    S8,                   S8("..."))

//~NOTE(tbt): constants

#define UI_ScrollSpeed       (32.0f)
#define UI_AnimateSlop       (0.001f)
#define UI_AnimateSpeedScale (0.5f)

//~NOTE(tbt): types

typedef struct UI_Node UI_Node;

//-NOTE(tbt): rendering details

typedef struct UI_PreparedTextCacheNode UI_PreparedTextCacheNode;
struct UI_PreparedTextCacheNode
{
    UI_PreparedTextCacheNode *next;
    U64 string_hash[2];
    FONT_PreparedText pt;
    U64 last_touched_frame_index;
};

typedef struct UI_PreparedTextCache UI_PreparedTextCache;
struct UI_PreparedTextCache
{
    UI_PreparedTextCacheNode *free_list;
    UI_PreparedTextCacheNode *buckets[4096];
};

//-NOTE(tbt): text editing

typedef enum
{
    UI_EditTextActionFlags_Delete     = Bit(0),
    UI_EditTextActionFlags_Copy       = Bit(1),
    UI_EditTextActionFlags_Paste      = Bit(2),
    UI_EditTextActionFlags_SelectAll  = Bit(3),
    
    UI_EditTextActionFlags_StickMark  = Bit(4),
    
    // NOTE(tbt): mutually exclusive
    UI_EditTextActionFlags_WordLevel  = Bit(5),
    UI_EditTextActionFlags_LineLevel  = Bit(6),
} UI_EditTextActionFlags;

typedef struct UI_EditTextAction UI_EditTextAction;
struct UI_EditTextAction
{
    G_Key key;
    G_ModifierKeys modifiers;
    
    UI_EditTextActionFlags flags;
    I32 offset;
    U32 codepoint;
};

typedef struct UI_EditTextOp UI_EditTextOp;
struct UI_EditTextOp
{
    Range1U64 replace;
    S8 with;
    Range1U64 next_selection;
};

typedef enum
{
    UI_EditTextStatus_None = 0,
    UI_EditTextStatus_Edited      = Bit(0),
    UI_EditTextStatus_CursorMoved = Bit(1),
    UI_EditTextStatus_MarkMoved   = Bit(2),
} UI_EditTextStatus;

typedef S8 UI_EditTextFilterHook (Arena *arena, S8 entered, void *user_data);

//-NOTE(tbt): node cache keying

typedef struct UI_Key UI_Key;
struct UI_Key
{
    U64 hash[2];
    char id_buffer[1024];
    U64 id_len;
};

//-NOTE(tbt): semantic sizes

typedef enum UI_SizeMode UI_SizeMode;
enum UI_SizeMode
{
    UI_SizeMode_None,
    UI_SizeMode_Pixels,
    UI_SizeMode_FromText,
    UI_SizeMode_PercentageOfAncestor,
    UI_SizeMode_SumOfChildren,
    UI_SizeMode_MAX,
};

typedef struct UI_Size UI_Size;
struct UI_Size
{
    UI_SizeMode mode;
    F32 f;
    F32 strictness;
};
#define UI_Size(...) ((UI_Size){ __VA_ARGS__, })

//-NOTE(tbt): node flags

typedef enum
{
    // NOTE(tbt): rendering
    UI_Flags_DrawFill                  = Bit(0),
    UI_Flags_DrawStroke                = Bit(1),
    UI_Flags_DrawDropShadow            = Bit(2),
    UI_Flags_DrawText                  = Bit(3),
    UI_Flags_DrawHotStyle              = Bit(4),
    UI_Flags_DrawActiveStyle           = Bit(5),
    UI_Flags_DrawTextCursor            = Bit(6),
    UI_Flags_DoNotMask                 = Bit(7),
    UI_Flags_Hidden                    = Bit(8),
    
    // NOTE(tbt): animation
    UI_Flags_AnimateHot                = Bit(9),
    UI_Flags_AnimateActive             = Bit(10),
    UI_Flags_AnimateInheritHot         = Bit(11),
    UI_Flags_AnimateInheritActive      = Bit(12),
    UI_Flags_AnimateSmoothScroll       = Bit(13),
    UI_Flags_AnimateSmoothLayout       = Bit(14),
    UI_Flags_AnimateIn                 = Bit(15),
    UI_Flags_SetCursorKind             = Bit(16),
    
    // NOTE(tbt): input
    UI_Flags_Pressable                 = Bit(17),
    UI_Flags_Scrollable                = Bit(18),
    UI_Flags_CursorPositionToCenterX   = Bit(19),
    UI_Flags_CursorPositionToCenterY   = Bit(20),
    UI_Flags_KeyboardFocused           = Bit(21),
    UI_Flags_NavDefaultFilter          = Bit(23),
    UI_Flags_ScrollChildrenX           = Bit(24),
    UI_Flags_ScrollChildrenY           = Bit(25),
    
    // NOTE(tbt): layout
    UI_Flags_ClampViewOffsetToOverflow = Bit(26),
    UI_Flags_OverflowX                 = Bit(27),
    UI_Flags_OverflowY                 = Bit(28),
    UI_Flags_Floating                  = Bit(29),
} UI_Flags;

#define UI_Flags_CursorPositionToCenter   (UI_Flags_CursorPositionToCenterX | UI_Flags_CursorPositionToCenterY)
#define UI_Flags_ScrollChildren           (UI_Flags_ScrollChildrenX | UI_Flags_ScrollChildrenY)
#define UI_Flags_Overflow                 (UI_Flags_OverflowX | UI_Flags_OverflowY)

//-NOTE(tbt): node string parse artifact

typedef struct UI_NodeStrings UI_NodeStrings;
struct UI_NodeStrings
{
    S8 id;   // NOTE(tbt): string to be hashed
    S8 text; // NOTE(tbt): to be displayed
};

//-NOTE(tbt): navigation

typedef void UI_NavHook(void *user_data, UI_Node *node);

typedef struct UI_NavContext UI_NavContext;
struct UI_NavContext
{
    UI_NavHook *hook;
    void *user_data;
    UI_Key key;
};
#define UI_NavContext(...) ((UI_NavContext){ __VA_ARGS__, })

//-NOTE(tbt): UI node 'lego brick'

typedef struct UI_Node UI_Node;
struct UI_Node
{
    // NOTE(tbt): tree links
    UI_Node *parent;
    UI_Node *first;
    UI_Node *last;
    UI_Node *next;
    UI_Node *prev;
    
    // NOTE(tbt): links
    UI_Node *next_hash;
    
    // NOTE(tbt): key data
    UI_Key key;
    
    // NOTE(tbt): per frame parameters, usually corresponding to a control stack
    UI_Flags flags;
    UI_Size size[Axis2_MAX];
    S8 text;
    Axis2 children_layout_axis;
    OpaqueHandle font_provider;
    I32 font_size;
    V4F bg;
    V4F fg;
    F32 growth;
    F32 stroke_width;
    F32 corner_radius;
    F32 shadow_radius;
    V4F hot_bg;
    V4F hot_fg;
    F32 hot_growth;
    F32 hot_stroke_width;
    F32 hot_corner_radius;
    F32 hot_shadow_radius;
    G_CursorKind hot_cursor_kind;
    V4F active_bg;
    V4F active_fg;
    F32 active_growth;
    F32 active_stroke_width;
    F32 active_corner_radius;
    F32 active_shadow_radius;
    G_CursorKind active_cursor_kind;
    OpaqueHandle texture;
    Range2F texture_region;
    UI_NavContext nav_context;
    S8 text_trunc_suffix;
    B32 is_new;
    F32 scale;
    
    // NOTE(tbt): post measurement data
    V2F computed_size;
    
    // NOTE(tbt): post layout data
    Range2F target_relative_rect;
    Range2F relative_rect;
    Range2F rect;
    Range2F clipped_rect;
    V2F overflow;
    
    // NOTE(tbt): persistant data
    U64 last_touched_frame_index;
    F32 hot_t;
    F32 active_t;
    F32 keyboard_focus_t;
    F32 user_t;
    F32 cursor_blink_cooldown; 
    V2F view_offset;
    V2F target_view_offset;
    Range1U64 text_selection;
    V2F text_cursor_pos;
    V2F target_text_cursor_pos;
    B32 is_dragging;
    UI_Key nav_default_keyboard_focus;
};

//-NOTE(tbt): interaction feedback

typedef struct UI_Use UI_Use;
struct UI_Use
{
    UI_Node *node;
    
    B8 is_hovering;
    V2F scroll_delta;
    B8 is_dragging;
    B8 is_left_down;
    B8 is_left_up;
    B8 is_right_down;
    B8 is_right_up;
    B8 is_double_clicked;
    
    V2F initial_mouse_position;
    V2F initial_mouse_relative_position;
    B8 is_pressed;
    V2F drag_delta;
    
    B32 is_edited;
    B32 is_toggled;
};

//-NOTE(tbt): core UI state

typedef struct UI_State UI_State;
struct UI_State
{
    OpaqueHandle window;
    
    UI_Node root;
    
    G_CursorKind default_cursor_kind;
    G_CursorKind next_cursor_kind;
    G_CursorKind cursor_kind;
    
    V2F last_initial_mouse_position;
    V2F last_initial_mouse_relative_position;
    
    B32 was_last_interaction_keyboard;
    
    S8List salt_stack;
    
    UI_Key hot;
    UI_Key active;
    
    UI_PreparedTextCache pt_cache;
    FONT_PreparedTextFreeList pt_free_list;
    DRW_List draw_list;
    
    U64 frame_index;
    
#define UI_ControlStackDef(NAME_L, NAME_U, TYPE, DEFAULT) \
TYPE Glue(NAME_L, _stack)[UI_ControlStackCap]; \
U64 Glue(NAME_L, _stack_count); \
B32 Glue(NAME_L, _set_next_flag);
    UI_ControlStackList
#undef UI_ControlStackDef
    
    UI_Key interface_stack[256];
    U64 interface_stack_count;
    
    UI_Node buckets[1024];
    UI_Node *free_list;
};

//~NOTE(tbt): globals

//-NOTE(tbt): globally selected ui context
Global UI_State *ui;

//-NOTE(tbt): default fonts
Global OpaqueHandle ui_font = {0};
Global OpaqueHandle ui_font_bold = {0};
Global OpaqueHandle ui_font_icons = {0};

//-NOTE(tbt): text editing
Global UI_EditTextAction ui_edit_text_key_bindings[] =
// key            | modifiers                                   | flags                                                                | offset
{
    { G_Key_C,         (G_ModifierKeys_Ctrl),                        (UI_EditTextActionFlags_Copy | UI_EditTextActionFlags_StickMark),      0, },
    { G_Key_Insert,    (G_ModifierKeys_Ctrl),                        (UI_EditTextActionFlags_Copy),                                         0, },
    
    { G_Key_V,         (G_ModifierKeys_Ctrl),                        (UI_EditTextActionFlags_Paste),                                        0, },
    { G_Key_Insert,    (G_ModifierKeys_Shift),                       (UI_EditTextActionFlags_Paste),                                        0, },
    
    { G_Key_X,         (G_ModifierKeys_Ctrl),                        (UI_EditTextActionFlags_Copy | UI_EditTextActionFlags_Delete),         0, },
    { G_Key_Delete,    (G_ModifierKeys_Shift),                       (UI_EditTextActionFlags_Copy | UI_EditTextActionFlags_Delete),         0, },
    
    { G_Key_A,         (G_ModifierKeys_Ctrl),                        (UI_EditTextActionFlags_SelectAll | UI_EditTextActionFlags_StickMark), 0, },
    
    { G_Key_Backspace, 0,                                            (UI_EditTextActionFlags_Delete),                                       -1, },
    { G_Key_Backspace, (G_ModifierKeys_Ctrl),                        (UI_EditTextActionFlags_Delete | UI_EditTextActionFlags_WordLevel),    -1, },
    
    { G_Key_Delete,    0,                                            (UI_EditTextActionFlags_Delete),                                       +1, },
    { G_Key_Delete,    (G_ModifierKeys_Ctrl),                        (UI_EditTextActionFlags_Delete | UI_EditTextActionFlags_WordLevel),    +1, },
    
    { G_Key_Left,      0,                                            0,                                                                     -1, },
    { G_Key_Left,      (G_ModifierKeys_Ctrl),                        (UI_EditTextActionFlags_WordLevel),                                    -1, },
    { G_Key_Left,      (G_ModifierKeys_Shift),                       (UI_EditTextActionFlags_StickMark),                                    -1, },
    { G_Key_Left,      (G_ModifierKeys_Ctrl | G_ModifierKeys_Shift), (UI_EditTextActionFlags_StickMark | UI_EditTextActionFlags_WordLevel), -1, },
    
    { G_Key_Right,     0,                                            0,                                                                     +1, },
    { G_Key_Right,     (G_ModifierKeys_Ctrl),                        (UI_EditTextActionFlags_WordLevel),                                    +1, },
    { G_Key_Right,     (G_ModifierKeys_Shift),                       (UI_EditTextActionFlags_StickMark),                                    +1, },
    { G_Key_Right,     (G_ModifierKeys_Ctrl | G_ModifierKeys_Shift), (UI_EditTextActionFlags_StickMark | UI_EditTextActionFlags_WordLevel), +1, },
    
    { G_Key_Home,      0,                                            (UI_EditTextActionFlags_LineLevel),                                    -1, },
    { G_Key_Home,      (G_ModifierKeys_Shift),                       (UI_EditTextActionFlags_StickMark | UI_EditTextActionFlags_LineLevel), -1, },
    
    { G_Key_End,       0,                                            (UI_EditTextActionFlags_LineLevel),                                    +1, },
    { G_Key_End,       (G_ModifierKeys_Shift),                       (UI_EditTextActionFlags_StickMark | UI_EditTextActionFlags_LineLevel), +1, },
};

//~NOTE(tbt): rendering

Function FONT_PreparedText *UI_PreparedTextFromS8 (OpaqueHandle font_provider, I32 font_size, S8 string);

//~NOTE(tbt): utilities

//-NOTE(tbt): rect cut layouts
typedef Range2F UI_CutProc(Range2F *rect, F32 f);
Function Range2F UI_CutLeft (Range2F *rect, F32 f);
Function Range2F UI_CutRight (Range2F *rect, F32 f);
Function Range2F UI_CutTop (Range2F *rect, F32 f);
Function Range2F UI_CutBottom (Range2F *rect, F32 f);

Function Range2F UI_GetLeft (Range2F rect, F32 f);
Function Range2F UI_GetRight (Range2F rect, F32 f);
Function Range2F UI_GetTop (Range2F rect, F32 f);
Function Range2F UI_GetBottom (Range2F rect, F32 f);

//-NOTE(tbt): text editing

// NOTE(tbt): stock filter hooks
Function S8  UI_EditTextFilterHookDefault  (Arena *arena, S8 entered, void *user_data);
Function S8  UI_EditTextFilterHookLineEdit (Arena *arena, S8 entered, void *user_data);
Function S8  UI_EditTextFilterHookNumeric (Arena *arena, S8 entered, void *user_data);
Function S8  UI_EditTextFilterHookDate (Arena *arena, S8 entered, void *user_data);

// NOTE(tbt): input -> action -> operation
Function B32 UI_EditTextActionFromEvent (G_Event *e, UI_EditTextAction *result);
Function UI_EditTextOp UI_EditTextOpFromAction (Arena *arena, UI_EditTextAction action, S8 string, Range1U64 selection, UI_EditTextFilterHook *filter_hook, void *filter_hook_user_data);

// NOTE(tbt): helper for going straight from input to operation and applying to a S8Builder
Function UI_EditTextStatus UI_EditS8Builder (G_Event *e, Range1U64 *selection, S8Builder *text, UI_EditTextFilterHook *filter_hook, void *filter_hook_user_data);

//-NOTE(tbt): animation
Function void UI_Animate1F (F32 *a, F32 b, F32 speed);
Function void UI_AnimateF (F32 a[], F32 b[], U64 n, F32 speed);

//-NOTE(tbt): misc. text rendering helpers
Function V2F UI_TextPositionFromRect (FONT_PreparedText *pt, Range2F rect);
Function S8 UI_TruncateWithSuffix (Arena *arena, S8 string, F32 width, FONT_PreparedText *pt, F32 padding, S8 suffix);

//~NOTE(tbt): core

//-NOTE(tbt): node cache keying

// NOTE(tbt): stack of extra identifiers used when computing a key for a node
Function void UI_SaltPush (S8 id);
Function void UI_SaltPushFmtV (const char *fmt, va_list args);
Function void UI_SaltPushFmt (const char *fmt, ...);
Function void UI_SaltPop (void);
#define UI_Salt(S) DeferLoop(UI_SaltPush(S), UI_SaltPop())
#define UI_SaltFmt(...) DeferLoop(UI_SaltPushFmt(__VA_ARGS__), UI_SaltPop())

// NOTE(tbt): key utilities
Function UI_Key UI_KeyNil (void);
Function B32 UI_KeyIsNil (UI_Key *key);
Function UI_Key UI_KeyFromIdAndSaltStack (S8 id, S8List salt_stack);
Function B32 UI_KeyMatch (UI_Key *a, UI_Key *b);
Function B32 UI_KeyMatchOrNil (UI_Key *a, UI_Key *b);

//-NOTE(tbt): control stacks
#define UI_ControlStackDef(NAME_L, NAME_U, TYPE, DEFAULT) \
Function TYPE Glue(UI_, Glue(NAME_U, Peek)) (void);   \
Function TYPE Glue(UI_, Glue(NAME_U, Pop))  (void);   \
Function void Glue(UI_, Glue(NAME_U, Push)) (TYPE v);
UI_ControlStackList
#undef UI_ControlStackDef

//-NOTE(tbt): tree utilities
Function UI_Node *UI_NodeLookup (UI_Key *key);
Function UI_Node *UI_NodeLookupIncludingUnused (UI_Key *key);
Function B32 UI_NodeIsChildOf (UI_Node *child, UI_Key *parent);

//-NOTE(tbt): low-level node creation API
Function UI_NodeStrings UI_TextAndIdFromString (S8 string);
Function UI_Node *UI_NodeFromKey (UI_Key *key);
Function void  UI_NodeUpdate (UI_Node *node, UI_Flags flags, S8 text);
Function void UI_NodeInsert (UI_Node *node, UI_Node *parent);

//-NOTE(tbt): higher level wrappers over the above
Function UI_Node *UI_NodeMake (UI_Flags flags, S8 string);
Function UI_Node *UI_NodeMakeFromFmtV (UI_Flags flags, const char *fmt, va_list args);
Function UI_Node *UI_NodeMakeFromFmt (UI_Flags flags, const char *fmt, ...);
Function UI_Node *UI_OverlayFromString (S8 string);

//-NOTE(tbt): navigation
Function B32 UI_NavInterfaceStackPush (UI_Key *key);
Function UI_Key UI_NavInterfaceStackPop (void);
Function UI_Key UI_NavInterfaceStackPeek (void);
Function B32 UI_NavInterfaceStackMatchTop (UI_Key *key);
Function UI_Key UI_NavInterfaceStackPopCond (UI_Key *match_top);
Function void UI_NavDefault (void *user_data, UI_Node *node);
Function UI_Node *UI_NavDefaultNext (UI_Node *node);
Function UI_Node *UI_NavDefaultPrev (UI_Node *node);

//-NOTE(tbt): interaction
Function UI_Use UI_UseFromNode (UI_Node *node);

//-NOTE(tbt): measurement
Function F32     UI_MeasureSizeFromText (UI_Node *node, Axis2 axis);
Function Range1F UI_MeasureSumOfChildren (UI_Node *root, Axis2 axis);

//-NOTE(tbt): layout
Function void UI_LayoutPassIndependentSizes (UI_Node *root, Axis2 axis);
Function void UI_LayoutPassUpwardsDependentSizes (UI_Node *root, Axis2 axis);
Function void UI_LayoutPassDownwardsDependentSizes (UI_Node *root, Axis2 axis);
Function void UI_LayoutPassSolveViolations (UI_Node *root, Axis2 axis);
Function void UI_LayoutPassComputeRelativeRects (UI_Node *root);
Function void UI_LayoutPassComputeFinalRects (UI_Node *root);

//-NOTE(tbt): animation
Function void UI_AnimationPass (UI_Node *root, UI_Key *hot_key, UI_Key *active_key);

//-NOTE(tbt): rendering
Function void UI_RenderPass (UI_Node *root, Range2F mask, OpaqueHandle window, DRW_List *draw_list);
Function void UI_RenderKeyboardFocusHighlight (UI_Node *root, Range2F mask, OpaqueHandle window, DRW_List *draw_list);

//-NOTE(tbt): main API
Function UI_State *UI_Init (OpaqueHandle window);
Function void UI_Begin (UI_State *ui_state);
Function void UI_End (void);

//~NOTE(tbt): builder helpers

#define UI_SizeNone()          (UI_Size(UI_SizeMode_None))
#define UI_SizePx(F, S)        (UI_Size(UI_SizeMode_Pixels, (F), (S)))
#define UI_SizeFromText(P, S)  (UI_Size(UI_SizeMode_FromText, (P), (S)))
#define UI_SizePct(F, S)       (UI_Size(UI_SizeMode_PercentageOfAncestor, (F), (S)))
#define UI_SizeSum(S)          (UI_Size(UI_SizeMode_SumOfChildren, (0.0f), (S)))
#define UI_SizeFill()          UI_SizePct(1.0f, 0.0f)

#define UI_Parent(P)              DeferLoop(UI_ParentPush(P), UI_ParentPop())
#define UI_ChildrenLayout(A)      DeferLoop(UI_ChildrenLayoutPush(A), UI_ChildrenLayoutPop())
#define UI_BgCol(C)               DeferLoop(UI_BgColPush(C), UI_BgColPop())
#define UI_FgCol(C)               DeferLoop(UI_FgColPush(C), UI_FgColPop())
#define UI_Growth(G)              DeferLoop(UI_GrowthPush(G), UI_GrowthPop())
#define UI_StrokeWidth(W)         DeferLoop(UI_StrokeWidthPush(W), UI_StrokeWidthPop())
#define UI_CornerRadius(R)        DeferLoop(UI_CornerRadiusPush(R), UI_CornerRadiusPop())
#define UI_ShadowRadius(R)        DeferLoop(UI_ShadowRadiusPush(R), UI_ShadowRadiusPop())
#define UI_HotBgCol(C)            DeferLoop(UI_HotBgColPush(C), UI_HotBgColPop())
#define UI_HotFgCol(C)            DeferLoop(UI_HotFgColPush(C), UI_HotFgColPop())
#define UI_HotGrowth(G)           DeferLoop(UI_HotGrowthPush(G), UI_HotGrowthPop())
#define UI_HotStrokeWidth(W)      DeferLoop(UI_HotStrokeWidthPush(W), UI_HotStrokeWidthPop())
#define UI_HotCornerRadius(R)     DeferLoop(UI_HotCornerRadiusPush(R), UI_HotCornerRadiusPop())
#define UI_HotShadowRadius(R)     DeferLoop(UI_HotShadowRadiusPush(R), UI_HotShadowRadiusPop())
#define UI_HotCursorKind(K)       DeferLoop(UI_HotCursorKindPush(K), UI_HotCursorKindPop())
#define UI_ActiveBgCol(C)         DeferLoop(UI_ActiveBgColPush(C), UI_ActiveBgColPop())
#define UI_ActiveFgCol(C)         DeferLoop(UI_ActiveFgColPush(C), UI_ActiveFgColPop())
#define UI_ActiveGrowth(G)        DeferLoop(UI_ActiveGrowthPush(G), UI_ActiveGrowthPop())
#define UI_ActiveStrokeWidth(W)   DeferLoop(UI_ActiveStrokeWidthPush(W), UI_ActiveStrokeWidthPop())
#define UI_ActiveCornerRadius(R)  DeferLoop(UI_ActiveCornerRadiusPush(R), UI_ActiveCornerRadiusPop())
#define UI_ActiveShadowRadius(R)  DeferLoop(UI_ActiveShadowRadiusPush(R), UI_ActiveShadowRadiusPop())
#define UI_ActiveCursorKind(K)    DeferLoop(UI_ActiveCursorKindPush(K), UI_ActiveCursorKindPop())
#define UI_Width(S)               DeferLoop(UI_WidthPush(S), UI_WidthPop())
#define UI_Height(S)              DeferLoop(UI_HeightPush(S), UI_HeightPop())
#define UI_Font(F)                DeferLoop(UI_FontPush(F), UI_FontPop())
#define UI_FontSize(S)            DeferLoop(UI_FontSizePush(S), UI_FontSizePop())
#define UI_DefaultFlags(F)        DeferLoop(UI_DefaultFlagsPush(F), UI_DefaultFlagsPop())
#define UI_Salt(S)                DeferLoop(UI_SaltPush(S), UI_SaltPop())
#define UI_FixedRect(P)           DeferLoop(UI_FixedRectPush(P), UI_FixedRectPop())
#define UI_Texture(T)             DeferLoop(UI_TexturePush(T), UI_TexturePop())
#define UI_Nav(C)                 DeferLoop(UI_NavPush(C), UI_NavPop())
#define UI_TextTruncSuffix(S)     DeferLoop(UI_TextTruncSuffixPush(S), UI_TextTruncSuffixPop())

#define UI_Row() UI_ChildrenLayout(Axis2_X) UI_Parent(UI_NodeMake(UI_Flags_DoNotMask, S8("")))
#define UI_Column() UI_ChildrenLayout(Axis2_Y) UI_Parent(UI_NodeMake(UI_Flags_DoNotMask, S8("")))

#define UI_Pad(S) DeferLoop(UI_Spacer(S), UI_Spacer(S))

Function void UI_Spacer  (UI_Size size);

#endif

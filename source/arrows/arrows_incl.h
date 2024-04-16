
typedef enum
{
    OpKind_NONE,
    
    OpKind_Begin,
    OpKind_Stroke,
    OpKind_Erase,
    
    OpKind_MAX,
} OpKind;

typedef struct Op Op;
struct Op
{
    U8 kind;
    U8 pressure;
    U16 x, y;
};
#define Op(...) ((Op){ __VA_ARGS__, })

typedef struct Ops Ops;
struct Ops
{
    Arena *arena;
    Op *ops;
    U64 count;
};

Function void OpsClear  (Ops *ops);
Function void OpsAppend (Ops *ops, Op op);
Function Op   OpsPop    (Ops *ops);

typedef struct TagMetadata TagMetadata;
struct TagMetadata
{
    TagMetadata *next;
    U64 id;
    Range2F bounds;
    S8Builder name;
};

typedef struct Tags Tags;
struct Tags
{
    TagMetadata *first;
};

Function TagMetadata *TagMetadataFromId (Arena *arena, Tags *tags, U64 id);

typedef enum
{
    InteractionFlags_PenDown       = Bit(0),
    InteractionFlags_Erasing       = Bit(1),
    InteractionFlags_PickingColour = Bit(2),
    InteractionFlags_Panning       = Bit(3),
    InteractionFlags_Grouping      = Bit(4),
    InteractionFlags_Translating   = Bit(5),
    
    // NOTE(tbt): flags which cause PenDown to not produce actions
    InteractionFlags_DoNotDraw = (InteractionFlags_PickingColour |
                                  InteractionFlags_Panning |
                                  InteractionFlags_Translating),
} InteractionFlags;

typedef struct InteractionTableEntry InteractionTableEntry;
struct InteractionTableEntry
{
    G_Key key;
    InteractionFlags flags;
    S8 label;
    G_ModifierKeys modifiers;
};
ReadOnlyVar Global InteractionTableEntry interaction_table[] =
{
    { G_Key_MouseButtonLeft,   InteractionFlags_PenDown, },
    { G_Key_E,                 InteractionFlags_Erasing,       S8Initialiser("Erase"), },
    { G_Key_C,                 InteractionFlags_PickingColour, S8Initialiser("Pick colour"), },
    { G_Key_Space,             InteractionFlags_Panning,       S8Initialiser("Pan"), },
    { G_Key_MouseButtonMiddle, (InteractionFlags_PenDown |
                                InteractionFlags_Panning),     S8Initialiser("Pan"), },
    { G_Key_G,                 InteractionFlags_Grouping,      S8Initialiser("Group strokes into object"), },
    { G_Key_T,                 InteractionFlags_Translating,   S8Initialiser("Translate object"), },
};
Function InteractionFlags InteractionFlagsFromKeyAndModifiers(G_Key key, G_ModifierKeys modifiers);

// TODO(tbt): get multiple boards in ASAP - I think I might be making a lot of hidden assumptions that there is only one board

typedef struct Board Board;
struct Board
{
    Board *next;
    Board *prev;
    
    Ops actions;
    
    Ops effects;
    U64 actions_applied_count;
    U64 actions_up_to;
    
    V2F position;
    U64 tag_to_highlight;
};

ReadOnlyVar Global Board nil_board = { .next = &nil_board, .prev = &nil_board, };

ReadOnlyVar Global V2F board_size = { 1024.0f, 768.0f, };

typedef struct Boards Boards;
struct Boards
{
    Board *first;
    Board *last;
};

Function Board *PushBoard (Arena *arena, Boards *boards);

Function V2F    WorldSpaceFromScreenSpace   (V2F screen_space_position, OpaqueHandle window, V2F camera_position, F32 zoom);
Function Board *BoardFromWorldSpacePosition (V2F world_space_position, Boards boards);
Function V2F    BoardSpaceFromWorldSpace    (V2F world_space_position, Board *board);
Function V2F    ScreenSpaceFromBoardSpace   (V2F board_space_position, Board *board, OpaqueHandle window, V2F camera_position, F32 zoom);

typedef struct AppState AppState;
struct AppState
{
    UI_State *ui;
    
    InteractionFlags interaction_flags;
    
    Boards boards;
    Board *active_board;
    
    V2F camera_position;
    V2F target_camera_position;
    F32 zoom;
    F32 target_zoom;
    
    LINE_SegmentList segments;
    R2D_QuadList quads;
    
    Tags tags;
};

Function void WindowHookOpen  (OpaqueHandle window);
Function void WindowHookDraw  (OpaqueHandle window);
Function void WindowHookClose (OpaqueHandle window);

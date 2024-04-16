
typedef enum
{
    DRW_Backend_None,
    DRW_Backend_Renderer2D,
    DRW_Backend_RendererLine,
    DRW_Backend_MAX,
} DRW_Backend;

typedef enum
{
    DRW_CmdKind_Sprite,
    DRW_CmdKind_Text,
    DRW_CmdKind_LineSegment,
    DRW_CmdKind_MAX,
} DRW_CmdKind;

typedef struct DRW_Cmd DRW_Cmd;
struct DRW_Cmd
{
    Range2F src;
    Range2F mask;
    V4F colour_1;
    V4F colour_2;
    OpaqueHandle texture;
    DRW_Cmd *next;
    FONT_PreparedText *pt;
    V2F position_1;
    V2F position_2;
    F32 width_1;
    F32 width_2;
    F32 width_3;
    DRW_CmdKind kind;
};

typedef struct DRW_List DRW_List;
struct DRW_List
{
    DRW_Cmd *first;
    DRW_Cmd *last;
};

Function DRW_Backend DRW_BackendFromCmdKind (DRW_CmdKind kind);


Function R2D_Quad DRW_Renderer2DQuadFromCmd (DRW_Cmd *cmd);
Function void DRW_Sprite (Arena *arena, DRW_List *list, R2D_Quad quad, OpaqueHandle texture);

Function LINE_Segment DRW_RendererLineSegmentFromCmd (DRW_Cmd *cmd);
Function void DRW_LineSegment (Arena *arena, DRW_List *list, LINE_Segment segment, Range2F mask);

Function void DRW_Text (Arena *arena, DRW_List *list, FONT_PreparedText *text, V2F position, V4F colour, Range2F mask);

Function void DRW_ListSubmit (OpaqueHandle window, DRW_List *list);

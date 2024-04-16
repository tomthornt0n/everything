
#define V_StandardRowHeight (32.0f)

typedef void V_Proc(S8 root, U64 depth, AppCmd **cmds);

Function V_Proc *V_DefaultProcFromFilename (S8 filename);

typedef struct V_CanvasPerFileState V_CanvasPerFileState;
struct V_CanvasPerFileState
{
    Range2F rect;
    V2F camera_position;
    F32 zoom;
    V2F children_layout;
};

enum
{
    V_CanvasMaxDepth = 2,
};

Function void V_Default (S8 root, U64 depth, AppCmd **cmds);
Function void V_Canvas (S8 root, U64 depth, AppCmd **cmds);
Function void V_FileProperties (S8 root, U64 depth, AppCmd **cmds);
Function void V_Image (S8 root, U64 depth, AppCmd **cmds);

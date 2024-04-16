
typedef struct VD_Bitmap VD_Bitmap;
struct VD_Bitmap
{
    U8 *pixels;
    I32 width;
    I32 height;
};

typedef struct VD_Node VD_Node;
struct VD_Node
{
    VD_Node *next;
    VD_Node *prev;
    VD_Node *first;
    VD_Node *last;
    I32 children_count;
    U8 value;
    Range2F bounds;
};

typedef struct VD_StackFrame VD_StackFrame;
struct VD_StackFrame
{
    V2I pos;
};
#define VD_StackFrame(...) ((VD_StackFrame){ __VA_ARGS__, })

typedef struct VD_StackChunk VD_StackChunk;
struct VD_StackChunk
{
    VD_StackChunk *next;
    VD_StackFrame *frames;
    U64 cap;
    U64 count;
};

typedef struct VD_Stack VD_Stack;
struct VD_Stack
{
    VD_StackChunk *top;
    VD_StackChunk *free_list;
};

Function VD_Bitmap VD_PushBitmap (Arena *arena, I32 width, I32 height);
Function U8 VD_BitmapPixelAt (VD_Bitmap *bitmap, I32 x, I32 y);
Function void VD_BitmapSetPixel (VD_Bitmap *bitmap, I32 x, I32 y, U8 value);

Function void VD_StackPush (Arena *arena, VD_Stack *stack, VD_StackFrame frame);
Function B32 VD_StackPop (VD_Stack *stack, VD_StackFrame *result);

Function VD_Node *VD_TreeFromBitmap (Arena *arena, VD_Bitmap *bitmap, V2I root);

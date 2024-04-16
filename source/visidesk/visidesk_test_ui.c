

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

Global OpaqueHandle main_window = {0};

Global OpaqueHandle font = {0};

enum
{
    BitmapWidth = 1024,
    BitmapHeight = 768,
};

Global VD_Bitmap bitmap;
Global OpaqueHandle bitmap_texture = {0};

Global Arena *tree_arena = 0;
Global VD_Node *visidesk_tree;
Global UI_Node *ui_tree;
Global S8 ui_string;

Function void WindowHookOpen (OpaqueHandle window);
Function void WindowHookDraw (OpaqueHandle window);
Function void WindowHookClose (OpaqueHandle window);

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
    main_window = G_WindowOpen(S8("visidesk"), V2I(1024, 768), window_hooks, 0);
    
    Arena *permanent_arena = OS_TCtxGet()->permanent_arena;
    
    font = FONT_ProviderFromTTF(permanent_arena, (S8){ .buffer = font_atkinson_hyperlegible_regular, .len = font_atkinson_hyperlegible_regular_len, });
    
    tree_arena = ArenaAlloc(Megabytes(4));
    
    G_WindowClearColourSet(main_window, ColFromHex(0xeee8d5ff));
    
    bitmap = VD_PushBitmap(permanent_arena, BitmapWidth, BitmapHeight);
    
    ArenaTemp scratch = OS_TCtxScratchBegin(0, 0);
    Pixel *pixels = PushArray(scratch.arena, Pixel, BitmapWidth*BitmapHeight);
    for(U64 pixel_index = 0; pixel_index < BitmapWidth*BitmapHeight; pixel_index += 1)
    {
        pixels[pixel_index].val = 0xffe3f6fd;
    }
    bitmap_texture = R2D_TextureAlloc(pixels, V2I(BitmapWidth, BitmapHeight));
    ArenaTempEnd(scratch);
    
    G_MainLoop();
    
    R2D_Cleanup();
    G_Cleanup();
    OS_Cleanup();
}

Function void
WindowHookOpen(OpaqueHandle window)
{
    
}

Function void
UITreeFromVisideskTree_(Arena *arena, VD_Node *root, UI_Node *parent, U64 depth)
{
    {
        Range1F x = Range1F(Infinity, NegativeInfinity);
        Range1F y = Range1F(Infinity, NegativeInfinity);
        for(VD_Node *child = root->first; 0 != child; child = child->next)
        {
            V2F centre = Centre2F(child->bounds);
            x.min = Min1F(x.min, centre.x);
            y.min = Min1F(y.min, centre.y);
            x.max = Max1F(x.max, centre.x);
            y.max = Max1F(y.max, centre.y);
        }
        
        if(Measure1F(x) > Measure1F(y))
        {
            parent->children_layout_axis = Axis2_X;
        }
        else
        {
            parent->children_layout_axis = Axis2_Y;
        }
    }
    
    F32 total_children_size = 0.0f;
    for(VD_Node *child = root->first; 0 != child; child = child->next)
    {
        if(1 == child->children_count)
        {
            total_children_size += Measure2F(child->bounds).elements[parent->children_layout_axis];
        }
    }
    
    if(root->children_count > 1)
    {
        ArenaTemp scratch = OS_TCtxScratchBegin(&arena, 1);
        VD_Node **ptrs = PushArray(scratch.arena, VD_Node *, root->children_count);
        {U64 child_index = 0;
            for(VD_Node *child = root->first; 0 != child; child = child->next)
            {
                ptrs[child_index] = child;
                child_index += 1;
            }}
        for(;;)
        {
            B32 is_sorted = True;
            for(U64 child_index = 0; child_index + 1 < root->children_count; child_index += 1)
            {
                F32 a = Centre2F(ptrs[child_index]->bounds).elements[parent->children_layout_axis];
                F32 b = Centre2F(ptrs[child_index + 1]->bounds).elements[parent->children_layout_axis];
                if(a > b)
                {
                    VD_Node *temp = ptrs[child_index];
                    ptrs[child_index] = ptrs[child_index + 1];
                    ptrs[child_index + 1] = temp;
                    is_sorted = False;
                }
            }
            if(is_sorted)
            {
                root->first = root->last = ptrs[0];
                root->first->prev = 0;
                root->last->next = 0;
                for(U64 child_index = 1; child_index < root->children_count; child_index += 1)
                {
                    root->last->next = ptrs[child_index];
                    root->last->next->prev = root->last;
                    root->last->next->next = 0;
                    root->last = root->last->next;
                }
                break;
            }
        }
        ArenaTempEnd(scratch);
    }
    
    
    for(VD_Node *child = root->first; 0 != child; child = child->next)
    {
        if(1 == child->children_count)
        {
            F32 f = Measure2F(child->bounds).elements[parent->children_layout_axis] / total_children_size;
            UI_Node *result = PushArray(arena, UI_Node, 1);
            result->flags |= UI_Flags_DrawFill;
            result->flags |= UI_Flags_DrawStroke;
            result->flags |= UI_Flags_DrawDropShadow;
            result->fg = ColFromHex(0x07364288);
            result->size[parent->children_layout_axis] = UI_SizePct(f, 0.0f);
            result->size[!parent->children_layout_axis] = UI_SizeFill();
            result->growth = -8.0f*depth;
            result->shadow_radius = 8.0f;
            result->corner_radius = 5.3f;
            result->bg = ColFromHex(0xfdf6e3ff);
            U32 cols[] =
            {
                0xcb4b16ff,
                0xdc322fff,
                0xd33682ff,
                0x6c71c4ff,
                0x268bd2ff,
                0x2aa198ff,
                0x859900ff,
                0xb58900ff,
            };
            result->fg = ColFromHex(cols[depth % ArrayCount(cols)]);
            result->stroke_width = 2.0f;
            result->scale = 1.0f;
            result->texture = R2D_TextureNil();
            result->children_layout_axis = Axis2_X;
            UI_NodeInsert(result, parent);
            
            UITreeFromVisideskTree_(arena, child->first, result, depth + 1);
        }
    }
}

Function UI_Node *
UITreeFromVisideskTree(Arena *arena, VD_Node *root)
{
    UI_Node *result = PushArray(arena, UI_Node, 1);
    result->size[Axis2_X] = UI_SizePx(BitmapWidth, 1.0f);
    result->size[Axis2_Y] = UI_SizePx(BitmapHeight, 1.0f);
    result->scale = 1.0f;
    result->texture = R2D_TextureNil();
    
    UITreeFromVisideskTree_(arena, root, result, 0);
    
    return result;
}

Function void
S8ListFromUITree(Arena *arena, UI_Node *root, S8List *result, U64 depth)
{
    for(U64 i = 0; i < depth; i += 1) { S8ListAppend(arena, result, S8("    ")); }
    
    if(0 != root->parent)
    {
        S8ListAppendFmt(arena, result, "%s(UI_SizePct(%.3ff, 0.0f)) ",
                        (Axis2_X == root->parent->children_layout_axis) ? "UI_Width" : "UI_Height",
                        root->size[root->parent->children_layout_axis].f);
    }
    
    if(0 == root->first)
    {
        S8ListAppend(arena, result, S8("UI_NodeMake(UI_Flags_DrawStroke, S8(\"\"));\n"));
    }
    else
    {
        if(Axis2_X == root->children_layout_axis)
        {
            S8ListAppend(arena, result, S8("UI_Row() UI_Height(UI_SizeFill())\n"));
            for(U64 i = 0; i < depth; i += 1) { S8ListAppend(arena, result, S8("    ")); }
            S8ListAppend(arena, result, S8("{\n"));
        }
        else
        {
            S8ListAppend(arena, result, S8("UI_Column() UI_Width(UI_SizeFill())\n"));
            for(U64 i = 0; i < depth; i += 1) { S8ListAppend(arena, result, S8("    ")); }
            S8ListAppend(arena, result, S8("{\n"));
        }
        for(UI_Node *child = root->first; 0 != child; child = child->next)
        {
            S8ListFromUITree(arena, child, result, depth + 1);
        }
        for(U64 i = 0; i < depth; i += 1) { S8ListAppend(arena, result, S8("    ")); }
        S8ListAppend(arena, result, S8("}\n"));
    }
}

Function void
WindowHookDraw(OpaqueHandle window)
{
    DRW_List drw = {0};
    
    F32 s = G_WindowScaleFactorGet(window);
    Range2F client_rect = G_WindowClientRectGet(window);
    Arena *frame_arena = G_WindowFrameArenaGet(window);
    G_EventQueue *events = G_WindowEventQueueGet(window);
    V2F mouse_position = G_WindowMousePositionGet(window);
    
    V2F canvas_size = Scale2F(V2F(BitmapWidth, BitmapHeight), 0.5f);
    /*
    V2F max_canvas_size = Scale2F(client_rect.max, 0.7f / s);
    while(2.0f*canvas_size.x < max_canvas_size.x &&
          2.0f*canvas_size.y < max_canvas_size.y)
    {
        canvas_size = Scale2F(canvas_size, 2.0f);
    }
*/
    V2F canvas_pos = Scale2F(Sub2F(Measure2F(client_rect), Mul2F(canvas_size, V2F(2.0f, 2.0f))), 0.5f);
    Range2F canvas_rect = RectMake2F(canvas_pos, canvas_size);
    R2D_Quad canvas_quad =
    {
        .dst = canvas_rect,
        .src = r2d_entire_texture,
        .mask = client_rect,
        .fill_colour = U4F(1.0f),
        .corner_radius = 8.0f,
        .edge_softness = 1.0f,
    };
    DRW_Sprite(frame_arena, &drw, canvas_quad, bitmap_texture);
    
    Persist enum
    {
        PenUp,
        PenDown,
        PenHolding,
    } pen = 0;
    if(HasPoint2F(canvas_rect, mouse_position) && G_EventQueueHasKeyDown(events, G_Key_MouseButtonLeft, G_ModifierKeys_Any, True))
    {
        pen = PenDown;
    }
    if(G_EventQueueHasKeyUp(events, G_Key_MouseButtonLeft, G_ModifierKeys_Any, True))
    {
        pen = PenUp;
        
        ArenaClear(tree_arena);
        visidesk_tree = VD_TreeFromBitmap(tree_arena, &bitmap, V2I(0, 0));
        ui_tree = UITreeFromVisideskTree(tree_arena, visidesk_tree);
        ArenaTemp scratch = OS_TCtxScratchBegin(0, 0);
        S8List ui_string_list = {0};
        S8ListFromUITree(scratch.arena, ui_tree, &ui_string_list, 0);
        ui_string = S8ListJoin(tree_arena, ui_string_list);
        ArenaTempEnd(scratch);
    }
    
    if(pen)
    {
        ArenaTemp scratch = OS_TCtxScratchBegin(0, 0);
        
        Range2F bitmap_range = Range2F(V2F(0.0f, 0.0f), V2F(BitmapWidth, BitmapHeight));
        V2F bitmap_mouse_position = Clamp2F(RangeMap2F(canvas_rect, bitmap_range, mouse_position), bitmap_range.min, bitmap_range.max);
        V2I pixel_position = V2I(Round1F(bitmap_mouse_position.x), Round1F(bitmap_mouse_position.y));
        
        Persist V2I prev_pixel_position;
        if(PenDown == pen)
        {
            prev_pixel_position = pixel_position;
            pen = PenHolding;
        }
        
        I32 x0 = pixel_position.x;
        I32 y0 = pixel_position.y;
        I32 x1 = prev_pixel_position.x;
        I32 y1 = prev_pixel_position.y;
        I32 dx = Abs1I(x1 - x0);
        I32 sx = (x1 < x0) ? -1 : 1;
        I32 dy = -Abs1I(y1 - y0);
        I32 sy = (y1 < y0) ? -1 : 1;
        
        I32 e = dx + dy;
        
        for(;;)
        {
            I32 r = 1;
            for(I32 cy = y0 - r; cy < y0 + r; cy += 1)
            {
                for(I32 cx = x0 - r; cx < x0 + r; cx += 1)
                {
                    I32 x_dist = cx - x0;
                    I32 y_dist = cy - y0;
                    I32 dist_2 = x_dist*x_dist + y_dist*y_dist;
                    if(dist_2 <= r*r)
                    {
                        bitmap.pixels[cx + cy*BitmapWidth] = 254;
                    }
                }
            }
            
            
            if(x0 == x1 &&
               y0 == y1)
            {
                break;
            }
            
            I32 e2 = 2*e;
            
            if(e2 >= dy)
            {
                if(x0 == x1)
                {
                    break;
                }
                else
                {
                    e += dy;
                    x0 += sx;
                }
            }
            if(e2 <= dx)
            {
                if(y0 == y1)
                {
                    break;
                }
                else
                {
                    e += dx;
                    y0 += sy;
                }
            }
        }
        
        prev_pixel_position = pixel_position;
        
        Pixel *pixels = PushArray(scratch.arena, Pixel, BitmapWidth*BitmapHeight);
        for(U64 pixel_index = 0; pixel_index < BitmapWidth*BitmapHeight; pixel_index += 1)
        {
            if(bitmap.pixels[pixel_index])
            {
                pixels[pixel_index].val = 0xff362b00;
            }
            else
            {
                pixels[pixel_index].val = 0xffe3f6fd;
            }
        }
        R2D_TexturePixelsSet(bitmap_texture, pixels);
        
        ArenaTempEnd(scratch);
    }
    
    if(0 != ui_tree)
    {
        ui_tree->size[Axis2_X] = UI_SizeNone();
        ui_tree->size[Axis2_Y] = UI_SizeNone();
        ui_tree->rect = Grow2F(canvas_rect, U2F(8.0f));
        ui_tree->rect.min.x += canvas_size.x + 16.0f;
        ui_tree->rect.max.x += canvas_size.x + 16.0f;
        ui_tree->clipped_rect = ui_tree->rect;
        ui_tree->computed_size = canvas_size;
        
        for(Axis2 axis = 0; axis < Axis2_MAX; axis += 1)
        {
            UI_LayoutPassIndependentSizes(ui_tree, axis);
            UI_LayoutPassUpwardsDependentSizes(ui_tree, axis);
            UI_LayoutPassDownwardsDependentSizes(ui_tree, axis);
            UI_LayoutPassSolveViolations(ui_tree, axis);
        }
        UI_LayoutPassComputeRelativeRects(ui_tree);
        UI_AnimationPass(ui_tree, (UI_Key[1]){0}, (UI_Key[1]){0});
        UI_LayoutPassComputeFinalRects(ui_tree);
        UI_RenderPass(ui_tree, client_rect, window, &drw);
    }
    
    FONT_PreparedText pt = FONT_PrepareS8(frame_arena, 0, font, 28, ui_string);
    DRW_Text(frame_arena, &drw, &pt, Add2F(canvas_pos, V2F(0.0f, canvas_size.y + 32.0f)), ColFromHex(0x362b00ff), client_rect);
    
    DRW_ListSubmit(window, &drw);
}

Function void
WindowHookClose(OpaqueHandle window)
{
    
}

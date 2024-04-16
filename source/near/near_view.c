
Function V_Proc *
V_DefaultProcFromFilename(S8 filename)
{
    V_Proc *result = V_FileProperties;
    
    FI_Node *info = FI_InfoFromFilename(filename);
    
    if(info->props.flags & OS_FilePropertiesFlags_IsDirectory)
    {
        result = V_Canvas;
    }
    else if(!R2D_TextureIsNil(info->texture))
    {
        result = V_Image;
    }
    
    return result;
}

Function V_CanvasPerFileState *
V_CanvasPerFileStateFromFilename(Arena *arena, S8 filename)
{
    V_CanvasPerFileState *result = DB_ReadStruct(arena, filename, S8("canvas"), V_CanvasPerFileState);
    if(0.0f == result->zoom)
    {
        F32 padding = 8.0f;
        result->zoom = 1.0f;
        result->children_layout = U2F(padding);
        
        S8 parent_filename = FilenamePop(filename);
        if(parent_filename.len < filename.len)
        {
            ArenaTemp scratch = OS_TCtxScratchBegin(&arena, 1);
            V_CanvasPerFileState *parent_canvas = V_CanvasPerFileStateFromFilename(scratch.arena, parent_filename);
            V2F dimensions = V2F(5.0f*V_StandardRowHeight, 150.0f);
            result->rect = RectMake2F(parent_canvas->children_layout, dimensions);
            parent_canvas->children_layout.y += dimensions.y + padding;
            DB_WriteStruct(parent_filename, S8("canvas"), *parent_canvas);
            ArenaTempEnd(scratch);
        }
        
        DB_WriteStruct(filename, S8("canvas"), *result);
    }
    return result;
}

Function Range2F
V_CanvasRectFromFile(V_CanvasPerFileState *file, V2F camera_position)
{
    F32 s = UI_ScalePeek();
    Range2F result =
        Range2F(Scale2F(Sub2F(file->rect.min, camera_position), s),
                Scale2F(Sub2F(file->rect.max, camera_position), s));
    return result;
}

Function V2F
V_CanvasWorldFromScreen(V2F position, V2F origin, V2F camera_position, F32 scale)
{
    V2F result = Add2F(Scale2F(Sub2F(position, origin), 1.0f / scale), camera_position);
    return result;
}

Function void
V_Default(S8 root, U64 depth, AppCmd **cmds)
{
    V_Proc *proc = V_DefaultProcFromFilename(root);
    proc(root, depth, cmds);
}

Function void
V_Canvas(S8 root, U64 depth, AppCmd **cmds)
{
    ArenaTemp scratch = OS_TCtxScratchBegin(0, 0);
    V2F mouse_position = G_WindowMousePositionGet(ui->window);
    
    V_CanvasPerFileState *root_canvas = V_CanvasPerFileStateFromFilename(scratch.arena, root);
    
    UI_Width(UI_SizeFill()) UI_Height(UI_SizeFill())
        UI_Column()
    {
        UI_Width(UI_SizeFromText(4.0f, 1.0f)) UI_Height(UI_SizeFromText(4.0f, 1.0f)) UI_Font(ui_font_bold)
            UI_Label(FilenameLast(root));
        
        F32 depth_zoom = Pow1F(0.3f, depth);
        F32 children_scale = UI_ScalePeek()*root_canvas->zoom*depth_zoom;
        
        UI_Node *bg = UI_NodeMakeFromFmt(UI_Flags_Pressable, "%.*s bg", FmtS8(root));
        UI_Parent(bg)
        {
            UI_ScalePush(children_scale);
            
            if(depth < V_CanvasMaxDepth)
                UI_Width(UI_SizeNone()) UI_Height(UI_SizeNone())
            {
                U64 next_depth = depth + 1;
                FI_Node *info = FI_InfoFromFilename(root);
                
                for(S8ListForEach(info->children_filenames, child_filename))
                {
                    V_CanvasPerFileState *child_canvas = V_CanvasPerFileStateFromFilename(scratch.arena, child_filename->string);
                    
                    V2F child_position = Sub2F(child_canvas->rect.min, root_canvas->camera_position);
                    V2F child_dimensions = Dimensions2F(child_canvas->rect);
                    
                    Range2F screenspace_child_rect =
                        Range2F(Add2F(bg->rect.min, Scale2F(Sub2F(child_canvas->rect.min, root_canvas->camera_position), UI_ScalePeek())),
                                Add2F(bg->rect.min, Scale2F(Sub2F(child_canvas->rect.max, root_canvas->camera_position), UI_ScalePeek())));
                    
                    B32 is_active = False;
                    {
                        UI_NodeStrings strings = UI_TextAndIdFromString(child_filename->string);
                        UI_Key key = UI_KeyFromIdAndSaltStack(strings.id, ui->salt_stack);
                        UI_Node *active = UI_NodeLookup(&ui->active);
                        is_active = -UI_NodeIsChildOf(active, &key);
                    }
                    
                    if(is_active || IsOverlapping2F(screenspace_child_rect, bg->rect))
                    {
                        UI_SetNextFloatingPosition(child_position);
                        UI_SetNextWidth(UI_SizePx(child_dimensions.x, 1.0f));
                        UI_SetNextHeight(UI_SizePx(child_dimensions.y, 1.0f));
                        UI_Node *child_panel =
                            UI_NodeMake(UI_Flags_DrawFill |
                                        UI_Flags_DrawDropShadow |
                                        UI_Flags_Floating |
                                        UI_Flags_DrawHotStyle |
                                        UI_Flags_DrawActiveStyle |
                                        UI_Flags_AnimateHot |
                                        UI_Flags_AnimateActive |
                                        UI_Flags_Pressable,
                                        child_filename->string);
                        UI_Parent(child_panel)
                        {
                            F32 resizer_size = 12.0f;
                            UI_SetNextFloatingPosition(Sub2F(child_dimensions, U2F(resizer_size)));
                            UI_SetNextWidth(UI_SizePx(resizer_size, 1.0f));
                            UI_SetNextHeight(UI_SizePx(resizer_size, 1.0f));
                            UI_SetNextHotCursorKind(G_CursorKind_Resize);
                            UI_Node *resizer =
                                UI_NodeMakeFromFmt(UI_Flags_SetCursorKind |
                                                   UI_Flags_Floating |
                                                   UI_Flags_Pressable,
                                                   "%.*s resizer",
                                                   FmtS8(child_filename->string));
                            UI_Use resizer_use = UI_UseFromNode(resizer);
                            if(resizer_use.is_dragging)
                            {
                                V2F new_rect_max = V_CanvasWorldFromScreen(mouse_position, bg->rect.min, root_canvas->camera_position, children_scale);
                                child_canvas->rect.max = Maxs2F(new_rect_max, Add2F(child_canvas->rect.min, V2F(64.0f, 64.0f)));
                                DB_WriteStruct(child_filename->string, S8("canvas"), *child_canvas);
                            }
                            
                            // TODO(tbt): customisable views for children
                            V_Default(child_filename->string, next_depth, cmds);
                        }
                        
                        UI_Use child_use = UI_UseFromNode(child_panel);
                        if(child_use.is_dragging)
                        {
                            V2F dimensions = Dimensions2F(child_canvas->rect);
                            V2F corrected_mouse_position = Sub2F(mouse_position, child_use.initial_mouse_relative_position);
                            V2F position = V_CanvasWorldFromScreen(corrected_mouse_position, bg->rect.min, root_canvas->camera_position, children_scale);
                            child_canvas->rect = RectMake2F(position, dimensions);
                            DB_WriteStruct(child_filename->string, S8("canvas"), *child_canvas);
                        }
                        else if(child_use.is_double_clicked)
                        {
                            AppCmd *cmd = AppCmdAlloc();
                            cmd->kind = AppCmdKind_BuildPathPush;
                            cmd->filename = child_filename->string;
                            cmd->next = *cmds;
                            *cmds = cmd;
                        }
                    }
                }
            }
            
            UI_ScalePop();
        }
        
        UI_Use use = UI_UseFromNode(bg);
        if(use.is_dragging)
        {
            root_canvas->camera_position = Sub2F(root_canvas->camera_position, Scale2F(use.drag_delta, 1.0f / children_scale));
            DB_WriteStruct(root, S8("canvas"), *root_canvas);
        }
    }
}

Function void
V_FileProperties(S8 root, U64 depth, AppCmd **cmds)
{
    ArenaTemp scratch = OS_TCtxScratchBegin(0, 0);
    
    FI_Node *info = FI_InfoFromFilename(root);
    
    F32 s = UI_ScalePeek();
    
    UI_Node *panel = UI_ParentPeek();
    F32 window_height = (panel->rect.max.y - panel->rect.min.y) / s;
    
    UI_SetNextWidth(UI_SizeFill());
    UI_SetNextHeight(UI_SizeFill());
    UI_Column()
        UI_Width(UI_SizeFromText(4.0f, 1.0f))
        UI_Height(UI_SizePx(V_StandardRowHeight, 1.0f))
    {
        UI_Font(ui_font_bold) UI_Label(FilenameLast(root));
        
        if(info->props.size < Kilobytes(1))
        {
            UI_LabelFromFmt("%llu bytes", info->props.size);
        }
        else if(info->props.size < Megabytes(1))
        {
            UI_LabelFromFmt("%llu KiB", info->props.size / 1024);
        }
        else if(info->props.size < Gigabytes(1))
        {
            UI_LabelFromFmt("%llu MiB", info->props.size / (1024*1024));
        }
        else
        {
            UI_LabelFromFmt("%llu GiB", info->props.size / (1024*1024*1024));
        }
        
        UI_SetNextWidth(UI_SizeSum(1.0f));
        UI_SetNextWidth(UI_SizeSum(1.0f));
        UI_Row()
        {
            if(info->props.access_flags & DataAccessFlags_Read) UI_Label(S8("R"));
            if(info->props.access_flags & DataAccessFlags_Write) UI_Label(S8("W"));
            if(info->props.access_flags & DataAccessFlags_Execute) UI_Label(S8("X"));
        }
        
        UI_Label(S8FromDenseTime(scratch.arena, info->props.creation_time));
        UI_Label(S8FromDenseTime(scratch.arena, info->props.write_time));
    }
    
    ArenaTempEnd(scratch);
}

Function void
V_Image(S8 root, U64 depth, AppCmd **cmds)
{
    ArenaTemp scratch = OS_TCtxScratchBegin(0, 0);
    
    FI_Node *info = FI_InfoFromFilename(root);
    
    F32 s = UI_ScalePeek();
    
    UI_Node *panel = UI_ParentPeek();
    V2F panel_dimensions = Scale2F(Dimensions2F(panel->rect), 1.0f / s);
    
    UI_Width(UI_SizeFill()) UI_Height(UI_SizeFill()) UI_Column()
    {
        UI_Width(UI_SizeFromText(4.0f, 1.0f)) UI_Height(UI_SizePx(V_StandardRowHeight, 1.0f)) UI_Font(ui_font_bold)
            UI_Label(FilenameLast(root));
        
        F32 padding = 4.0f;
        
        V2F image_dimensions = {0};
        {
            V2I texture_dimensions = R2D_TextureDimensionsGet(info->texture);
            F32 texture_aspect_ratio = (F32)texture_dimensions.y / texture_dimensions.x;
            
            V2F available = Sub2F(panel_dimensions, V2F(0.0f, V_StandardRowHeight));
            
            image_dimensions = Sub2F((available.x < available.y / texture_aspect_ratio ?
                                      V2F(available.x, available.x * texture_aspect_ratio) : 
                                      V2F(available.y / texture_aspect_ratio, available.y)),
                                     U2F(2.0f*padding));
        }
        
        UI_Pad(UI_SizePx(padding, 1.0f)) UI_Row() UI_Pad(UI_SizeFill()) UI_Pad(UI_SizePx(padding, 1.0f))
        {
            UI_Texture(info->texture) UI_BgCol(U4F(1.0f)) UI_Width(UI_SizePx(image_dimensions.x, 1.0f)) UI_Height(UI_SizePx(image_dimensions.y, 1.0f))
                UI_NodeMake(UI_Flags_DrawFill | UI_Flags_DrawDropShadow, S8(""));
        }
    }
    
    ArenaTempEnd(scratch);
}

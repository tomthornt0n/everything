
Function void
UI_Label(S8 text)
{
    Arena *frame_arena = G_WindowFrameArenaGet(ui->window);
    UI_Node *node = PushArray(frame_arena, UI_Node, 1);
    UI_NodeUpdate(node, UI_Flags_DrawText | UI_Flags_DrawHotStyle | UI_Flags_DrawActiveStyle, S8Clone(frame_arena, text));
    UI_NodeInsert(node, UI_ParentPeek());
}

Function void
UI_LabelFromFmtV(char *fmt, va_list args)
{
    ArenaTemp scratch = OS_TCtxScratchBegin(0, 0);
    S8 text = S8FromFmtV(scratch.arena, fmt, args);
    UI_Label(text);
    ArenaTempEnd(scratch);
}

Function void
UI_LabelFromFmt(char *fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    UI_LabelFromFmtV(fmt, args);
    va_end(args);
}

Function void
UI_LabelFromIcon(char icon)
{
    UI_Font(ui_font_icons) UI_TextTruncSuffix(S8("")) UI_LabelFromFmt("%c", icon);
}

Function void
UI_ColourPickerPixelsMake(Pixel *result, V2I dimensions, I32 hue_bar_width, F32 hue)
{
    for(int y = 0; y < dimensions.y; y += 1)
    {
        for(int x = 0; x < dimensions.x - hue_bar_width; x += 1)
        {
            V4F hsv = V4F(hue, (F32)x / (dimensions.x - hue_bar_width), (F32)y / dimensions.y, 1.0f);
            V4F rgb = RGBFromHSV(hsv);
            result[x + y*dimensions.x] = (Pixel){ .r = rgb.r*255, .g = rgb.g*255, .b = rgb.b*255, .a = 255, };
        }
    }
    for(int y = 0; y < dimensions.y; y += 1)
    {
        V4F hsv = V4F(360.0f*y / dimensions.y, 0.95f, 0.95f, 1.0f);
        V4F rgb = RGBFromHSV(hsv);
        Pixel pixel = { .r = rgb.r*255, .g = rgb.g*255, .b = rgb.b*255, .a = 255, };
        for(int x = 0; x < hue_bar_width; x += 1)
        {
            result[dimensions.x - x - 1 + y*dimensions.x] = pixel;
        }
    }
}

Function OpaqueHandle
UI_TextureFromColourPicker(F32 hue)
{
    UI_ColourPickerTextureCache *item = 0;
    
    I32 hue_i = Round1F(hue);
    
    UI_ColourPickerTextureCache *lru = 0;
    for(U64 i = 0; i < ArrayCount(ui_colour_picker_texture_cache); i += 1)
    {
        if(0 == lru || ui_colour_picker_texture_cache[i].last_touched_frame_index < lru->last_touched_frame_index)
        {
            lru = &ui_colour_picker_texture_cache[i];
        }
        
        if(R2D_TextureIsNil(ui_colour_picker_texture_cache[i].texture) ||
           ui_colour_picker_texture_cache[i].hue == hue_i)
        {
            item = &ui_colour_picker_texture_cache[i];
            break;
        }
    }
    
    if(0 == item)
    {
        item = lru;
    }
    
    if(R2D_TextureIsNil(item->texture))
    {
        ArenaTemp scratch = OS_TCtxScratchBegin(0, 0);
        V2I dimensions = UI_ColourPickerTextureDimensions;
        Pixel *pixels = PushArray(scratch.arena, Pixel, dimensions.x*dimensions.y);
        UI_ColourPickerPixelsMake(pixels, dimensions, UI_ColourPickerHueBarWidth, hue_i);
        item->texture = R2D_TextureAlloc(pixels, dimensions);
        ArenaTempEnd(scratch);
    }
    else if(item->hue != hue_i)
    {
        ArenaTemp scratch = OS_TCtxScratchBegin(0, 0);
        V2I dimensions = UI_ColourPickerTextureDimensions;
        Pixel *pixels = PushArray(scratch.arena, Pixel, dimensions.x*dimensions.y);
        UI_ColourPickerPixelsMake(pixels, dimensions, UI_ColourPickerHueBarWidth, hue_i);
        R2D_TexturePixelsSet(item->texture, pixels);
        ArenaTempEnd(scratch);
    }
    
    item->hue = hue_i;
    item->last_touched_frame_index = ui->frame_index;
    
    OpaqueHandle result = item->texture;
    return result;
}

#if 0

// TODO(tbt): port from UI_Flags_FixedRelativeRect -> UI_Flags_Floating

Function UI_Use
UI_ColourPicker(S8 string, V4F *colour)
{
    ArenaTemp scratch = OS_TCtxScratchBegin(0, 0);
    
    V2I dimensions = UI_ColourPickerTextureDimensions;
    
    UI_SetNextBgCol(U4F(1.0f));
    UI_Node *node = UI_NodeMake(UI_Flags_DrawFill |
                                UI_Flags_DrawDropShadow |
                                UI_Flags_DoNotMask |
                                UI_Flags_Pressable,
                                string);
    
    V4F hsv = HSVFromRGB(*colour);
    if(hsv.h == 0.0f && (hsv.s == 0.0f || hsv.v == 0.0f))
    {
        hsv.h = node->user_t;
    }
    
    UI_Use use = UI_UseFromNode(node);
    
    if(use.is_pressed || use.is_dragging)
    {
        V2F mouse_offset = Sub2F(G_WindowMousePositionGet(ui->window), node->rect.min);
        V2F p = Clamp2F(Div2F(mouse_offset, Measure2F(node->rect)), U2F(0.0f), U2F(1.0f));
        
        if(p.x > (F32)(dimensions.x - UI_ColourPickerHueBarWidth) / dimensions.x)
        {
            hsv.h = 360.0f*p.y;
        }
        else
        {
            hsv.s = p.x;
            hsv.v = p.y;
        }
    }
    
    node->texture = UI_TextureFromColourPicker(hsv.h);
    
    *colour = RGBFromHSV(hsv);
    
    UI_Width(UI_SizeNone()) UI_Height(UI_SizeNone()) UI_Parent(node)
    {
        // NOTE(tbt): hue indicator
        {
            V2F layout_size = Measure2F(node->rect);
            V2F center = V2F(layout_size.x*(dimensions.x - 0.5f*UI_ColourPickerHueBarWidth) / dimensions.x,
                             layout_size.y*hsv.h / 360.0f);
            V2F half_span = V2F(10.0f, 4.0f);
            
            UI_SetNextFixedRect(Range2F(Sub2F(center, half_span), Add2F(center, half_span)));
            UI_SetNextBgCol(RGBFromHSV(V4F(hsv.h, 1.0f, 1.0f, 1.0f)));
            UI_SetNextFgCol(ColFromHex(0xeceff4ff));
            UI_SetNextStrokeWidth(1.0f);
            UI_SetNextCornerRadius(4.0f);
            UI_Node *hue_indicator =
                UI_NodeMake(UI_Flags_DrawFill |
                            UI_Flags_DrawStroke |
                            UI_Flags_DrawDropShadow |
                            UI_Flags_FixedRelativeRect,
                            S8(""));
            
        }
        
        // NOTE(tbt): saturation and value indicator
        {
            V2F layout_size = Measure2F(node->rect);
            V2F center = V2F(layout_size.x*hsv.s, layout_size.y*hsv.v);
            V2F half_span = U2F(8.0f);
            
            UI_SetNextFixedRect(Range2F(Sub2F(center, half_span), Add2F(center, half_span)));
            UI_SetNextBgCol(*colour);
            UI_SetNextFgCol(ColMix(ColFromHex(0xeceff4ff), ColFromHex(0x2e3440ff), hsv.v*(1.0f - hsv.s)));
            UI_SetNextStrokeWidth(1.0f);
            UI_SetNextCornerRadius(8.0f);
            UI_Node *saturation_and_value_indicator =
                UI_NodeMake(UI_Flags_DrawFill |
                            UI_Flags_DrawStroke |
                            UI_Flags_DrawDropShadow |
                            UI_Flags_FixedRelativeRect,
                            S8(""));
        }
    }
    
    ArenaTempEnd(scratch);
    
    return use;
}

#endif

Function UI_Use
UI_Slider1F(S8 string, F32 *value, Range1F range, F32 thumb_size)
{
    UI_Use result = {0};
    
    UI_Node *track = UI_NodeMake(UI_Flags_DrawFill | UI_Flags_DrawDropShadow | UI_Flags_Pressable | UI_Flags_Scrollable | UI_Flags_DoNotMask, string);
    
    F32 s = UI_ScalePeek();
    
    V2F dim = Scale2F(track->computed_size, 1.0f / s);
    Axis2 layout = dim.x > dim.y ? Axis2_X : Axis2_Y;
    
    track->children_layout_axis = layout;
    
    thumb_size = Clamp1F(thumb_size, 1.0f, dim.elements[layout] - 1.0f);
    
    F32 d = dim.elements[layout] - thumb_size;
    
    F32 thumb_width = (Axis2_X == layout) ? thumb_size : dim.elements[!layout];
    F32 thumb_height = (Axis2_Y == layout) ? thumb_size : dim.elements[!layout];
    
    if(range.max - range.min >= 0.001f)
    {
        UI_Parent(track) UI_Salt(string)
            UI_Height(UI_SizePx(thumb_height, 1.0f))
            UI_Width(UI_SizePx(thumb_width, 1.0f))
        {
            F32 p = (*value - range.min) / (range.max - range.min);
            UI_Spacer(UI_SizePx(p*d, 1.0f));
            
            UI_SetNextBgCol(ColFromHex(0x4c566aff));
            UI_SetNextCornerRadius(0.5f*dim.elements[!layout]);
            UI_SetNextHotBgCol(ColFromHex(0x81a1c1ff));
            UI_SetNextHotGrowth(2.0f);
            UI_SetNextHotCornerRadius(0.5f*dim.elements[!layout] + 2.0f);
            UI_SetNextActiveBgCol(ColFromHex(0x88c0d0ff));
            UI_SetNextActiveGrowth(2.0f);
            UI_SetNextActiveCornerRadius(0.5f*dim.elements[!layout] + 2.0f);
            UI_SetNextActiveCursorKind(G_CursorKind_Hidden);
            UI_Node *thumb = UI_NodeMake(UI_Flags_DrawFill |
                                         UI_Flags_DrawDropShadow |
                                         UI_Flags_DrawHotStyle |
                                         UI_Flags_DrawActiveStyle |
                                         UI_Flags_Pressable |
                                         UI_Flags_NavDefaultFilter |
                                         UI_Flags_CursorPositionToCenter |
                                         UI_Flags_AnimateHot |
                                         UI_Flags_AnimateActive |
                                         UI_Flags_SetCursorKind,
                                         S8("thumb"));
            
            result = UI_UseFromNode(thumb);
            UI_Use track_use = UI_UseFromNode(track);
            
            if(result.is_dragging || track_use.is_pressed)
            {
                F32 mouse_position = G_WindowMousePositionGet(ui->window).elements[layout];
                F32 p = (mouse_position - result.initial_mouse_relative_position.elements[layout] - track->rect.min.elements[layout]) / (d*s);
                *value = range.min + p*(range.max - range.min);
                result.is_edited = True;
            }
            else if(track->flags & UI_Flags_KeyboardFocused)
            {
                *value += (range.max - range.min)*track_use.scroll_delta.elements[layout] / 100.0f;
            }
        }
    }
    else
    {
        track->flags |= UI_Flags_NavDefaultFilter;
    }
    
    *value = Clamp1F(*value, range.min, range.max);
    
    return result;
}

Function UI_Use
UI_ScrollBar(S8 string, UI_Node *node_for, Axis2 axis)
{
    UI_Use result = {0};
    
    F32 overflow_in_pixels = node_for->overflow.elements[axis];
    
    UI_Node *track = UI_NodeMake(UI_Flags_DrawFill | UI_Flags_DrawDropShadow | UI_Flags_Pressable | UI_Flags_Scrollable | UI_Flags_DoNotMask, string);
    
    F32 s = UI_ScalePeek();
    V2F dim = Scale2F(track->computed_size, 1.0f / s);
    F32 overflow = overflow_in_pixels / s;
    F32 window_size = node_for->computed_size.elements[axis] / s;
    F32 track_size = dim.elements[axis];
    F32 thumb_size = Clamp1F(track_size*window_size / (window_size + overflow), 16.0f, dim.elements[axis] - 1.0f);
    
    track->children_layout_axis = axis;
    
    F32 d = dim.elements[axis] - thumb_size;
    
    F32 thumb_width = (Axis2_X == axis) ? thumb_size : dim.elements[!axis];
    F32 thumb_height = (Axis2_Y == axis) ? thumb_size : dim.elements[!axis];
    
    F32 scroll = Clamp1F(-node_for->target_view_offset.elements[axis], 0.0f, overflow_in_pixels);
    
    if(overflow_in_pixels >= 1.0f)
    {
        UI_Parent(track) UI_Salt(string)
            UI_Height(UI_SizePx(thumb_height, 1.0f))
            UI_Width(UI_SizePx(thumb_width, 1.0f))
        {
            F32 p = scroll / overflow_in_pixels;
            UI_Spacer(UI_SizePx(p*d, 1.0f));
            
            UI_SetNextBgCol(ColFromHex(0x4c566aff));
            UI_SetNextCornerRadius(0.5f*dim.elements[!axis]);
            UI_SetNextHotBgCol(ColFromHex(0x81a1c1ff));
            UI_SetNextHotGrowth(2.0f);
            UI_SetNextHotCornerRadius(0.5f*dim.elements[!axis] + 2.0f);
            UI_SetNextActiveBgCol(ColFromHex(0x88c0d0ff));
            UI_SetNextActiveGrowth(2.0f);
            UI_SetNextActiveCornerRadius(0.5f*dim.elements[!axis] + 2.0f);
            UI_SetNextActiveCursorKind(G_CursorKind_Hidden);
            UI_Node *thumb = UI_NodeMake(UI_Flags_DrawFill |
                                         UI_Flags_DrawDropShadow |
                                         UI_Flags_DrawHotStyle |
                                         UI_Flags_DrawActiveStyle |
                                         UI_Flags_Pressable |
                                         UI_Flags_NavDefaultFilter |
                                         UI_Flags_CursorPositionToCenter |
                                         UI_Flags_AnimateHot |
                                         UI_Flags_AnimateActive |
                                         UI_Flags_SetCursorKind,
                                         S8("thumb"));
            
            result = UI_UseFromNode(thumb);
            UI_Use track_use = UI_UseFromNode(track);
            
            if(result.is_dragging || track_use.is_pressed)
            {
                F32 mouse_position = G_WindowMousePositionGet(ui->window).elements[axis];
                F32 p = Clamp1F((mouse_position - result.initial_mouse_relative_position.elements[axis] - track->rect.min.elements[axis]) / (d*s), 0.0f, 1.0f);
                node_for->target_view_offset.elements[axis] = -p*overflow_in_pixels;
                node_for->view_offset.elements[axis] = node_for->target_view_offset.elements[axis];
                result.is_edited = True;
            }
            else if(track->flags & UI_Flags_KeyboardFocused)
            {
                node_for->target_view_offset.elements[axis] -= overflow_in_pixels*track_use.scroll_delta.elements[axis] / 100.0f;
            }
        }
    }
    else
    {
        track->flags |= UI_Flags_NavDefaultFilter;
    }
    
    return result;
}

Function void
UI_TextFieldMouseInput(S8 string, UI_Use use, V2F text_pos, FONT_PreparedText *pt, Range1U64 *selection)
{
    if(use.is_left_down || use.is_double_clicked)
    {
        U64 clicked_char_index = FONT_PreparedTextNearestIndexFromPoint(pt, text_pos, G_WindowMousePositionGet(ui->window));
        U64 clicked_byte_index = S8ByteIndexFromCharIndex(string, clicked_char_index);
        selection->cursor = clicked_byte_index;
        if(!(G_WindowModifiersMaskGet(ui->window) & G_ModifierKeys_Shift))
        {
            selection->mark = clicked_byte_index;
        }
        if(use.is_double_clicked)
        {
            int dir = (selection->cursor >= selection->mark) ? +1 : -1;
            while(HasPoint1U64(Range1U64(0, string.len), selection->cursor) && !S8IsWordBoundary(string, selection->cursor))
            {
                selection->cursor += dir;
            }
            while(HasPoint1U64(Range1U64(0, string.len), selection->mark) && !S8IsWordBoundary(string, selection->mark))
            {
                selection->mark -= dir;
            }
        }
    }
    else if(use.is_dragging)
    {
        U64 clicked_char_index = FONT_PreparedTextNearestIndexFromPoint(pt, text_pos, use.initial_mouse_position);
        U64 dragged_char_index = FONT_PreparedTextNearestIndexFromPoint(pt, text_pos, G_WindowMousePositionGet(ui->window));
        selection->mark = S8ByteIndexFromCharIndex(string, clicked_char_index);
        selection->cursor = S8ByteIndexFromCharIndex(string, dragged_char_index);
    }
    
    selection->cursor = Clamp1U64(selection->cursor, 0, string.len);
    selection->mark = Clamp1U64(selection->mark, 0, string.len);
}

Function UI_Use
UI_LineEdit(S8 string, S8Builder *text)
{
    UI_Use result = UI_TextField(string, text, UI_EditTextFilterHookLineEdit, 0);
    return result;
}

Function UI_Use
UI_MultiLineEdit(S8 string, S8Builder *text)
{
    UI_Use result = UI_TextField(string, text, UI_EditTextFilterHookDefault, 0);
    return result;
}

Function UI_Use
UI_TextField(S8 string, S8Builder *text, UI_EditTextFilterHook *filter_hook, void *filter_hook_user_data)
{
    UI_Use result = {0};
    
    ArenaTemp scratch = OS_TCtxScratchBegin(0, 0);
    
    B32 is_toggled = False;
    UI_EditTextStatus status = 0;
    
    UI_SetNextChildrenLayout(Axis2_X);
    UI_Node *container = UI_NodeMake(UI_Flags_DrawFill |
                                     UI_Flags_DrawStroke |
                                     UI_Flags_DrawDropShadow |
                                     UI_Flags_DrawHotStyle |
                                     UI_Flags_DrawActiveStyle |
                                     UI_Flags_AnimateHot |
                                     UI_Flags_AnimateSmoothScroll |
                                     UI_Flags_Pressable |
                                     UI_Flags_Overflow |
                                     UI_Flags_ClampViewOffsetToOverflow |
                                     UI_Flags_SetCursorKind,
                                     string);
    
    UI_Parent(container) UI_Width(UI_SizeFromText(8.0f, 1.0f)) UI_Height(UI_SizeFromText(8.0f, 1.0f)) UI_Salt(string)
    {
        UI_Spacer(UI_SizePx(0.5f*container->corner_radius, 0.0f));
        
        B32 is_focused = !!(container->flags & UI_Flags_KeyboardFocused);
        
        UI_SetNextHotCursorKind(G_CursorKind_IBeam);
        UI_SetNextTextTruncSuffix(S8(""));
        UI_Node *inner_text = UI_NodeMake(UI_Flags_DrawText |
                                          UI_Flags_DrawHotStyle |
                                          UI_Flags_DrawActiveStyle |
                                          (UI_Flags_DrawTextCursor*is_focused) |
                                          UI_Flags_SetCursorKind |
                                          UI_Flags_AnimateInheritHot |
                                          UI_Flags_AnimateInheritActive,
                                          S8("inner text"));
        
        UI_Spacer(UI_SizePx(0.5f*container->corner_radius, 0.0f));
        
        if(container->is_new)
        {
            inner_text->text_selection.cursor = text->len;
            inner_text->text_selection.mark = text->len;
        }
        
        G_EventQueue *events = G_WindowEventQueueGet(ui->window);
        
        FONT_PreparedText *pt = UI_PreparedTextFromS8(inner_text->font_provider,
                                                      inner_text->font_size,
                                                      S8FromBuilder(*text));
        
        V2F text_pos = UI_TextPositionFromRect(pt, inner_text->rect);
        
        if(is_focused)
        {
            is_toggled = True;
            
            U64 cursor = S8CharIndexFromByteIndex(S8FromBuilder(*text), inner_text->text_selection.cursor);
            FONT_PreparedTextLine *cursor_line = 0;
            FONT_PreparedTextGlyph *c = FONT_PreparedTextGlyphFromIndex(pt, cursor, &cursor_line);
            
            container->target_view_offset.x = (0 == c) ? 0.0f : 0.5f*(container->rect.max.x - container->rect.min.x) - c->bounds.min.x;
            
            I32 line_offset = (+1*G_EventQueueHasKeyDown(events, G_Key_Down, G_ModifierKeys_Any, False) +
                               -1*G_EventQueueHasKeyDown(events, G_Key_Up, G_ModifierKeys_Any, False));
            if(0 != line_offset && 0 != cursor_line)
            {
                FONT_PreparedTextLine *next_line = (line_offset > 0) ? cursor_line->next : cursor_line->prev;
                if(0 != next_line)
                {
                    F32 min_dist2 = Infinity;
                    FONT_PreparedTextGlyph *next_cursor_glyph = 0;
                    for(FONT_PreparedTextGlyph *g = next_line->glyphs; 0 != g; g = g->next)
                    {
                        F32 dist2 = g->bounds.min.x - c->bounds.min.x; dist2 *= dist2;
                        if(dist2 < min_dist2)
                        {
                            min_dist2 = dist2;
                            next_cursor_glyph = g;
                        }
                    }
                    if(0 != next_cursor_glyph)
                    {
                        Range1U64 next_selection = inner_text->text_selection;
                        next_selection.cursor = S8ByteIndexFromCharIndex(S8FromBuilder(*text), next_cursor_glyph->char_index);
                        if(!G_WindowKeyStateGet(ui->window, G_Key_Shift))
                        {
                            next_selection.mark = next_selection.cursor;
                        }
                        if(next_selection.cursor != inner_text->text_selection.cursor ||
                           next_selection.mark != inner_text->text_selection.mark)
                        {
                            G_EventQueueHasKeyDown(events, G_Key_Down, G_ModifierKeys_Any, True);
                            G_EventQueueHasKeyDown(events, G_Key_Up, G_ModifierKeys_Any, True);
                        }
                        inner_text->text_selection = next_selection;
                    }
                }
            }
            
            for(G_EventQueueForEach(events, e))
            {
                status |= UI_EditS8Builder(e, &inner_text->text_selection, text, filter_hook, filter_hook_user_data);
                
                if(G_EventKind_Char == e->kind)
                {
                    for(G_EventQueueForEach(events, f))
                    {
                        if(G_EventKind_Key == f->kind && e->key < G_Key_PRINTABLE_END)
                        {
                            G_EventConsume(f);
                        }
                    }
                }
                
            }
        }
        if(status != UI_EditTextStatus_None)
        {
            inner_text->cursor_blink_cooldown = 5.0f;
        }
        UI_Animate1F(&inner_text->cursor_blink_cooldown, 0.0f, 5.0f);
        
        result = UI_UseFromNode(container);
        if(text->len > 0)
        {
            UI_TextFieldMouseInput(S8FromBuilder(*text), result, text_pos, pt, &inner_text->text_selection);
        }
        
        inner_text->text = S8Clone(G_WindowFrameArenaGet(ui->window), S8FromBuilder(*text));
    }
    
    result.is_edited = (status & UI_EditTextStatus_Edited);
    result.is_toggled = is_toggled;
    
    ArenaTempEnd(scratch);
    
    return result;
}

Function UI_Use
UI_IconButton(S8 string, FONT_Icon icon)
{
    UI_SetNextFont(ui_font_icons);
    UI_SetNextHotFgCol(ColFromHex(0x81a1c1ff));
    UI_SetNextActiveFgCol(ColFromHex(0x88c0d0ff));
    UI_SetNextTextTruncSuffix(S8(""));
    UI_SetNextHotGrowth(UI_GrowthPeek());
    UI_SetNextActiveGrowth(UI_GrowthPeek());
    UI_Node *button = UI_NodeMakeFromFmt(UI_Flags_DrawText |
                                         UI_Flags_DrawHotStyle |
                                         UI_Flags_DrawActiveStyle |
                                         UI_Flags_Pressable |
                                         UI_Flags_AnimateHot |
                                         UI_Flags_AnimateActive,
                                         "%c###%.*s", icon, FmtS8(string));
    UI_Use result = UI_UseFromNode(button);
    
    return result;
}

Function UI_Use
UI_Toggle(S8 string, B32 is_toggled)
{
    UI_SetNextHotBgCol(ColFromHex(0x81a1c1ff));
    UI_SetNextHotFgCol(ColFromHex(0xd8dee9ff));
    UI_SetNextActiveBgCol(ColFromHex(0x88c0d0ff));
    UI_Node *button = UI_NodeMake(UI_Flags_DrawFill |
                                  UI_Flags_DrawDropShadow |
                                  UI_Flags_DrawText |
                                  UI_Flags_DrawHotStyle |
                                  UI_Flags_DrawActiveStyle |
                                  UI_Flags_Pressable |
                                  UI_Flags_AnimateHot,
                                  string);
    
    B32 is_active = UI_KeyMatch(&button->key, &ui->active);
    UI_Animate1F(&button->active_t, is_toggled || is_active, 15.0f);
    
    UI_Use result = UI_UseFromNode(button);
    
    return result;
}

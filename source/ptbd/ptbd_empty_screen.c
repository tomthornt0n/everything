
Function void
PTBD_BuildEmptyScreen(PTBD_BuildEmptyScreenFlags flags, S8 message, PTBD_MsgQueue *m2c)
{
    UI_Column() UI_Pad(UI_SizeFill()) UI_Height(UI_SizePx(32.0f, 1.0f))
    {
        UI_Label(message);
        
        if(flags & PTBD_BuildEmptyScreenFlags_ButtonToClearSearch)
            UI_Row() UI_Pad(UI_SizeFill()) UI_Width(UI_SizeSum(1.0f))
        {
            UI_SetNextFgCol(ptbd_grey);
            UI_SetNextHotBgCol(ColFromHex(0x81a1c1ff));
            UI_SetNextActiveBgCol(ColFromHex(0x88c0d0ff));
            UI_Node *button = UI_NodeMakeFromFmt(UI_Flags_DrawFill |
                                                 UI_Flags_DrawStroke |
                                                 UI_Flags_DrawDropShadow |
                                                 UI_Flags_DrawHotStyle |
                                                 UI_Flags_DrawActiveStyle |
                                                 UI_Flags_AnimateHot |
                                                 UI_Flags_AnimateActive |
                                                 UI_Flags_Pressable,
                                                 "Clear search %.*s", FmtS8(message));
            UI_Parent(button)
            {
                UI_Width(UI_SizeFromText(8.0f, 1.0f))
                    UI_DefaultFlags(UI_Flags_AnimateInheritHot | UI_Flags_AnimateInheritActive)
                    UI_HotFgCol(ColFromHex(0xd8dee9ff))
                    UI_ActiveFgCol(ColFromHex(0xd8dee9ff))
                    UI_Label(S8("Clear search"));
            }
            
            if(UI_UseFromNode(button).is_pressed)
            {
                M2C_SetSearch(m2c, S8(""));
                /*app->search_bar.len = 0;
                S8BuilderAppend(&app->search_bar, S8(""));*/
            }
        }
    }
}

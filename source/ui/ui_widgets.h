
typedef struct UI_Window UI_Window;
struct UI_Window
{
    Range1F pixel_range;
    Range1U64 index_range;
    F32 space_before;
    F32 space_after;
};

typedef struct UI_ColourPickerTextureCache UI_ColourPickerTextureCache;
struct UI_ColourPickerTextureCache
{
    I32 hue;
    U64 last_touched_frame_index;
    
    OpaqueHandle texture;
};

Global UI_ColourPickerTextureCache ui_colour_picker_texture_cache[16] = {0};

enum { UI_ColourPickerHueBarWidth = 16, };
#define UI_ColourPickerTextureDimensions V2I(256, 256 + UI_ColourPickerHueBarWidth)

Function void UI_Label         (S8 text);
Function void UI_LabelFromFmtV (char *fmt, va_list args);
Function void UI_LabelFromFmt  (char *fmt, ...);
Function void UI_LabelFromIcon (char icon);

Function UI_Use UI_Slider1F (S8 string, F32 *value, Range1F range, F32 thumb_size);

Function UI_Use UI_ScrollBar  (S8 string, UI_Node *node_for, Axis2 axis);

Function void   UI_TextFieldMouseInput (S8 string, UI_Use use, V2F text_pos, FONT_PreparedText *pt, Range1U64 *selection);
Function UI_Use UI_LineEdit            (S8 string, S8Builder *text);
Function UI_Use UI_MultiLineEdit       (S8 string, S8Builder *text);
Function UI_Use UI_TextField           (S8 string, S8Builder *text, UI_EditTextFilterHook *filter_hook, void *filter_hook_user_data);

Function UI_Use UI_IconButton (S8 string, FONT_Icon icon);
Function UI_Use UI_Toggle     (S8 string, B32 is_toggled);

Function void   UI_PixelsFromColourPicker (Pixel *result, V2I dimensions, I32 hue_bar_width, F32 hue);
Function UI_Use UI_ColourPicker           (S8 string, V4F *colour);


#include <stdint.h>

extern uint64_t font_atkinson_hyperlegible_bold_len;
extern uint8_t font_atkinson_hyperlegible_bold[];
extern uint64_t font_atkinson_hyperlegible_regular_len;
extern uint8_t font_atkinson_hyperlegible_regular[];
extern uint64_t font_ac437_len;
extern uint8_t font_ac437[];
extern uint64_t font_icons_len;
extern uint8_t font_icons[];

typedef enum FONT_Icon FONT_Icon;
enum FONT_Icon
{
	FONT_Icon_Stopwatch      = '!',
	FONT_Icon_Adult          = '"',
	FONT_Icon_CreditCard     = '#',
	FONT_Icon_Briefcase      = '$',
	FONT_Icon_CheckEmpty     = '%',
	FONT_Icon_Check          = '&',
	FONT_Icon_PlusCircled    = '\'',
	FONT_Icon_TrashEmpty     = '(',
	FONT_Icon_OkCircled      = ')',
	FONT_Icon_CancelCircled  = '*',
	FONT_Icon_EllipsisVert   = '+',
	FONT_Icon_Ellipsis       = ',',
	FONT_Icon_ResizeFull     = '-',
	FONT_Icon_ResizeSmall    = '.',
	FONT_Icon_ThList         = '3',
	FONT_Icon_LeftDir        = '<',
	FONT_Icon_RightDir       = '>',
	FONT_Icon_UpDir          = '^',
	FONT_Icon_Filter         = 'f',
	FONT_Icon_Search         = 'm',
	FONT_Icon_Pencil         = 'p',
	FONT_Icon_ArrowsCw       = 'r',
	FONT_Icon_Cog            = 's',
	FONT_Icon_DownDir        = 'v',
	FONT_Icon_Ccw            = '/',
	FONT_Icon_Export         = '0',
	FONT_Icon_Cube           = '1',
	FONT_Icon_Th             = '2',
	FONT_Icon_Fork           = '4',
	FONT_Icon_Cab            = '5',
	FONT_Icon_Clock          = '6',
	FONT_Icon_ThumbsUpAlt    = '7',
	FONT_Icon_Pound          = '8',
    FONT_Icon_Eye            = 'i',
};


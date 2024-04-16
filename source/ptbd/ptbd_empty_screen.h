
typedef enum
{
    PTBD_BuildEmptyScreenFlags_ButtonToClearSearch = Bit(0),
} PTBD_BuildEmptyScreenFlags;

Function void PTBD_BuildEmptyScreen (PTBD_BuildEmptyScreenFlags flags, S8 message, PTBD_MsgQueue *m2c);

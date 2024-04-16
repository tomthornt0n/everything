
typedef struct IMG_Data IMG_Data;
struct IMG_Data
{
    V2I dimensions;
    Pixel *pixels;
};

Function void IMG_Init    (void);
Function void IMG_Cleanup (void);
Function IMG_Data IMG_DataFromFilename (Arena *arena, S8 filename);

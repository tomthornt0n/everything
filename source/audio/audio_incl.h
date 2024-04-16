
typedef struct A_Format A_Format;
struct A_Format
{
    U64 sample_rate;
    I32 channels_count;
};
#define A_Format(...) ((A_Format){ __VA_ARGS__, })

#define A_UseDefaultFormat ((A_Format){0})

typedef struct A_Buffer A_Buffer;
struct A_Buffer
{
    A_Format format;
    void *samples;
    U64 samples_count;
    U64 played_count;
};

Function void A_Init (A_Format format);
Function void A_Cleanup (void);
Function A_Format A_GetFormat (void);

Function A_Buffer A_Lock (void);
Function void A_Unlock (U64 written_samples);

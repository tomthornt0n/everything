
typedef struct Sample Sample;
struct Sample
{
    U64 volatile generation;
    
    void *buffer;
    U64 count;
};

ReadOnlyVar Global Sample nil_sample = {0};

typedef I32 TriggerKind;
typedef enum
{
    TriggerKind_Normal,
    TriggerKind_Link,
} TriggerKindEnum;

typedef struct Trigger Trigger;
struct Trigger
{
    TriggerKind kind;
    OpaqueHandle clip;
    F32 hovered_t;
};

typedef struct Clip Clip;
struct Clip
{
    U64 generation;
    
    Clip *next;
    Clip *prev;
    
    OpaqueHandle sample;
    V2F position;
    
    F32 level;
    
    F32 trigger_t;
    U64 triggered_counter;
    U64 play_every_n_triggers;
    
    Trigger triggers[32];
    U64 triggers_count;
    
    U64 triggered[32];
    U64 triggered_count;
};

ReadOnlyVar Global Clip nil_clip =
{
    .prev = &nil_clip,
    .next = &nil_clip,
};

typedef struct Sound Sound;
struct Sound
{
    Sound *next;
    OpaqueHandle sample;
    U64 cursor;
    F32 level;
};

ReadOnlyVar Global Sound nil_sound =
{
    .next = &nil_sound,
};

typedef struct Mixer Mixer;
struct Mixer
{
    OpaqueHandle lock;
    OpaqueHandle thread;
    B32 volatile should_quit;
    Sound *volatile sounds;
    Sound *volatile free_list;
    Sample *samples;
};

typedef struct WaveformTextureCacheEntry WaveformTextureCacheEntry;
struct WaveformTextureCacheEntry
{
    WaveformTextureCacheEntry *next;
    OpaqueHandle sample_for;
    OpaqueHandle texture;
};

typedef struct WaveformTextureCache WaveformTextureCache;
struct WaveformTextureCache
{
    WaveformTextureCacheEntry *buckets[128];
};

typedef struct AppState AppState;
struct AppState
{
    UI_State *ui;
    
    // TODO(tbt): rethink hard limits
    Sample samples[128];
    Clip clips[512];
    Clip *clips_first;
    Clip *clips_free_list;
    
    Mixer mixer;
    
    OpaqueHandle dragging;
    
    TriggerKind new_trigger_kind;
    Clip *new_trigger_from;
    
    LINE_SegmentList line_segments;
    
    WaveformTextureCache waveform_texture_cache;
};

Global OpaqueHandle main_window = {0};

ReadOnlyVar Global U64 microseconds_per_unit = (1000);
ReadOnlyVar Global F32 clip_height = 64.0f;

Function void WindowHookOpen  (OpaqueHandle window);
Function void WindowHookDraw  (OpaqueHandle window);
Function void WindowHookClose (OpaqueHandle window);

Function S8   SaveFilename        (Arena *arena);
Function S8   SerialiseAppState   (Arena *arena, AppState *state);
Function void DeserialiseAppState (S8 serialised_data, AppState *state);

Function void    MixerThreadProc (void *user_data);
Function Sample *SampleFromHandle (Sample samples[], OpaqueHandle handle);

Function V4F ColourFromSampleHandle (Sample samples[], OpaqueHandle handle);

Function Clip *ClipFromHandle (Clip clips[], OpaqueHandle handle);
Function void  TriggerClipIn  (Clip *clip, U64 microseconds);

Function void LoadSample (S8 filename, U64 target_sample_rate, Sample *result);
Function void PlaySample (Mixer *mixer, OpaqueHandle sample, F32 level);

typedef enum
{
    DrawArrowFlags_Dashed = Bit(0),
} DrawArrowFlags;

Function void DrawArrow (OpaqueHandle window, LINE_SegmentList *segments, V2F from, V2F to, V4F col, F32 width, Range2F mask, DrawArrowFlags flags);

Function OpaqueHandle WaveformTextureFromSampleHandle (Arena *arena, WaveformTextureCache *cache, Sample samples[], OpaqueHandle sample);

Function UI_Node *BuildClipUI (S8 string, V2F position, F32 level, U64 triggered_counter, U64 play_every_n_triggers, WaveformTextureCache *texture_cache, Sample samples[], OpaqueHandle sample_handle);

#if BuildModeDebug
Global DBG_SymTable debug_symbols = {0};
#endif

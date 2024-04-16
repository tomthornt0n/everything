
#include "external/fonts.h"

#include "base/base_incl.h"
#include "os/os_incl.h"
#include "graphics/graphics_incl.h"
#include "renderer_2d/renderer_2d_incl.h"
#include "renderer_line/renderer_line_incl.h"
#include "font/font_incl.h"
#include "draw/draw_incl.h"
#include "audio/audio_incl.h"
#include "visidesk_incl.h"

#include "base/base_incl.c"
#include "os/os_incl.c"
#include "graphics/graphics_incl.c"
#include "renderer_2d/renderer_2d_incl.c"
#include "renderer_line/renderer_line_incl.c"
#include "font/font_incl.c"
#include "draw/draw_incl.c"
#include "audio/audio_incl.c"
#include "visidesk_incl.c"

typedef enum
{
    SampleTag_Kick,
    SampleTag_Snare,
    SampleTag_Hat,
    SampleTag_MAX,
} SampleTag;

typedef struct Sample Sample;
struct Sample
{
    void *buffer;
    U64 count;
};

Global Sample samples[SampleTag_MAX];

typedef struct Sound Sound;
struct Sound
{
    Sound *next;
    SampleTag sample;
    U64 cursor;
    F32 level;
};

#include <mfapi.h>
#include <mfidl.h>
#include <mfreadwrite.h>

#pragma comment (lib, "mfplat")
#pragma comment (lib, "mfreadwrite")

Function void
LoadSample(S8 filename, U64 target_sample_rate, Sample *result)
{
    ArenaTemp scratch = OS_TCtxScratchBegin(0, 0);
    Arena *permanent_arena = OS_TCtxGet()->permanent_arena;
    
	HRESULT hr = MFStartup(MF_VERSION, MFSTARTUP_LITE);
    
	IMFSourceReader *reader;
    S16 filename_s16 = S16FromS8(scratch.arena, filename);
	MFCreateSourceReaderFromURL(filename_s16.buffer, 0, &reader);
    
	// read only first audio stream
	IMFSourceReader_SetStreamSelection(reader, (DWORD)MF_SOURCE_READER_ALL_STREAMS, False);
	IMFSourceReader_SetStreamSelection(reader, (DWORD)MF_SOURCE_READER_FIRST_AUDIO_STREAM, True);
    
	WAVEFORMATEXTENSIBLE format =
	{
		.Format =
		{
			.wFormatTag = WAVE_FORMAT_EXTENSIBLE,
			.nChannels = 2,
			.nSamplesPerSec = target_sample_rate,
			.nAvgBytesPerSec = target_sample_rate*2*sizeof(I16),
			.nBlockAlign = 2*sizeof(I16),
			.wBitsPerSample = 8*sizeof(I16),
			.cbSize = sizeof(format) - sizeof(format.Format),
		},
		.Samples.wValidBitsPerSample = 8*sizeof(I16),
		.dwChannelMask = SPEAKER_FRONT_LEFT | SPEAKER_FRONT_RIGHT,
		.SubFormat = MEDIASUBTYPE_PCM,
	};
    
	IMFMediaType *type;
	MFCreateMediaType(&type);
	MFInitMediaTypeFromWaveFormatEx(type, &format.Format, sizeof(format));
	IMFSourceReader_SetCurrentMediaType(reader, (DWORD)MF_SOURCE_READER_FIRST_AUDIO_STREAM, 0, type);
	IMFMediaType_Release(type);
    
	U64 samples_len = 0;
    void *samples = 0;
    
	for(;;)
	{
		IMFSample *sample;
		DWORD flags = 0;
		HRESULT hr = IMFSourceReader_ReadSample(reader, (DWORD)MF_SOURCE_READER_FIRST_AUDIO_STREAM, 0, 0, &flags, 0, &sample);
		if(FAILED(hr))
		{
			break;
		}
        
		if(flags & MF_SOURCE_READERF_ENDOFSTREAM)
		{
			break;
		}
        
		IMFMediaBuffer *buffer;
		IMFSample_ConvertToContiguousBuffer(sample, &buffer);
        
		BYTE *data;
		DWORD size;
		IMFMediaBuffer_Lock(buffer, &data, 0, &size);
		{
            void *new_samples = PushArray(permanent_arena, U8, size);
            MemoryCopy(new_samples, data, size);
            if(0 == samples)
            {
                samples = new_samples;
            }
            samples_len += size;
		}
		IMFMediaBuffer_Unlock(buffer);
        
		IMediaBuffer_Release(buffer);
		IMFSample_Release(sample);
	}
    
	IMFSourceReader_Release(reader);
    
	MFShutdown();
    
    result->buffer = samples;
    result->count = samples_len / format.Format.nBlockAlign;
    
    ArenaTempEnd(scratch);
}

ReadOnlyVar Global Sound nil_sound =
{
    .next = &nil_sound,
};

typedef struct Mixer Mixer;
struct Mixer
{
    OpaqueHandle thread;
    B32 volatile should_quit;
    Sound *sounds;
    Sound *free_list;
    Sample *samples;
};

Global Mixer mixer = {0};

Global U64 playhead = 0;


Global OpaqueHandle main_window = {0};

Global OpaqueHandle font = {0};

enum
{
    BitmapWidth = 640,
    BitmapHeight = 480,
};

Global VD_Bitmap bitmap;
Global OpaqueHandle bitmap_texture = {0};

Function void WindowHookOpen (OpaqueHandle window);
Function void WindowHookDraw (OpaqueHandle window);
Function void WindowHookClose (OpaqueHandle window);

enum
{
    MicrosecondsPerBar = 2000000,
};

Global U64 samples_per_bar;

Global I32 seed = 0;

typedef struct Trigger Trigger;
struct Trigger
{
    Trigger *next; 
    U64 at;
    B32 triggered;
    I32 depth;
    Range2F drawing_bounds;
};

typedef struct TriggerList TriggerList;
struct TriggerList
{
    Trigger *first;
    Trigger *last;
};

Global Arena *tree_arena = 0;
Global VD_Node *tree;

Global OpaqueHandle triggers_lock = {0};
Global Arena *triggers_arena_old = 0;
Global Arena *triggers_arena = 0;
Global TriggerList triggers = {0};
Global TriggerList triggers_next = {0};

Function void
MixerThreadProc(void *user_data)
{
    Arena *permanent_arena = OS_TCtxGet()->permanent_arena;
    
    I32 rand = seed;
    
    while(!mixer.should_quit)
    {
        A_Buffer buffer = A_Lock();
        
        playhead += buffer.played_count;
        
        U64 write_count = Min1U64(buffer.samples_count, buffer.format.sample_rate / 10);
        
        Sound **from = &mixer.sounds;
        Sound *next = 0;
        for(Sound *sound = mixer.sounds; 0 != sound; sound = next)
        {
            Sample *sample = &samples[sound->sample];
            next = sound->next;
            if(sound->cursor > sample->count)
            {
                *from = sound->next;
                sound->next = mixer.free_list;
                mixer.free_list = sound;
            }
            else
            {
                from = &sound->next;
            }
        }
        
        MemorySet(buffer.samples, 0, write_count*buffer.format.channels_count*sizeof(F32));
        
        for(Sound *sound = mixer.sounds; 0 != sound; sound = sound->next)
        {
            Sample *sample = &samples[sound->sample];
            I16 *in = &((I16 *)sample->buffer)[sound->cursor*2];
            F32 *out = buffer.samples;
            U64 mix_count = Min1U64(write_count, sample->count - sound->cursor);
            for(U64 i = 0; i < mix_count; i += 1)
            {
                if(1 == buffer.format.channels_count)
                {
                    out[0] += Clamp1F(((F32)in[0] + (F32)in[1])*(sound->level / 32768.0f), -1.0f, 1.0f);
                    out += 1;
                }
                else if(2 == buffer.format.channels_count)
                {
                    out[0] += Clamp1F(in[0]*(sound->level / 32768.0f), -1.0f, 1.0f);
                    out[1] += Clamp1F(in[1]*(sound->level / 32768.0f), -1.0f, 1.0f);
                    out += 2;
                }
                in += 2;
            }
            sound->cursor += buffer.played_count;
        }
        
        A_Unlock(write_count);
        
        for(Trigger *trigger = triggers.first; 0 != trigger; trigger = trigger->next)
        {
            if(playhead > trigger->at*buffer.format.sample_rate / 1000000 && !trigger->triggered)
            {
                Sound *sound = mixer.free_list;
                if(0 == sound)
                {
                    sound = PushArray(permanent_arena, Sound, 1);
                }
                else
                {
                    mixer.free_list = mixer.free_list->next;
                }
                SampleTag choices[] = 
                {
                    SampleTag_Kick,
                    SampleTag_Kick,
                    SampleTag_Snare,
                    SampleTag_Hat,
                    SampleTag_Hat,
                    SampleTag_Hat,
                };
                sound->sample =  choices[RandNextBounded1I(&rand, 0, ArrayCount(choices))];
                sound->level = 0.25f + RandNextBounded1I(&rand, 0, 1000) / 2000.0f;
                sound->cursor = 0;
                sound->next = OS_InterlockedExchange1Ptr(&mixer.sounds, sound);
                
                trigger->triggered = True;
            }
        }
        
        if(playhead > samples_per_bar)
        {
            rand = seed;
            playhead = playhead % samples_per_bar;
            for(Trigger *trigger = triggers.first; 0 != trigger; trigger = trigger->next)
            {
                trigger->triggered = False;
            }
        }
        
        OS_CriticalSectionExclusive(triggers_lock)
        {
            if(0 != triggers_next.first)
            {
                triggers = triggers_next;
                triggers_next.first = 0;
                triggers_next.last = 0;
                Arena *temp = triggers_arena_old;
                triggers_arena_old = triggers_arena;
                triggers_arena = temp;
            }
        }
        
    }
}

EntryPoint
{
    OS_Init();
    G_Init();
    R2D_Init();
    FONT_Init();
    A_Init(A_UseDefaultFormat);
    
    A_Format audio_format = A_GetFormat();
    samples_per_bar = MicrosecondsPerBar*audio_format.sample_rate / 1000000;
    LoadSample(S8("kick.wav"), audio_format.sample_rate, &samples[SampleTag_Kick]);
    LoadSample(S8("snare.wav"), audio_format.sample_rate, &samples[SampleTag_Snare]);
    LoadSample(S8("hat.wav"), audio_format.sample_rate, &samples[SampleTag_Hat]);
    
    triggers_lock = OS_SRWAlloc();
    
    G_WindowHooks window_hooks =
    {
        WindowHookOpen,
        WindowHookDraw,
        WindowHookClose,
    };
    main_window = G_WindowOpen(S8("visidesk"), V2I(1024, 768), window_hooks, 0);
    
    Arena *permanent_arena = OS_TCtxGet()->permanent_arena;
    
    font = FONT_ProviderFromTTF(permanent_arena, (S8){ .buffer = font_ac437, .len = font_ac437_len, });
    
    tree_arena = ArenaAlloc(Megabytes(4));
    triggers_arena = ArenaAlloc(Megabytes(4));
    triggers_arena_old = ArenaAlloc(Megabytes(4));
    
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
    
    OS_ThreadStart(MixerThreadProc, 0);
    
    G_MainLoop();
    
    OS_InterlockedExchange1I(&mixer.should_quit, True);
    OS_ThreadJoin(mixer.thread);
    
    A_Cleanup();
    R2D_Cleanup();
    G_Cleanup();
    OS_Cleanup();
}

Function void
WindowHookOpen(OpaqueHandle window)
{
    
}

Function void
TriggersFromTree(Arena *arena, VD_Node *root, Range1U64 subdivision, I32 depth, TriggerList *result)
{
    U64 microseconds_per_child = Measure1U64(subdivision) / Max1U64(root->children_count, 1);
    
    U64 child_index = 0;
    for(VD_Node *child = root->first; 0 != child; child = child->next)
    {
        if(0 == child->children_count)
        {
        }
        else
        {
            Range1U64 child_range =
                Range1U64(subdivision.min + child_index*microseconds_per_child,
                          subdivision.min + child_index*microseconds_per_child + microseconds_per_child);
            
            Trigger *trigger = PushArray(arena, Trigger, 1);
            trigger->at = child_range.min;
            trigger->depth = depth;
            trigger->drawing_bounds = child->bounds;
            if(0 == result->last)
            {
                result->first = trigger;
            }
            else
            {
                result->last->next = trigger;
            }
            result->last = trigger;
            
            for(VD_Node *next = child->first; 0 != next; next = next->next)
            {
                TriggersFromTree(arena, next, child_range, depth + 1, result);
            }
        }
        child_index += 1;
    }
}

Function V2F
DrawTree(Arena *arena, DRW_List *drw, VD_Node *root, V2F position, Range2F mask, Range2F canvas_rect)
{
    F32 result = 0.0f;
    
    ArenaTemp scratch = OS_TCtxScratchBegin(&arena, 1);
    S8 string = root->value ? S8("black") : S8("white");
    FONT_PreparedText *pt = PushArray(arena, FONT_PreparedText, 1);
    *pt = FONT_PrepareS8(arena, 0, font, 28, string);
    DRW_Text(arena, drw, pt, position, ColFromHex(0x362b00ff), mask);
    ArenaTempEnd(scratch);
    
    if(HasPoint2F(pt->bounds, Sub2F(G_WindowMousePositionGet(main_window), position)))
    {
        Range2F rect = 
        {
            .min = RangeMap2F(Range2F(V2F(0.0f, 0.0f), V2F(BitmapWidth, BitmapHeight)), canvas_rect, root->bounds.min),
            .max = RangeMap2F(Range2F(V2F(0.0f, 0.0f), V2F(BitmapWidth, BitmapHeight)), canvas_rect, Add2F(root->bounds.max, V2F(1.0f, 1.0f))),
        };
        R2D_Quad quad =
        {
            .dst = rect,
            .src = r2d_entire_texture,
            .mask = mask,
            .fill_colour = ColFromHex(0x2aa19866),
            .stroke_colour = ColFromHex(0x2aa198ff),
            .stroke_width = 4.0f,
            .edge_softness = 1.0f,
        };
        DRW_Sprite(arena, drw, quad, R2D_TextureNil());
    }
    
    position.x += 32.0f;
    for(VD_Node *child = root->first; 0 != child; child = child->next)
    {
        position.y += FONT_ProviderVerticalAdvanceFromSize(font, 24);
        position = DrawTree(arena, drw, child, position, mask, canvas_rect);
    }
    position.x -= 32.0f;
    
    return position;
};

Function void
WindowHookDraw(OpaqueHandle window)
{
    DRW_List drw = {0};
    
    F32 s = G_WindowScaleFactorGet(window);
    Range2F client_rect = G_WindowClientRectGet(window);
    Arena *frame_arena = G_WindowFrameArenaGet(window);
    G_EventQueue *events = G_WindowEventQueueGet(window);
    V2F mouse_position = G_WindowMousePositionGet(window);
    
    V2F canvas_size = V2F(BitmapWidth, BitmapHeight);
    V2F max_canvas_size = Scale2F(client_rect.max, 0.7f / s);
    while(2.0f*canvas_size.x < max_canvas_size.x &&
          2.0f*canvas_size.y < max_canvas_size.y)
    {
        canvas_size = Scale2F(canvas_size, 2.0f);
    }
    V2F canvas_pos = Scale2F(Sub2F(Measure2F(client_rect), Add2F(canvas_size, V2F(256.0f, 0.0f))), 0.5f);
    Range2F canvas_rect = RectMake2F(canvas_pos, canvas_size);
    R2D_Quad canvas_quad =
    {
        .dst = canvas_rect,
        .src = r2d_entire_texture,
        .mask = client_rect,
        .fill_colour = U4F(1.0f),
    };
    DRW_Sprite(frame_arena, &drw, canvas_quad, bitmap_texture);
    
    if(0 != tree)
        DrawTree(frame_arena, &drw, tree, V2F(canvas_rect.max.x, canvas_rect.min.y), client_rect, canvas_rect);
    
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
        tree = VD_TreeFromBitmap(tree_arena, &bitmap, V2I(0, 0));
        
        OS_CriticalSectionExclusive(triggers_lock)
        {
            ArenaClear(triggers_arena);
            triggers_next.first = 0;
            triggers_next.last = 0;
            TriggersFromTree(triggers_arena, tree, Range1U64(0, MicrosecondsPerBar), 0, &triggers_next);
            playhead = 0;
        }
    }
    
    for(Trigger *trigger = triggers.first; 0 != trigger; trigger = trigger->next)
    {
        F32 trigger_radius = 8.0f*s;
        V2F trigger_pos = V2F(canvas_pos.x + canvas_size.x*((double)trigger->at / MicrosecondsPerBar), canvas_rect.max.y + trigger->depth*3.0f*trigger_radius);
        Range2F trigger_rect = Range2F(Sub2F(trigger_pos, U2F(trigger_radius)),
                                       Add2F(trigger_pos, U2F(trigger_radius)));
        R2D_Quad trigger_quad =
        {
            .dst = trigger_rect,
            .mask = client_rect,
            .fill_colour = trigger->triggered ? ColFromHex(0x859900ff)  : ColFromHex(0x362b00ff),
        };
        
        V2F canvas_mouse_position = RangeMap2F(canvas_rect, Range2F(V2F(0.0f, 0.0f), V2F(BitmapWidth, BitmapHeight)), mouse_position);
        if(HasPoint2F(trigger->drawing_bounds, canvas_mouse_position))
        {
            trigger_quad.dst = Grow2F(trigger_quad.dst, U2F(4.0f));
            trigger_quad.stroke_width = 4.0f;
            trigger_quad.stroke_colour = ColFromHex(0x6c71c4ff);
        }
        
        DRW_Sprite(frame_arena, &drw, trigger_quad, R2D_TextureNil());
    }
    
    if(pen && HasPoint2F(canvas_rect, mouse_position))
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
    
    DRW_ListSubmit(window, &drw);
}

Function void
WindowHookClose(OpaqueHandle window)
{
    
}

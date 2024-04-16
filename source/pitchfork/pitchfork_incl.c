
#if BuildOSWindows

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
    
    OS_InterlockedIncrement1U64(&result->generation);
    
    // TODO(tbt): handle errors?
    
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
#else
# error not implemented for this os yet
#endif

Function OpaqueHandle
WaveformTextureFromSampleHandle(Arena *arena, WaveformTextureCache *cache, Sample samples[], OpaqueHandle sample_handle)
{
    OpaqueHandle result = R2D_TextureNil();
    
    Sample *sample = SampleFromHandle(samples, sample_handle);
    if(&nil_sample != sample)
    {
        ArenaTemp scratch = OS_TCtxScratchBegin(&arena, 1);
        
        // NOTE(tbt): lookup entry in cache hash table
        U64 hash[2];
        MurmurHash3_x64_128(&sample_handle, sizeof(sample_handle), 0, hash);
        U64 index = hash[0] % ArrayCount(cache->buckets);
        WaveformTextureCacheEntry *entry = cache->buckets[index];
        while(0 != entry && !OpaqueHandleMatch(entry->sample_for, sample_handle))
        {
            entry = entry->next;
        }
        
        if(0 == entry)
        {
            // NOTE(tbt): if not found, allocate and push a new entry
            entry = PushArray(arena, WaveformTextureCacheEntry, 1);
            entry->sample_for = sample_handle;
            entry->next = cache->buckets[index];
            cache->buckets[index] = entry;
            
            // NOTE(tbt): and generate the texture
            U64 samples_per_column = microseconds_per_unit*A_GetFormat().sample_rate*0.000001f;
            U64 width = (sample->count / samples_per_column);
            Pixel *pixels = PushArray(scratch.arena, Pixel, width*clip_height);
            I16 *buffer = sample->buffer;
            I16 max_amplitude = 0;
            for(U64 i = 0; i < sample->count; i += 1)
            {
                U64 sample_index = 2*(i);
                I16 l = buffer[sample_index];
                I16 r = buffer[sample_index + 1];
                max_amplitude = Max1I(max_amplitude, Max1I(Abs1F(l), Abs1F(r)));
            }
            for(U64 x = 0; x < width; x += 1)
            {
                U64 sum_of_squares = 0;
                for(U64 i = 0; i < samples_per_column; i += 1)
                {
                    U64 sample_index = 2*(x*samples_per_column + i);
                    I16 l = buffer[sample_index];
                    I16 r = buffer[sample_index + 1];
                    I16 avg = (l + r) / 2;
                    sum_of_squares += avg*avg;
                }
                
                // NOTE(tbt): rms
                {
                    F32 rms = Sqrt1F(sum_of_squares / samples_per_column);
                    U64 column_height = (clip_height*rms) / max_amplitude;
                    U64 padding = (clip_height - column_height) / 2;
                    for(U64 y = padding; y < clip_height - padding; y += 1)
                    {
                        pixels[y*width + x] = Pixel(0xffffffff);
                    }
                }
            }
            entry->texture = R2D_TextureAlloc(pixels, V2I(width, clip_height));
        }
        
        ArenaTempEnd(scratch);
        
        result = entry->texture;
    }
    
    return result;
}

Function Range2F
RectFromClipPositionAndSample(V2F position, Sample *sample)
{
    Range2F result = {0};
    
    F32 s = G_WindowScaleFactorGet(main_window);
    
    if(&nil_sample == sample)
    {
        V2F half_span = U2F(10.0f*s);
        result = Range2F(Sub2F(position, half_span),
                         Add2F(position, half_span));
    }
    else
    {
        V2F size = V2F(((sample->count*1000000.0*s) / (A_GetFormat().sample_rate*microseconds_per_unit)), clip_height*s);
        result = Range2F(V2F(position.x, position.y - 0.5f*size.y),
                         V2F(position.x + size.x, position.y + 0.5f*size.y));
    }
    
    return result;
}

Function UI_Node *
BuildClipUI(S8 string, V2F position, F32 level, U64 triggered_counter, U64 play_every_n_triggers, WaveformTextureCache *texture_cache, Sample samples[], OpaqueHandle sample_handle)
{
    UI_Node *result = 0;
    
    Arena *permanent_arena = OS_TCtxGet()->permanent_arena;
    F32 s = G_WindowScaleFactorGet(main_window);
    
    UI_Width(UI_SizeNone()) UI_Height(UI_SizeNone())
        UI_CornerRadius(10.0f)
    {
        if(play_every_n_triggers > 1)
            UI_Font(ui_font_bold) UI_FontSize(clip_height)
        {
            ArenaTemp scratch = OS_TCtxScratchBegin(0, 0);
            S8 label = S8FromFmt(scratch.arena, "%llu/%llu", triggered_counter, play_every_n_triggers);
            FONT_PreparedText *pt = UI_PreparedTextFromS8(UI_FontPeek(), UI_FontSizePeek()*s, label);
            Range2F label_rect = Range2F(V2F(position.x + pt->bounds.min.x, position.y - 0.5f*clip_height*s),
                                         V2F(position.x + pt->bounds.max.x + 32.0f, position.y + 0.5f*clip_height*s));
            UI_SetNextFgCol(ColFromHex(0x2e344088));
            UI_SetNextFixedRect(label_rect);
            UI_SetNextDefaultFlags(UI_Flags_FixedRelativeRect);
            UI_Label(label);
            ArenaTempEnd(scratch);
        }
        
        Sample *sample = SampleFromHandle(samples, sample_handle);
        Range2F rect = RectFromClipPositionAndSample(position, sample);
        V4F colour = ColourFromSampleHandle(samples, sample_handle);
        
        if(&nil_sample != sample)
        {
            OpaqueHandle texture = WaveformTextureFromSampleHandle(permanent_arena, texture_cache, samples, sample_handle);
            UI_SetNextFixedRect(rect);
            UI_SetNextBgCol(ColFromHex(0xeceff4aa));
            UI_SetNextTexture(texture);
            UI_NodeMake(UI_Flags_DrawFill |
                        UI_Flags_FixedRelativeRect, S8(""));
            
            Range2F level_rect = rect;
            level_rect.max.y = InterpolateLinear1F(rect.min.y, rect.max.y, level);
            UI_SetNextFixedRect(level_rect);
            UI_SetNextBgCol(ColFromHex(0xeceff488));
            UI_NodeMake(UI_Flags_DrawFill |
                        UI_Flags_DrawHotStyle |
                        UI_Flags_DrawActiveStyle |
                        UI_Flags_AnimateInheritHot |
                        UI_Flags_AnimateInheritActive |
                        UI_Flags_FixedRelativeRect, S8(""));
        }
        
        UI_SetNextFixedRect(rect);
        UI_SetNextBgCol(colour);
        result =
            UI_NodeMake(UI_Flags_DrawFill |
                        UI_Flags_DrawDropShadow |
                        UI_Flags_DrawHotStyle |
                        UI_Flags_DrawActiveStyle |
                        UI_Flags_Pressable |
                        UI_Flags_AnimateHot |
                        UI_Flags_AnimateActive |
                        UI_Flags_FixedRelativeRect,
                        string);
    }
    
    return result;
}

Function void
WindowHookOpen(OpaqueHandle window)
{
    ArenaTemp scratch = OS_TCtxScratchBegin(0, 0);
    AppState *app = G_WindowUserDataGet(window);
    app->ui = UI_Init(window);
    G_WindowClearColourSet(window, ColFromHex(0xeceff4ff));
    S8 save_file_path = SaveFilename(scratch.arena);
    S8 serialised_data = OS_FileReadEntire(scratch.arena, save_file_path);
    DeserialiseAppState(serialised_data, app);
    ArenaTempEnd(scratch);
}

Function V4F
ColourFromSampleHandle(Sample samples[], OpaqueHandle handle)
{
    V4F result = ColFromHex(0x4c566aff);
    if(handle.g > 0 && samples[handle.p].generation == handle.g)
    {
        V4F colours[] =
        {
            ColFromHex(0xebcb8bff),
            ColFromHex(0xa3be8cff),
            ColFromHex(0x8fbcbbff),
            ColFromHex(0x88c0d0ff),
            ColFromHex(0x81a1c1ff),
            ColFromHex(0x5e81acff),
            ColFromHex(0xb48eadff),
            ColFromHex(0xd08770ff),
        };
        U64 hash[2];
        MurmurHash3_x64_128(&handle, sizeof(handle), 0, hash);
        result = colours[hash[0] % ArrayCount(colours)];
    }
    return result;
}

Function Sample *
SampleFromHandle(Sample samples[], OpaqueHandle handle)
{
    Sample *result = &nil_sample;
    if(0 != handle.g && samples[handle.p].generation == handle.g)
    {
        result = &samples[handle.p];
    }
    return result;
}

Function Clip *
ClipFromHandle(Clip clips[], OpaqueHandle handle)
{
    Clip *result = &nil_clip;
    if(0 != handle.g && clips[handle.p].generation == handle.g)
    {
        result = &clips[handle.p];
    }
    return result;
}

Function V2F
WorldFromScreen(V2F screen_position, UI_Node *canvas)
{
    F32 s = G_WindowScaleFactorGet(main_window);
    V2F result = Scale2F(Sub2F(Sub2F(screen_position, canvas->rect.min), canvas->view_offset), 1.0f / s);
    return result;
}

Function V2F
ScreenFromWorld(V2F world_position, UI_Node *canvas)
{
    F32 s = G_WindowScaleFactorGet(main_window);
    V2F result = Add2F(Scale2F(world_position, s), Add2F(canvas->rect.min, canvas->view_offset));
    return result;
}

Function S8
SaveFilename(Arena *arena)
{
    ArenaTemp scratch = OS_TCtxScratchBegin(&arena, 1);
    S8 config_dir = OS_S8FromStdPath(scratch.arena, OS_StdPath_Config);
    S8 result = FilenamePush(arena, config_dir, S8("pitchfork_canvas"));
    ArenaTempEnd(scratch);
    return result;
}

Function S8
SerialiseAppState(Arena *arena, AppState *state)
{
    //-NOTE(tbt): calculate size
    U64 size = 3*sizeof(U64); // samples_count, clips_count, sample rate
    U64 samples_count = 0;
    U64 clips_count = 0;
    for(U64 sample_index = 0; sample_index < ArrayCount(state->samples); sample_index += 1)
    {
        if(state->samples[sample_index].count > 0)
        {
            size += (2*sizeof(U64) + // index, count
                     2*sizeof(I16)*state->samples[sample_index].count); // buffer
            samples_count += 1;
        }
    }
    for(Clip *clip = state->clips_first; &nil_clip != clip; clip = clip->next)
    {
        U64 size_per_trigger = (sizeof(U64) + // clip index
                                sizeof(TriggerKind)); // kind
        size += (4*sizeof(U64) + // index, sample index, triggers_count, play_every_n_triggers
                 sizeof(V2F) + // position
                 sizeof(F32) + // level
                 clip->triggers_count*size_per_trigger);
        clips_count += 1;
    }
    
    //-NOTE(tbt): write
    S8Builder builder = PushS8Builder(arena, size);
    U64 sample_rate = A_GetFormat().sample_rate;
    S8BuilderAppendBytesFromLValue(&builder, sample_rate);
    S8BuilderAppendBytesFromLValue(&builder, samples_count);
    for(U64 sample_index = 0; sample_index < ArrayCount(state->samples); sample_index += 1)
    {
        if(state->samples[sample_index].count > 0)
        {
            S8BuilderAppendBytesFromLValue(&builder, sample_index);
            S8BuilderAppendBytesFromLValue(&builder, state->samples[sample_index].count);
            S8BuilderAppendBytes(&builder, state->samples[sample_index].buffer, 2*sizeof(I16)*state->samples[sample_index].count);
        }
    }
    S8BuilderAppendBytesFromLValue(&builder, clips_count);
    for(Clip *clip = state->clips_first; &nil_clip != clip; clip = clip->next)
    {
        U64 index = clip - state->clips;
        S8BuilderAppendBytesFromLValue(&builder, index);
        if(0 == clip->sample.g)
        {
            U64 p = ~((U64)0);
            S8BuilderAppendBytesFromLValue(&builder, p);
        }
        else
        {
            S8BuilderAppendBytesFromLValue(&builder, clip->sample.p);
        }
        S8BuilderAppendBytesFromLValue(&builder, clip->position);
        S8BuilderAppendBytesFromLValue(&builder, clip->level);
        S8BuilderAppendBytesFromLValue(&builder, clip->play_every_n_triggers);
        S8BuilderAppendBytesFromLValue(&builder, clip->triggers_count);
        for(U64 trigger_index = 0; trigger_index < clip->triggers_count; trigger_index += 1)
        {
            S8BuilderAppendBytesFromLValue(&builder, clip->triggers[trigger_index].clip.p);
            S8BuilderAppendBytesFromLValue(&builder, clip->triggers[trigger_index].kind);
        }
    }
    
    Assert(builder.len == builder.cap);
    S8 result = S8FromBuilder(builder);
    return result;
}

Function void
DeserialiseAppState(S8 serialised_data, AppState *state)
{
    Arena *permanent_arena = OS_TCtxGet()->permanent_arena;
    
    U64 sample_rate = TFromS8Advance(&serialised_data, U64);
    if(A_GetFormat().sample_rate != sample_rate)
    {
        OS_TCtxErrorPush(S8("Saved canvas has different sample rate to this devices default - TODO(tbt): resample on load"));
    }
    else
    {
        MemorySet(state->samples, 0, sizeof(state->samples));
        U64 samples_count = TFromS8Advance(&serialised_data, U64);
        for(U64 sample_index = 0; sample_index < samples_count; sample_index += 1)
        {
            U64 index = TFromS8Advance(&serialised_data, U64);
            U64 count = TFromS8Advance(&serialised_data, U64);
            state->samples[index].generation = 1;
            state->samples[index].count = count;
            state->samples[index].buffer = (void *)S8Clone(permanent_arena, S8Advance(&serialised_data, 2*sizeof(I16)*count)).buffer;
        }
        
        MemorySet(state->clips, 0, sizeof(state->clips));
        state->clips_first = &nil_clip;
        state->clips_free_list = &nil_clip;
        U64 clips_count = TFromS8Advance(&serialised_data, U64);
        for(U64 clip_index = 0; clip_index < clips_count; clip_index += 1)
        {
            U64 index = TFromS8Advance(&serialised_data, U64);
            Clip *clip = &state->clips[index];
            clip->generation = 1;
            clip->next = state->clips_first;
            if(&nil_clip != clip->next)
            {
                clip->next->prev = clip;
            }
            clip->prev = &nil_clip;
            state->clips_first = clip;
            clip->sample.p = TFromS8Advance(&serialised_data, U64);
            clip->sample.g = (clip->sample.p != ~((U64)0));
            clip->position = TFromS8Advance(&serialised_data, V2F);
            clip->level = TFromS8Advance(&serialised_data, F32);
            clip->play_every_n_triggers = TFromS8Advance(&serialised_data, U64);
            clip->triggers_count = TFromS8Advance(&serialised_data, U64);
            for(U64 trigger_index = 0; trigger_index < clip->triggers_count; trigger_index += 1)
            {
                clip->triggers[trigger_index].clip.p = TFromS8Advance(&serialised_data, U64);
                clip->triggers[trigger_index].clip.g = 1;
                clip->triggers[trigger_index].kind = TFromS8Advance(&serialised_data, TriggerKind);
            }
        }
        for(U64 clip_index = 0; clip_index < ArrayCount(state->clips); clip_index += 1)
        {
            Clip *clip = &state->clips[clip_index];
            if(0 == clip->generation)
            {
                clip->next = state->clips_free_list;
                state->clips_free_list = clip;
            }
        }
    }
}

Function void
WindowHookDraw(OpaqueHandle window)
{
    AppState *app = G_WindowUserDataGet(window);
    
    ArenaTemp scratch = OS_TCtxScratchBegin(0, 0);
    Arena *permanent_arena = OS_TCtxGet()->permanent_arena;
    F32 s = G_WindowScaleFactorGet(window);
    G_EventQueue *events = G_WindowEventQueueGet(window);
    V2F mouse_position = G_WindowMousePositionGet(window);
    
    //-NOTE(tbt): global hot-keys
    B32 should_pause = G_EventQueueHasKeyDown(events, G_Key_Space, 0, True);
    
    UI_Begin(app->ui);
    UI_Node *canvas = 0;
    UI_Row()
    {
        //-NOTE(tbt): build samples panel
        UI_Width(UI_SizePx(clip_height*2.0f, 1.0f))
            UI_Column()
        {
            UI_Height(UI_SizePx(40.0f, 1.0f)) UI_Font(ui_font_bold) UI_Label(S8("Samples:"));
            
            UI_SetNextChildrenLayout(Axis2_Y);
            UI_Node *samples_panel =
                UI_NodeMake(UI_Flags_AnimateSmoothScroll |
                            UI_Flags_Scrollable |
                            UI_Flags_ScrollChildrenY |
                            UI_Flags_OverflowY |
                            UI_Flags_ClampViewOffsetToOverflow,
                            S8("samples panel"));
            
            
            F32 height_per_row = clip_height;
            U64 rows_count = ArrayCount(app->samples);
            UI_Window window = {0};
            window.pixel_range.v = Scale2F(Range1F(-samples_panel->view_offset.y, -samples_panel->target_view_offset.y + samples_panel->computed_size.y).v, 1.0f / s);
            window.index_range = Range1U64(Min1U64(window.pixel_range.min / height_per_row, rows_count),
                                           Min1U64(window.pixel_range.max / height_per_row + 1, rows_count));
            window.space_before = window.index_range.min*height_per_row;
            window.space_after = (rows_count - window.index_range.max)*height_per_row;
            
            UI_Parent(samples_panel) UI_Height(UI_SizePx(height_per_row, 1.0f))
            {
                UI_Spacer(UI_SizePx(window.space_before, 1.0f));
                
                for(U64 sample_index = window.index_range.min; sample_index < window.index_range.max; sample_index += 1)
                {
                    Sample *sample = &app->samples[sample_index];
                    OpaqueHandle handle = { sample_index, sample->generation, };
                    
                    if(0 == sample->count)
                    {
                        UI_SetNextBgCol(ColFromHex(0xd8dee9ff));
                        UI_SetNextGrowth(-4.0f);
                        UI_SetNextCornerRadius(16.0f);
                        UI_Node *panel =
                            UI_NodeMakeFromFmt(UI_Flags_DrawFill |
                                               UI_Flags_DrawDropShadow |
                                               UI_Flags_DrawHotStyle |
                                               UI_Flags_DrawActiveStyle |
                                               UI_Flags_Pressable |
                                               UI_Flags_AnimateHot |
                                               UI_Flags_AnimateActive,
                                               "sample %llu %llu",
                                               handle.p, handle.g);
                        UI_Parent(panel)
                        {
                            UI_Label(S8("Add a sample."));
                        }
                        
                        UI_Use use = UI_UseFromNode(panel);
                        if(use.is_pressed)
                        {
                            S8List extensions = {0};
                            S8ListAppendExplicit(&extensions, &(S8Node){ .string = S8("wav"), });
                            S8 filename = OS_FilenameFromOpenDialogue(scratch.arena, S8("Add a sample"), extensions);
                            if(filename.len > 0)
                            {
                                LoadSample(filename, A_GetFormat().sample_rate, sample);
                            }
                        }
                    }
                    else
                    {
                        V4F col = ColourFromSampleHandle(app->samples, handle);
                        UI_SetNextBgCol(col);
                        UI_SetNextGrowth(-4.0f);
                        UI_SetNextCornerRadius(16.0f);
                        UI_Node *panel =
                            UI_NodeMakeFromFmt(UI_Flags_DrawFill |
                                               UI_Flags_DrawDropShadow |
                                               UI_Flags_DrawHotStyle |
                                               UI_Flags_DrawActiveStyle |
                                               UI_Flags_Pressable |
                                               UI_Flags_AnimateHot |
                                               UI_Flags_AnimateActive,
                                               "sample %llu %llu",
                                               handle.p, handle.g);
                        
                        UI_Parent(panel)
                        {
                            OpaqueHandle waveform_texture = WaveformTextureFromSampleHandle(permanent_arena, &app->waveform_texture_cache, app->samples, handle);
                            UI_SetNextWidth(UI_SizeFill());
                            UI_SetNextHeight(UI_SizeFill());
                            UI_SetNextFgCol(ColFromHex(0x4c566aff));
                            UI_SetNextTexture(waveform_texture);
                            UI_NodeMake(UI_Flags_DrawFill, S8(""));
                        }
                        
                        UI_Use use = UI_UseFromNode(panel);
                        if(use.is_pressed)
                        {
                            PlaySample(&app->mixer, handle, 0.7f);
                        }
                        else if(use.is_dragging && !OpaqueHandleMatch(handle, app->dragging))
                        {
                            app->dragging = handle;
                            ui->hot = UI_KeyNil();
                            ui->active = UI_KeyNil();
                        }
                    }
                }
                
                UI_Spacer(UI_SizePx(window.space_after, 1.0f));
            }
            
            UI_UseFromNode(samples_panel);
        }
        
        //-NOTE(tbt): build canvas
        {
            canvas =
                UI_NodeMake(UI_Flags_AnimateSmoothScroll |
                            UI_Flags_Pressable |
                            UI_Flags_Scrollable |
                            UI_Flags_ScrollChildren |
                            UI_Flags_Overflow,
                            S8("canvas"));
            
            UI_Parent(canvas)
            {
                for(Clip *clip = app->clips_first; &nil_clip != clip; clip = clip->next)
                {
                    OpaqueHandle handle =
                    {
                        .p = clip - app->clips,
                        .g = clip->generation,
                    };
                    
                    Sample *sample = SampleFromHandle(app->samples, clip->sample);
                    
                    S8 string = S8FromFmt(scratch.arena, "clip %llu %llu", handle.p, handle.g);
                    
                    UI_Node *panel;
                    UI_Growth(8.0f*clip->trigger_t)
                        panel = BuildClipUI(string,
                                            Scale2F(clip->position, s),
                                            clip->level,
                                            clip->triggered_counter,
                                            clip->play_every_n_triggers,
                                            &app->waveform_texture_cache,
                                            app->samples,
                                            clip->sample);
                    
                    for(U64 trigger_index = 0; trigger_index < clip->triggers_count; trigger_index += 1)
                    {
                        Trigger *trigger = &clip->triggers[trigger_index];
                        Clip *other = ClipFromHandle(app->clips, trigger->clip);
                        V2F a = ScreenFromWorld(clip->position, canvas);
                        V2F b = ScreenFromWorld(other->position, canvas);
                        F32 r = 16.0f;
                        Range2F aabb = Grow2F(Range2F(Mins2F(a, b), Maxs2F(a, b)), U2F(r));
                        B32 is_hovered = False;
                        if(HasPoint2F(aabb, mouse_position))
                        {
                            V2F n = Normalise2F(Sub2F(b, a));
                            V2F p = mouse_position;
                            F32 d2 = LengthSquared2F(Sub2F(Sub2F(p, a), Scale2F(n, Dot2F(Sub2F(p, a), n))));
                            is_hovered = (d2 < r*r*s*s);
                            if(is_hovered &&
                               G_WindowKeyStateGet(window, G_Key_D) &&
                               G_EventQueueHasKeyUp(events, G_Key_MouseButtonLeft, 0, True))
                            {
                                trigger->clip = (OpaqueHandle){0};
                            }
                        }
                        UI_Animate1F(&trigger->hovered_t, is_hovered, 15.0f);
                    }
                    
                    UI_Use use = UI_UseFromNode(panel);
                    if(use.is_dragging)
                    {
                        if(G_WindowKeyStateGet(window, G_Key_C))
                        {
                            Sample *sample = SampleFromHandle(app->samples, clip->sample);
                            app->new_trigger_from = clip;
                            app->new_trigger_kind = TriggerKind_Normal;
                            ui->hot = UI_KeyNil();
                            ui->active = UI_KeyNil();
                        }
                        else if(G_WindowKeyStateGet(window, G_Key_L))
                        {
                            app->new_trigger_from = clip;
                            app->new_trigger_kind = TriggerKind_Link;
                            ui->hot = UI_KeyNil();
                            ui->active = UI_KeyNil();
                        }
                        else if(G_WindowKeyStateGet(window, G_Key_E))
                        {
                            F32 distance = Abs1F(mouse_position.y - ScreenFromWorld(clip->position, canvas).y);
                            clip->play_every_n_triggers = Max1U64(distance / 32.0f*s, 1);
                        }
                        else if(G_WindowKeyStateGet(window, G_Key_V))
                        {
                            clip->level = Clamp1F(clip->level + use.drag_delta.y / (clip_height*s), 0.0f, 1.0f);
                        }
                        else
                        {
                            clip->position = Add2F(clip->position, Scale2F(use.drag_delta, 1.0f / s));
                            
                            Range2F inner_window_rect = Grow2F(G_WindowClientRectGet(window), U2F(-16.0f*s));
                            if(!HasPoint2F(inner_window_rect, mouse_position))
                            {
                                // TODO(tbt): only delete on mouse up, to be a bit more forgiving
                                clip->generation = 0;
                            }
                        }
                    }
                    else if(use.is_pressed)
                    {
                        TriggerClipIn(clip, 0);
                    }
                    else if(use.is_left_up)
                    {
                        Sample *dragging = SampleFromHandle(app->samples, app->dragging);
                        if(&nil_sample != dragging)
                        {
                            clip->sample = app->dragging;
                            app->dragging = (OpaqueHandle){0};
                        }
                        else if(&nil_clip != app->new_trigger_from)
                        {
                            app->new_trigger_from->triggers[app->new_trigger_from->triggers_count].kind = app->new_trigger_kind;
                            app->new_trigger_from->triggers[app->new_trigger_from->triggers_count].clip = handle;
                            app->new_trigger_from->triggers_count += 1;
                            app->new_trigger_from = &nil_clip;
                        }
                    }
                }
            }
            
            UI_Use use = UI_UseFromNode(canvas);
            if(use.is_dragging)
            {
                canvas->view_offset = canvas->target_view_offset = Add2F(canvas->target_view_offset, use.drag_delta);
            }
            else if(use.is_double_clicked)
            {
                Clip *clip = app->clips_free_list;
                if(clip != &nil_clip)
                {
                    app->clips_free_list = app->clips_free_list->next;
                    clip->sample = (OpaqueHandle){0};
                    clip->position = WorldFromScreen(mouse_position, canvas);
                    clip->play_every_n_triggers = 1;
                    clip->level = 0.5f;
                    clip->prev = &nil_clip;
                    clip->next = app->clips_first;
                    if(&nil_clip != clip->next)
                    {
                        clip->next->prev = clip;
                    }
                    app->clips_first = clip;
                    clip->generation += 1;
                }
            }
        }
        
        //-NOTE(tbt): remove deleted clips
        {
            Clip *next = &nil_clip;
            for(Clip *clip = app->clips_first; &nil_clip != clip; clip = next)
            {
                next = clip->next;
                if(0 == clip->generation)
                {
                    if(&nil_clip == clip->prev)
                    {
                        app->clips_first = clip->next;
                        clip->next->prev = &nil_clip;
                    }
                    else
                    {
                        clip->prev->next = clip->next;
                        if(&nil_clip != clip->next) { clip->next->prev = clip->prev; }
                    }
                    MemorySet(clip, 0, sizeof(*clip));
                    clip->next = app->clips_free_list;
                    app->clips_free_list = clip;
                }
            }
        }
        
        
        //-NOTE(tbt): drop dragged sample onto canvas as new clip
        if(!G_WindowKeyStateGet(window, G_Key_MouseButtonLeft))
        {
            Sample *dragging = SampleFromHandle(app->samples, app->dragging);
            if(&nil_sample != dragging)
            {
                Clip *clip = app->clips_free_list;
                if(clip != &nil_clip)
                {
                    app->clips_free_list = app->clips_free_list->next;
                    clip->sample = app->dragging;
                    clip->position = WorldFromScreen(mouse_position, canvas);
                    clip->play_every_n_triggers = 1;
                    clip->level = 0.5f;
                    clip->prev = &nil_clip;
                    clip->next = app->clips_first;
                    if(&nil_clip != clip->next)
                    {
                        clip->next->prev = clip;
                    }
                    app->clips_first = clip;
                    clip->generation += 1;
                }
            }
            
            app->dragging = (OpaqueHandle){0};
            app->new_trigger_from = &nil_clip;
        }
        
        //-NOTE(tbt): build dragged sample
        if(0 != app->dragging.g)
        {
            BuildClipUI(S8("dragging in new clip"), mouse_position, 0, 0, 0, &app->waveform_texture_cache, app->samples, app->dragging);
        }
    }
    
    ui->default_cursor_kind = G_CursorKind_Default;
    if(&nil_clip != app->new_trigger_from)
    {
        ui->default_cursor_kind = G_CursorKind_Hidden;
    }
    
    UI_End();
    
    //-NOTE(tbt): play triggered clips
    U64 now = OS_TimeInMicroseconds();
    for(Clip *clip = app->clips_first; &nil_clip != clip; clip = clip->next)
    {
        UI_Animate1F(&clip->trigger_t, 0.0f, 1.0f);
        
        if(should_pause)
        {
            clip->triggered_count = 0;
        }
        
        U64 finished_count = 0;
        for(U64 triggered_index = 0; triggered_index < clip->triggered_count; triggered_index += 1)
        {
            if(now >= clip->triggered[triggered_index])
            {
                clip->triggered_counter += 1;
                clip->trigger_t = 1.0f;
                
                if(clip->triggered_counter >= clip->play_every_n_triggers)
                {
                    PlaySample(&app->mixer, clip->sample, clip->level);
                    clip->triggered_counter = 0;
                    
                    for(U64 trigger_index = 0; trigger_index < clip->triggers_count; trigger_index += 1)
                    {
                        Trigger *trigger = &clip->triggers[trigger_index];
                        OpaqueHandle to_trigger_handle = trigger->clip;
                        Clip *to_trigger = ClipFromHandle(app->clips, to_trigger_handle);
                        U64 trigger_clip_in = 0;
                        if(TriggerKind_Normal == trigger->kind)
                        {
                            trigger_clip_in = Length2F(Sub2F(clip->position, to_trigger->position))*microseconds_per_unit;
                        }
                        TriggerClipIn(to_trigger, trigger_clip_in);
                    }
                }
                
                finished_count += 1;
            }
            else
            {
                break;
            }
        }
        MemoryMove(clip->triggered, &clip->triggered[finished_count], (clip->triggered_count - finished_count)*sizeof(U64));
        clip->triggered_count -= finished_count;
    }
    
    //-NOTE(tbt): remove triggers marked as deleted
    for(Clip *clip = app->clips_first; &nil_clip != clip; clip = clip->next)
    {
        for(U64 trigger_index = 0; trigger_index < clip->triggers_count; trigger_index += 1)
        {
            Trigger *trigger = &clip->triggers[trigger_index];
            Clip *clip_to_trigger = ClipFromHandle(app->clips, trigger->clip);
            if(&nil_clip == clip_to_trigger || clip == clip_to_trigger)
            {
                MemoryMove(trigger, &clip->triggers[trigger_index + 1], (clip->triggers_count - trigger_index - 1)*sizeof(Trigger));
                clip->triggers_count -= 1;
                trigger_index -= 1;
            }
        }
    }
    
    //-NOTE(tbt): draw arrows for triggers
    LINE_SegmentListClear(&app->line_segments);
    for(Clip *clip = app->clips_first; &nil_clip != clip; clip = clip->next)
    {
        for(U64 trigger_index = 0; trigger_index < clip->triggers_count; trigger_index += 1)
        {
            Trigger *trigger = &clip->triggers[trigger_index];
            Clip *other = ClipFromHandle(app->clips, trigger->clip);
            V2F from_position = ScreenFromWorld(clip->position, canvas);
            V2F to_position = ScreenFromWorld(other->position, canvas);
            V4F col = (TriggerKind_Link == trigger->kind) ? ColFromHex(0xbf616a88
                                                                       ) : ColFromHex(0x3b4252ff);
            if(G_WindowKeyStateGet(window, G_Key_D))
            {
                col = ColMix(col, ColFromHex(0xbf616aff), trigger->hovered_t);
            }
            DrawArrowFlags flags = (TriggerKind_Link == trigger->kind) ? 0 : DrawArrowFlags_Dashed;
            DrawArrow(window, &app->line_segments, from_position, to_position, col, 3.0f*s, canvas->clipped_rect, flags);
        }
    }
    if(&nil_clip != app->new_trigger_from)
    {
        V2F from_position = ScreenFromWorld(app->new_trigger_from->position, canvas);
        DrawArrowFlags flags = (TriggerKind_Link == app->new_trigger_kind) ? 0 : DrawArrowFlags_Dashed;
        DrawArrow(window, &app->line_segments, from_position, mouse_position, ColFromHex(0xa3be8cff), 6.0f*s, canvas->clipped_rect, flags);
    }
    LINE_SegmentListSubmit(window, app->line_segments, canvas->clipped_rect);
    
    ArenaTempEnd(scratch);
}

Function void
WindowHookClose(OpaqueHandle window)
{
    ArenaTemp scratch = OS_TCtxScratchBegin(0, 0);
    AppState *app = G_WindowUserDataGet(window);
    S8 serialised_state = SerialiseAppState(scratch.arena, app);
    S8 save_file_path = SaveFilename(scratch.arena);
    OS_FileMove(save_file_path, S8("pitchfork_canvas_backup"));
    OS_FileWriteEntire(save_file_path, serialised_state);
    ArenaTempEnd(scratch);
}

Function void
DrawArrow(OpaqueHandle window, LINE_SegmentList *segments, V2F from, V2F to, V4F col, F32 width, Range2F mask, DrawArrowFlags flags)
{
    ArenaTemp scratch = OS_TCtxScratchBegin(0, 0);
    
    V2F v = Sub2F(to, from);
    F32 l = Length2F(v);
    V2F i = Scale2F(v, 1.0f / l);
    V2F j = Tangent2F(i);
    
    U64 arrow_segments_count = 0;
    LINE_Segment *arrow_segments = PushArray(scratch.arena, LINE_Segment, 3);
#define AppendArrowSegment(...) Statement(arrow_segments[arrow_segments_count] = (LINE_Segment){ __VA_ARGS__ }; arrow_segments_count += 1;)
    
    if(flags & DrawArrowFlags_Dashed)
    {
        F32 dash_length = 32.0f + width;
        F32 spacing = 4.0f + width;
        U64 dashes_count = Ceil1F(l / dash_length);
        PushArray(scratch.arena, LINE_Segment, dashes_count);
        for(U64 dash_index = 0; dash_index < dashes_count; dash_index += 1)
        {
            V2F a = Add2F(from, Scale2F(i, dash_length*dash_index));
            V2F b = Add2F(from, Scale2F(i, Clamp1F(dash_length*(dash_index + 1) - spacing, 0.0f, l)));
            AppendArrowSegment(.a = { .pos = a, .col = col, .width = width, },
                               .b = { .pos = b, .col = col, .width = width, });
        }
    }
    else
    {
        AppendArrowSegment(.a = { .pos = from, .col = col, .width = width, },
                           .b = { .pos = to, .col = col, .width = width, });
    }
    V2F head_end_1 = Add2F(to, Add2F(Scale2F(i, -16.0f), Scale2F(j, -16.0f)));
    AppendArrowSegment(.a = { .pos = to, .col = col, .width = width, },
                       .b = { .pos = head_end_1, .col = col, .width = width, });
    V2F head_end_2 = Add2F(to, Add2F(Scale2F(i, -16.0f), Scale2F(j, 16.0f)));
    AppendArrowSegment(.a = { .pos = to, .col = col, .width = width, },
                       .b = { .pos = head_end_2, .col = col, .width = width, });
    
#undef AppendArrowSegment
    
    for(U64 segment_index = 0; segment_index < arrow_segments_count; segment_index += 1)
    {
        while(!LINE_SegmentListAppend(segments, arrow_segments[segment_index]))
        {
            LINE_SegmentListSubmit(window, *segments, mask);
            LINE_SegmentListClear(segments);
        }
    }
    
    ArenaTempEnd(scratch);
}

Function void
TriggerClipIn(Clip *clip, U64 microseconds)
{
    if(&nil_clip != clip && clip->triggered_count + 1 < ArrayCount(clip->triggered))
    {
        U64 t = OS_TimeInMicroseconds() + microseconds;
        if(0 == clip->triggered_count)
        {
            clip->triggered[0] = t;
        }
        else
        {
            for(U64 triggered_index = 0; triggered_index < clip->triggered_count + 1; triggered_index += 1)
            {
                if(clip->triggered[triggered_index] > t || triggered_index == clip->triggered_count)
                {
                    MemoryMove(&clip->triggered[triggered_index + 1], &clip->triggered[triggered_index], (clip->triggered_count - triggered_index)*sizeof(U64));
                    clip->triggered[triggered_index] = t;
                    break;
                }
            }
        }
        clip->triggered_count += 1;
    }
}

Function void
PlaySample(Mixer *mixer, OpaqueHandle sample, F32 level)
{
    Arena *permanent_arena = OS_TCtxGet()->permanent_arena;
    
    OS_SemaphoreWait(mixer->lock);
    Sound *sound = mixer->free_list;
    if(0 == sound)
    {
        sound = PushArray(permanent_arena, Sound, 1);
    }
    else
    {
        mixer->free_list = mixer->free_list->next;
    }
    sound->sample = sample;
    sound->level = level;
    sound->cursor = 0;
    sound->next = OS_InterlockedExchange1Ptr(&mixer->sounds, sound);
    OS_SemaphoreSignal(mixer->lock);
}

Function void
MixerThreadProc(void *user_data)
{
    Mixer *mixer = user_data;
    
    U64 n = 0;
    
    while(!mixer->should_quit)
    {
        A_Buffer buffer = A_Lock();
        
        U64 write_count = Min1U64(buffer.samples_count, buffer.format.sample_rate / 10);
        
        OS_SemaphoreWait(mixer->lock);
        Sound *volatile *from = &mixer->sounds;
        Sound *next = 0;
        for(Sound *sound = mixer->sounds; 0 != sound; sound = next)
        {
            Sample *sample = SampleFromHandle(mixer->samples, sound->sample);
            next = sound->next;
            if(sound->cursor > sample->count)
            {
                *from = sound->next;
                sound->next = mixer->free_list;
                mixer->free_list = sound;
            }
            else
            {
                from = &sound->next;
            }
        }
        OS_SemaphoreSignal(mixer->lock);
        
        MemorySet(buffer.samples, 0, write_count*buffer.format.channels_count*sizeof(F32));
        
        for(Sound *sound = mixer->sounds; 0 != sound; sound = sound->next)
        {
            Sample *sample = SampleFromHandle(mixer->samples, sound->sample);
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
    }
}

EntryPoint
{
    OS_Init();
    G_Init();
    R2D_Init();
    LINE_Init();
    FONT_Init();
    A_Init(A_UseDefaultFormat);
    
    Arena *permanent_arena = OS_TCtxGet()->permanent_arena;
    
#if BuildModeDebug
    {
        ArenaTemp scratch = OS_TCtxScratchBegin(0, 0);
        OpaqueHandle debug_info = DBG_InfoAlloc(OS_S8FromStdPath(scratch.arena, OS_StdPath_ExecutableFile));
        debug_symbols = DBG_SymTableMake(permanent_arena, 1024);
        DBG_SymTableAddAllSymbolsFromCompiland(permanent_arena, &debug_symbols, debug_info, S8("C:\\programming\\build\\pitchfork_main.obj")); // TODO(tbt): don't hard code
        DBG_InfoRelease(debug_info);
        ArenaTempEnd(scratch);
    }
#endif
    
    AppState app_state = {0};
    app_state.line_segments = LINE_PushSegmentList(permanent_arena, LINE_SegmentListCap);
    app_state.mixer.samples = app_state.samples;
    app_state.mixer.lock = OS_SemaphoreAlloc(1);
    app_state.mixer.thread = OS_ThreadStart(MixerThreadProc, &app_state.mixer);
    app_state.clips_first = &nil_clip;
    app_state.clips_free_list = &nil_clip;
    
    G_WindowHooks window_hooks =
    {
        WindowHookOpen,
        WindowHookDraw,
        WindowHookClose,
    };
    main_window = G_WindowOpen(S8("Pitchfork"), V2I(1024, 768), window_hooks, &app_state);
    
    G_MainLoop();
    
    OS_InterlockedExchange1I(&app_state.mixer.should_quit, True);
    OS_ThreadJoin(app_state.mixer.thread);
    
    A_Cleanup();
    LINE_Cleanup();
    R2D_Cleanup();
    G_Cleanup();
    OS_Cleanup();
}

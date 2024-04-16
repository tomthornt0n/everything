
#define COBJMACROS
#define WIN32_LEAN_AND_MEAN
#include <initguid.h>
#include <objbase.h>
#include <uuids.h>
#include <avrt.h>
#include <audioclient.h>
#include <mmdeviceapi.h>

#pragma comment (lib, "avrt.lib")
#pragma comment (lib, "ole32.lib")
#pragma comment (lib, "onecore.lib")

typedef struct WASAPI_State WASAPI_State;
struct WASAPI_State
{
    WAVEFORMATEX *buffer_format;
    IAudioClient *client;
	HANDLE event;
    B32 stop;
    B32 lock;
    U8 *buffer_1;
    U8 *buffer_2;
    U32 out_size;
	U32 buffer_used;
	B32 buffer_first_lock;
    U64 ringbuffer_size;
    U64 volatile ringbuffer_read_offset;
    U64 volatile ringbuffer_lock_offset;
	U64 volatile ringbuffer_write_offset;
    
    B32 is_initialised;
};

Global WASAPI_State wasapi_state = {0};

Global HANDLE wasapi_audio_thread = 0;

DEFINE_GUID(CLSID_MMDeviceEnumerator, 0xbcde0395, 0xe52f, 0x467c, 0x8e, 0x3d, 0xc4, 0x57, 0x92, 0x91, 0x69, 0x2e);
DEFINE_GUID(IID_IMMDeviceEnumerator,  0xa95664d2, 0x9614, 0x4f35, 0xa7, 0x46, 0xde, 0x8d, 0xb6, 0x36, 0x17, 0xe6);
DEFINE_GUID(IID_IAudioClient,         0x1cb9ad4c, 0xdbfa, 0x4c32, 0xb1, 0x78, 0xc2, 0xf5, 0x68, 0xa7, 0x03, 0xb2);
DEFINE_GUID(IID_IAudioClient3,        0x7ed4ee07, 0x8e67, 0x4cd4, 0x8c, 0x1a, 0x2b, 0x7a, 0x59, 0x87, 0xad, 0x42);
DEFINE_GUID(IID_IAudioRenderClient,   0xf294acfc, 0x3146, 0x4483, 0xa7, 0xbf, 0xad, 0xdc, 0xa7, 0xc2, 0x60, 0xe2);

Function void
WASAPI_LockBuffer(void)
{
    while(InterlockedCompareExchange(&wasapi_state.lock, True, False) != False)
	{
		B32 locked = False;
		WaitOnAddress(&wasapi_state.lock, &locked, sizeof(locked), INFINITE);
	}
}

Function void
WASAPI_UnlockBuffer(void)
{
    InterlockedExchange(&wasapi_state.lock, False);
	WakeByAddressSingle(&wasapi_state.lock);
}

Function DWORD CALLBACK
WASAPI_AudioThreadProc(LPVOID param)
{
    OS_ThreadContext tctx;
    LONG logical_thread_index;
    ReleaseSemaphore(w32_threads_count, 1, &logical_thread_index);
    OS_ThreadContextAlloc(&tctx, logical_thread_index + 1);
    OS_TCtxSet(&tctx);
    
    OS_ThreadNameSet(S8(ApplicationNameString ": WASAPI"));
    
    // TODO(tbt): handle errors better than just asserting
    
	DWORD task;
	HANDLE handle = AvSetMmThreadCharacteristicsW(L"Pro Audio", &task);
    Assert(0 != handle);
    
	IAudioClient *client = wasapi_state.client;
    
	IAudioRenderClient *playback;
	HRESULT get_service = IAudioClient_GetService(client, &IID_IAudioRenderClient, (void **)&playback);
    Assert(SUCCEEDED(get_service));
    
	U32 buffer_samples;
	HRESULT get_buffer_size = IAudioClient_GetBufferSize(client, &buffer_samples);
    Assert(SUCCEEDED(get_buffer_size));
    
	HRESULT start = IAudioClient_Start(client);
    Assert(SUCCEEDED(start));
    
	U32 bytes_per_sample = wasapi_state.buffer_format->nBlockAlign;
	U32 ringbuffer_mask = wasapi_state.ringbuffer_size - 1;
	U8 *input = wasapi_state.buffer_1;
    
	while(WaitForSingleObject(wasapi_state.event, INFINITE) == WAIT_OBJECT_0)
	{
		if(InterlockedExchange(&wasapi_state.stop, False))
		{
			break;
		}
        
		U32 padding_samples;
		HRESULT get_padding = IAudioClient_GetCurrentPadding(client, &padding_samples);
        Assert(SUCCEEDED(get_padding));
        
		U8 *output;
		U32 max_output_samples = buffer_samples - padding_samples;
		HRESULT get_buffer = IAudioRenderClient_GetBuffer(playback, max_output_samples, &output);
        Assert(SUCCEEDED(get_buffer));
        
        WASAPI_LockBuffer();
        
		U32 read_offset = wasapi_state.ringbuffer_read_offset;
		U32 write_offset = wasapi_state.ringbuffer_write_offset;
        
		U32 available_size = write_offset - read_offset;
		U32 available_samples = available_size / bytes_per_sample;
		U32 use_samples = Min1U64(available_samples, max_output_samples);
		U32 use_size = use_samples*bytes_per_sample;
        
		wasapi_state.ringbuffer_lock_offset = read_offset + use_size;
        
		U32 submit_count = use_samples ? use_samples : max_output_samples;
		DWORD flags = use_samples ? 0 : AUDCLNT_BUFFERFLAGS_SILENT;
        
		wasapi_state.buffer_used += submit_count;
        
		WASAPI_UnlockBuffer();
        
		MemoryCopy(output, input + (read_offset & ringbuffer_mask), use_size);
		InterlockedAdd64(&wasapi_state.ringbuffer_read_offset, use_size);
		
        HRESULT release_buffer = IAudioRenderClient_ReleaseBuffer(playback, submit_count, flags);
        Assert(SUCCEEDED(release_buffer));
	}
    
    HRESULT stop = IAudioClient_Stop(client);
	Assert(SUCCEEDED(stop));
    
    IAudioRenderClient_Release(playback);
    
	AvRevertMmThreadCharacteristics(handle);
    
    OS_ThreadContextRelease(&tctx);
    WaitForSingleObject(w32_threads_count, 0);
    
    return 0;
}

Function void
A_Init(A_Format format)
{
    Assert(0 == format.channels_count ||
           1 == format.channels_count ||
           2 == format.channels_count);
    
    
    // NOTE(tbt): enumerate available devices
    IMMDeviceEnumerator *enumerator = 0;
    {
        HRESULT hr = CoCreateInstance(&CLSID_MMDeviceEnumerator, 0, CLSCTX_ALL, &IID_IMMDeviceEnumerator, (void **)&enumerator);
        if(!SUCCEEDED(hr))
        {
            OS_TCtxErrorPush(S8("Error while setting up audio - Could not create device enumerator instance"));
            enumerator = 0;
        }
    }
    
    // NOTE(tbt): get default device
    IMMDevice *device = 0;
    if(0 != enumerator)
    {
        HRESULT hr = IMMDeviceEnumerator_GetDefaultAudioEndpoint(enumerator, eRender, eConsole, &device);
        if(SUCCEEDED(hr))
        {
            IMMDeviceEnumerator_Release(enumerator);
        }
        else
        {
            OS_TCtxErrorPush(S8("Error while setting up audio - Could not get default audio device"));
            device = 0;
        }
    }
    
    // NOTE(tbt): create audio client
    IAudioClient *client = 0;
    if(0 != device)
    {
        HRESULT hr = IMMDevice_Activate(device, &IID_IAudioClient, CLSCTX_ALL, 0, (void **)&client);
        if(SUCCEEDED(hr))
        {
            IMMDevice_Release(device);
        }
        else
        {
            OS_TCtxErrorPush(S8("Error while setting up audio - Could not create audio client"));
            client = 0;
        }
    }
    
    // NOTE(tbt): setup audio format
    WAVEFORMATEX* wfx = 0;
    B32 is_using_default_format = False;
    if(0 != client)
    {
        DWORD channels_mask = 0;
        if(1 == format.channels_count)
        {
            channels_mask = SPEAKER_FRONT_CENTER;
        }
        else if(2 == format.channels_count)
        {
            channels_mask = (SPEAKER_FRONT_LEFT |
                             SPEAKER_FRONT_RIGHT);
        }
        
        WAVEFORMATEXTENSIBLE format_ex =
        {
            .Format =
            {
                .wFormatTag = WAVE_FORMAT_EXTENSIBLE,
                .nChannels = (WORD)format.channels_count,
                .nSamplesPerSec = (WORD)format.sample_rate,
                .nAvgBytesPerSec = (DWORD)(format.sample_rate*format.channels_count*sizeof(F32)),
                .nBlockAlign = (WORD)(format.channels_count*sizeof(F32)),
                .wBitsPerSample = (WORD)(8*sizeof(float)),
                .cbSize = sizeof(format_ex) - sizeof(format_ex.Format),
            },
            .Samples.wValidBitsPerSample = 8*sizeof(F32),
            .dwChannelMask = channels_mask,
            .SubFormat = MEDIASUBTYPE_IEEE_FLOAT,
        };
        
        if(format.sample_rate == 0 ||
           format.channels_count == 0)
        {
            HRESULT hr = IAudioClient_GetMixFormat(client, &wfx);
            if(SUCCEEDED(hr))
            {
                is_using_default_format = True;
            }
            else
            {
                OS_TCtxErrorPush(S8("Error while setting up audio - Could not get default audio format"));
                wfx = 0;
            }
        }
        else
        {
            wfx = &format_ex.Format;
        }
    }
    
    // NOTE(tbt): initialise client
    B32 is_client_initialised = False;
    if(0 != wfx)
    {
        // NOTE(tbt): try to use newer functionality if available
        {
            IAudioClient3 *client_3 = 0;
            HRESULT hr = IAudioClient_QueryInterface(client, &IID_IAudioClient3, (void **)&client_3);
            if(SUCCEEDED(hr))
            {
                U32 default_period_samples;
                U32 fundamental_period_samples;
                U32 min_period_samples;
                U32 max_period_samples;
                HRESULT get_period =
                    IAudioClient3_GetSharedModeEnginePeriod(client_3, wfx,
                                                            &default_period_samples,
                                                            &fundamental_period_samples,
                                                            &min_period_samples,
                                                            &max_period_samples);
                if(SUCCEEDED(get_period))
                {
                    HRESULT initialise =
                        IAudioClient3_InitializeSharedAudioStream(client_3,
                                                                  AUDCLNT_STREAMFLAGS_EVENTCALLBACK,
                                                                  min_period_samples,
                                                                  wfx, 0);
                    
                    is_client_initialised = SUCCEEDED(initialise);
                }
                
                IAudioClient3_Release(client_3);
            }
        }
        
        // NOTE(tbt): otherwise initialise normally
        if(!is_client_initialised)
        {
            REFERENCE_TIME duration;
            HRESULT get_period = IAudioClient_GetDevicePeriod(client, &duration, 0);
            
            DWORD flags = AUDCLNT_STREAMFLAGS_EVENTCALLBACK | AUDCLNT_STREAMFLAGS_AUTOCONVERTPCM | AUDCLNT_STREAMFLAGS_SRC_DEFAULT_QUALITY;
            HRESULT initialise =
                IAudioClient_Initialize(client,
                                        AUDCLNT_SHAREMODE_SHARED,
                                        AUDCLNT_STREAMFLAGS_EVENTCALLBACK |
                                        AUDCLNT_STREAMFLAGS_AUTOCONVERTPCM |
                                        AUDCLNT_STREAMFLAGS_SRC_DEFAULT_QUALITY,
                                        duration,
                                        0, wfx, 0);
            
            is_client_initialised = (SUCCEEDED(get_period) &&
                                     SUCCEEDED(initialise));
        }
    }
    
    // NOTE(tbt): use format
    if(is_client_initialised && !is_using_default_format)
    {
        IAudioClient_GetMixFormat(client, &wfx);
    }
    
    // NOTE(tbt): get buffer size
    U32 out_size = 0;
    if(0 != wfx)
    {
        U32 buffer_samples;
        HRESULT hr = IAudioClient_GetBufferSize(client, &buffer_samples);
        if(SUCCEEDED(hr))
        {
            out_size = buffer_samples*wfx->nBlockAlign;
        }
        else
        {
            OS_TCtxErrorPush(S8("Error while setting up audio - Could not get buffer size"));
        }
    }
    
    // NOTE(tbt): setup event handle
    HANDLE event = INVALID_HANDLE_VALUE;
    if(0 != out_size)
    {
        event = CreateEventW(0, 0, 0, 0);
        HRESULT hr = IAudioClient_SetEventHandle(client, event);
        if(!SUCCEEDED(hr))
        {
            OS_TCtxErrorPush(S8("Error while setting up audio - Could not get buffer size"));
            event = INVALID_HANDLE_VALUE;
        }
    }
    
    // NOTE(tbt): setup magic ringbuffer
    U8 *buffer_1 = 0;
    U8 *buffer_2 = 0;
    U64 ringbuffer_size = 0;
    if(INVALID_HANDLE_VALUE != event)
    {
        ringbuffer_size = NextPowerOfTwo1U(Max1U64(1024, wfx->nAvgBytesPerSec));
        U8 *placeholder_1 = VirtualAlloc2(0, 0, 2*ringbuffer_size, MEM_RESERVE | MEM_RESERVE_PLACEHOLDER, PAGE_NOACCESS, 0, 0);
        U8 *placeholder_2 = placeholder_1 + ringbuffer_size;
        VirtualFree(placeholder_1, ringbuffer_size, MEM_RELEASE | MEM_PRESERVE_PLACEHOLDER);
        
        HANDLE section = CreateFileMappingW(INVALID_HANDLE_VALUE, 0, PAGE_READWRITE, 0, ringbuffer_size, 0);
        buffer_1 = MapViewOfFile3(section, 0, placeholder_1, 0, ringbuffer_size, MEM_REPLACE_PLACEHOLDER, PAGE_READWRITE, 0, 0);
        buffer_2 = MapViewOfFile3(section, 0, placeholder_2, 0, ringbuffer_size, MEM_REPLACE_PLACEHOLDER, PAGE_READWRITE, 0, 0);
        
        VirtualFree(placeholder_1, 0, MEM_RELEASE);
        VirtualFree(placeholder_2, 0, MEM_RELEASE);
        CloseHandle(section);
        
        if(0 == buffer_1 || 0 == buffer_2)
        {
            OS_TCtxErrorPush(S8("Error while setting up audio - Could not allocate ring buffer"));
            ringbuffer_size = 0;
        }
    }
    
    if(0 != ringbuffer_size)
    {
        // NOTE(tbt): save to global
        wasapi_state.buffer_format = wfx;
        wasapi_state.client = client;
        wasapi_state.event = event;
        wasapi_state.buffer_1 = buffer_1;
        wasapi_state.buffer_2 = buffer_2;
        wasapi_state.out_size = out_size;
        wasapi_state.buffer_first_lock = True;
        wasapi_state.ringbuffer_size = ringbuffer_size;
        wasapi_state.is_initialised = True;
        
        // NOTE(tbt): spawn audio thread
        wasapi_audio_thread = CreateThread(0, 0, &WASAPI_AudioThreadProc, 0, 0, 0);
    }
}

Function void
A_Cleanup(void)
{
    InterlockedExchange(&wasapi_state.stop, True);
	SetEvent(wasapi_state.event);
	WaitForSingleObject(wasapi_audio_thread, INFINITE);
    CloseHandle(wasapi_audio_thread);
}

Function A_Format
A_GetFormat(void)
{
    A_Format result =
    {
        .sample_rate = wasapi_state.buffer_format->nSamplesPerSec,
        .channels_count = wasapi_state.buffer_format->nChannels,
    };
    return result;
}

Function A_Buffer
A_Lock(void)
{
    A_Buffer result = {0};
    
    if(wasapi_state.is_initialised)
    {
        U32 bytes_per_sample = wasapi_state.buffer_format->nBlockAlign;
        U32 ringbuffer_size = wasapi_state.ringbuffer_size;
        U32 ringbuffer_mask = ringbuffer_size - 1;
        U32 out_size = wasapi_state.out_size;
        
        WASAPI_LockBuffer();
        
        U32 read_offset = wasapi_state.ringbuffer_read_offset;
        U32 lock_offset = wasapi_state.ringbuffer_lock_offset;
        U32 write_offset = wasapi_state.ringbuffer_write_offset;
        
        U32 used_size = lock_offset - read_offset;
        
        if (used_size < out_size)
        {
            U32 available_size = write_offset - read_offset;
            
            used_size = Min(out_size, available_size);
            wasapi_state.ringbuffer_lock_offset = lock_offset = read_offset + used_size;
        }
        
        U32 write_size = ringbuffer_size - used_size;
        
        wasapi_state.ringbuffer_write_offset = lock_offset;
        
        U64 play_count = wasapi_state.buffer_first_lock ? 0 : wasapi_state.buffer_used;
        wasapi_state.buffer_first_lock = False;
        wasapi_state.buffer_used = 0;
        
        WASAPI_UnlockBuffer();
        
        result.format.sample_rate = wasapi_state.buffer_format->nSamplesPerSec;
        result.format.channels_count = wasapi_state.buffer_format->nChannels;
        result.samples = wasapi_state.buffer_1 + (lock_offset & ringbuffer_mask);
        result.samples_count = write_size / bytes_per_sample;
        result.played_count = play_count;
    }
    
    return result;
}

Function void
A_Unlock(U64 written_samples)
{
    if(wasapi_state.is_initialised)
    {
        U32 bytes_per_sample = wasapi_state.buffer_format->nBlockAlign;
        U64 write_size = written_samples*bytes_per_sample;
        InterlockedAdd64(&wasapi_state.ringbuffer_write_offset, write_size);
    }
}


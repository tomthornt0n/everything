
#include <wincodec.h>

DEFINE_GUID(IID_IWICImagingFactory, 0xec5ec8a9, 0xc395, 0x4314, 0x9c, 0x77, 0x54, 0xd7, 0xa9, 0x35, 0xff, 0x70);

Global IWICImagingFactory *w32_wic_factory = 0;

Function void
IMG_Init(void)
{
    HRESULT hr =
        CoCreateInstance(&CLSID_WICImagingFactory,
                         0, CLSCTX_INPROC_SERVER,
                         &IID_IWICImagingFactory,
                         (void **)&w32_wic_factory);
    if(!SUCCEEDED(hr))
    {
        OS_TCtxErrorPush(S8("Could not creat WIC factory instance"));
        w32_wic_factory = 0;
    }
}

Function void
IMG_Cleanup(void)
{
    IWICImagingFactory_Release(w32_wic_factory);
}

Function IMG_Data
IMG_DataFromFilename(Arena *arena, S8 filename)
{
    ArenaTemp scratch = OS_TCtxScratchBegin(&arena, 1);
    
    S16 filename_s16 = S16FromS8(scratch.arena, filename);
    
    IWICBitmapDecoder *decoder = 0;
    if(0 != w32_wic_factory)
    {
        HRESULT hr =
            IWICImagingFactory_CreateDecoderFromFilename(w32_wic_factory,
                                                         filename_s16.buffer, 0,
                                                         GENERIC_READ,
                                                         WICDecodeMetadataCacheOnDemand,
                                                         &decoder);
        if(!SUCCEEDED(hr))
        {
            OS_TCtxErrorPushFmt("Error decoding image '%.*s': could not create decoder", FmtS8(filename));
            decoder = 0;
        }
    }
    
    IWICBitmapFrameDecode *frame = 0;
    if(0 != decoder)
    {
        HRESULT hr = IWICBitmapDecoder_GetFrame(decoder, 0, &frame);
        if(!SUCCEEDED(hr))
        {
            OS_TCtxErrorPushFmt("Error decoding image '%.*s': could not get first frame", FmtS8(filename));
            frame = 0;
        }
    }
    
    IWICFormatConverter *converted_source_bitmap = 0;
    if(0 != frame)
    {
        HRESULT hr_created =
            IWICImagingFactory_CreateFormatConverter(w32_wic_factory, &converted_source_bitmap);
        if(SUCCEEDED(hr_created))
        {
            HRESULT hr_initialised =
                IWICFormatConverter_Initialize(converted_source_bitmap,
                                               (IWICBitmapSource *)frame,
                                               &GUID_WICPixelFormat32bppRGBA,
                                               WICBitmapDitherTypeNone,
                                               0, 0.0f,
                                               WICBitmapPaletteTypeCustom);
            if(!SUCCEEDED(hr_initialised))
            {
                OS_TCtxErrorPushFmt("Error decoding image '%.*s': could not initialise converted source bitmap", FmtS8(filename));
                converted_source_bitmap = 0;
            }
        }
        else
        {
            OS_TCtxErrorPushFmt("Error decoding image '%.*s': could not create converted source bitmap", FmtS8(filename));
            converted_source_bitmap = 0;
        }
    }
    
    IMG_Data result = {0};
    if(0 != converted_source_bitmap)
    {
        UINT width;
        UINT height;
        HRESULT hr_got_size =
            IWICFormatConverter_GetSize(converted_source_bitmap, &width, &height);
        if(SUCCEEDED(hr_got_size))
        {
            ArenaTemp checkpoint = ArenaTempBegin(arena);
            result.dimensions = V2I(width, height);
            result.pixels = PushArray(arena, Pixel, width*height);
            
            HRESULT hr_copied_pixels =
                IWICFormatConverter_CopyPixels(converted_source_bitmap,
                                               0, width*sizeof(Pixel),
                                               width*height*sizeof(Pixel),
                                               (BYTE *)result.pixels);
            
            if(!SUCCEEDED(hr_copied_pixels))
            {
                OS_TCtxErrorPushFmt("Error decoding image '%.*s': could not copy pixels", FmtS8(filename));
                result.dimensions = V2I(0, 0);
                result.pixels = 0;
                ArenaTempEnd(checkpoint);
            }
        }
    }
    
    ArenaTempEnd(scratch);
    
    return result;
}

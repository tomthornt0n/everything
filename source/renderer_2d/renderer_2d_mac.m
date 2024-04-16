
#import <Cocoa/Cocoa.h>
#import <Metal/Metal.h>
#import <QuartzCore/CAMetalLayer.h>
#import <QuartzCore/QuartzCore.h>
#import <Foundation/Foundation.h>
#include <ApplicationServices/ApplicationServices.h>

#include "renderer_2d/renderer_2d_mac_shared_types.h"

typedef struct MAC_Renderer2DTexture MAC_Renderer2DTexture;
struct MAC_Renderer2DTexture
{
    V2I dimensions;
    MAC_Renderer2DTexture *free_list_next;
    id<MTLTexture> texture;
};

typedef struct MAC_Renderer2DData MAC_Renderer2DData;
struct MAC_Renderer2DData
{
    id<MTLRenderPipelineState> pipeline_state;
    id<MTLBuffer> vertex_buffer;
    id<MTLBuffer> index_buffer;
    OpaqueHandle texture_allocator_lock;
    Arena *texture_arena;
    MAC_Renderer2DTexture *texture_free_list;
    MAC_Renderer2DTexture nil_texture;
};
Global MAC_Renderer2DData mac_r2d_data = {0};

//~NOTE(tbt): initialisation and cleanup

Function void
R2D_Init(void)
{
    mac_r2d_data.texture_allocator_lock = OS_SemaphoreMake(1);
    mac_r2d_data.texture_arena = ArenaMake();
    mac_r2d_data.texture_free_list = 0;
    mac_r2d_data.nil_texture.free_list_next = &mac_r2d_data.nil_texture;
    MTLTextureDescriptor *texture_desc = [[MTLTextureDescriptor alloc] init];
    texture_desc.pixelFormat = MTLPixelFormatBGRA8Unorm;
    texture_desc.width = 1;
    texture_desc.height = 1;
    mac_r2d_data.nil_texture.texture = [mac_g_app.metal_device newTextureWithDescriptor:texture_desc];
    [texture_desc release];
    MTLRegion region =
    {
        { 0, 0, 0, },
        { 1, 1, 1, },
    };
    Pixel data = { .val = 0xffffffff, };
    [mac_r2d_data.nil_texture.texture replaceRegion:region
                        mipmapLevel:0
                          withBytes:&data
                        bytesPerRow:4];

    Assert(sizeof(R2D_Quad) == sizeof(MAC_Renderer2DInstance));

    NSError *error = nil;
    NSURL *url = [NSURL URLWithString:@"renderer_2d_mac_shaders.metallib"];
    id<MTLLibrary> library = [mac_g_app.metal_device newLibraryWithURL:url error:&error];
    OS_TCtxErrorPushCond((0 != library), S8("Graphics error"), S8("Could not load shader library"));

    id<MTLFunction> vertex_function = [library newFunctionWithName:@"vs"];
    id<MTLFunction> fragment_function = [library newFunctionWithName:@"ps"];

    MTLRenderPipelineDescriptor *pipeline_desc = [[MTLRenderPipelineDescriptor alloc] init];
    pipeline_desc.label = @"renderer_2d";
    pipeline_desc.vertexFunction = vertex_function;
    pipeline_desc.fragmentFunction = fragment_function;
    pipeline_desc.colorAttachments[0].pixelFormat = MTLPixelFormatBGRA8Unorm;
    pipeline_desc.colorAttachments[0].blendingEnabled = YES;
    pipeline_desc.colorAttachments[0].rgbBlendOperation = MTLBlendOperationAdd;
    pipeline_desc.colorAttachments[0].alphaBlendOperation = MTLBlendOperationAdd;
    pipeline_desc.colorAttachments[0].sourceRGBBlendFactor = MTLBlendFactorOne;
    pipeline_desc.colorAttachments[0].sourceAlphaBlendFactor = MTLBlendFactorOne;
    pipeline_desc.colorAttachments[0].destinationRGBBlendFactor = MTLBlendFactorOneMinusSourceAlpha;
    pipeline_desc.colorAttachments[0].destinationAlphaBlendFactor = MTLBlendFactorOneMinusSourceAlpha;
    mac_r2d_data.pipeline_state = [mac_g_app.metal_device newRenderPipelineStateWithDescriptor:pipeline_desc
                                                                                         error:&error];
    [pipeline_desc release];
    OS_TCtxErrorPushCond((0 != mac_r2d_data.pipeline_state), S8("Graphics error"), S8("Could not create pipeline state"));

    MTLResourceOptions options = MTLResourceCPUCacheModeDefaultCache;

    Persist MAC_Renderer2DVertex vertices[] =
    {
        { { -1.0f, -1.0f, } },
        { { -1.0f, +1.0f, } },
        { { +1.0f, -1.0f, } },
        { { +1.0f, +1.0f, } },
    };
    mac_r2d_data.vertex_buffer = [mac_g_app.metal_device newBufferWithBytes:vertices  
                                                                     length:sizeof(vertices)
                                                                     options:options];
    Persist U16 indices[] =
    {
        0, 1, 2, 3, 2, 1,
    };
    mac_r2d_data.index_buffer = [mac_g_app.metal_device newBufferWithBytes:indices  
                                                                    length:sizeof(indices)
                                                                    options:options];
}

Function void
R2D_Cleanup(void)
{

}

//~NOTE(tbt): textures

Function MAC_Renderer2DTexture *
MAC_Renderer2DTextureAllocate(void)
{
    OS_SemaphoreWait(mac_r2d_data.texture_allocator_lock);
    MAC_Renderer2DTexture *result = mac_r2d_data.texture_free_list;
    if(0 == result || result == &mac_r2d_data.nil_texture)
    {
        result = ArenaPush(mac_r2d_data.texture_arena, sizeof(*result));
    }
    else
    {
        mac_r2d_data.texture_free_list = mac_r2d_data.texture_free_list->free_list_next;
    }
    result->free_list_next = &mac_r2d_data.nil_texture;
    OS_SemaphoreSignal(mac_r2d_data.texture_allocator_lock);
    
    return result;
}

Function void
MAC_Renderer2DTextureFree(MAC_Renderer2DTexture *texture)
{
    OS_SemaphoreWait(mac_r2d_data.texture_allocator_lock);
    texture->free_list_next = mac_r2d_data.texture_free_list;
    mac_r2d_data.texture_free_list = texture;
    OS_SemaphoreSignal(mac_r2d_data.texture_allocator_lock);
}

Function OpaqueHandle
R2D_TextureNil(void)
{
    OpaqueHandle result = { .p = IntFromPtr(&mac_r2d_data.nil_texture), };
    return result;
}

Function OpaqueHandle
R2D_TextureMake(Pixel *data, V2I dimensions)
{
    // NOTE(tbt): allocate my texture state
    MAC_Renderer2DTexture *texture = MAC_Renderer2DTextureAllocate();
    texture->dimensions = dimensions;

    // NOTE(tbt): make metal texture object
    MTLTextureDescriptor *texture_desc = [[MTLTextureDescriptor alloc] init];
    texture_desc.pixelFormat = MTLPixelFormatRGBA8Unorm;
    texture_desc.width = dimensions.x;
    texture_desc.height = dimensions.y;
    texture->texture = [mac_g_app.metal_device newTextureWithDescriptor:texture_desc];
    [texture_desc release];

    // NOTE(tbt): upload pixels
    MTLRegion region =
    {
        { 0, 0, 0, },
        { dimensions.x, dimensions.y, 1, },
    };
    [texture->texture replaceRegion:region
                        mipmapLevel:0
                          withBytes:data
                        bytesPerRow:dimensions.x*4];

    OpaqueHandle result = { .p = IntFromPtr(texture), };
    return result;
}

Function void
R2D_TextureDestroy(OpaqueHandle texture)
{
    if(!R2D_TextureIsNil(texture))
    {
        MAC_Renderer2DTexture *t = PtrFromInt(texture.p);
        [t->texture release];
        MemorySet(t, 0, sizeof(*t));
        MAC_Renderer2DTextureFree(t);
    }
}

Function void
R2D_TexturePixelsSet(OpaqueHandle texture, Pixel *data)
{
    MAC_Renderer2DTexture *t = PtrFromInt(texture.p);
    MTLRegion region =
    {
        { 0, 0, 0 },
        { t->dimensions.x, t->dimensions.y, 1},
    };
    [t->texture replaceRegion:region
                        mipmapLevel:0
                          withBytes:data
                        bytesPerRow:t->dimensions.x*4];
}

Function V2I
R2D_TextureDimensionsGet(OpaqueHandle texture)
{
    MAC_Renderer2DTexture *t = PtrFromInt(texture.p);
    V2I result = t->dimensions;
    return result;
}

//~NOTE(tbt): quad lists

Function void
R2D_QuadListSubmit(OpaqueHandle window, R2D_QuadList *list, OpaqueHandle texture, Range2F mask)
{
    MAC_Window *w = PtrFromInt(window.p);

    U64 quads_count = R2D_QuadListCountGet(list);

    CGSize drawable_size = [w->metal_view.metal_layer drawableSize];
    V2I viewport_size = V2I(drawable_size.width, drawable_size.height);

    [w->command_encoder setViewport:(MTLViewport){0.0, 0.0, viewport_size.x, viewport_size.y, 0.0, 1.0 }];
        
    [w->command_encoder setRenderPipelineState:mac_r2d_data.pipeline_state];

    [w->command_encoder setVertexBuffer:mac_r2d_data.vertex_buffer offset:0 atIndex:MAC_Renderer2DVertexInputIndex_Vertices];
    [w->command_encoder setVertexBytes:list->quads
                                length:sizeof(MAC_Renderer2DInstance)*quads_count
                               atIndex:MAC_Renderer2DVertexInputIndex_Instances];
    
    [w->command_encoder setVertexBytes:&viewport_size
                            length:sizeof(viewport_size)
                            atIndex:MAC_Renderer2DVertexInputIndex_ViewportSize];

    MAC_Renderer2DTexture *t = PtrFromInt(texture.p);
    [w->command_encoder setFragmentTexture:t->texture
                                   atIndex:MAC_Renderer2DFragmentTextureIndex_Colour];

    MTLScissorRect scissor_rect =
    {
        .x = mask.min.x,
        .y = mask.min.y,
        .width = mask.max.x - mask.min.x,
        .height = mask.max.y - mask.min.y,
    };
    [w->command_encoder setScissorRect:scissor_rect];

    [w->command_encoder drawIndexedPrimitives:MTLPrimitiveTypeTriangle
                                   indexCount:6
                                    indexType:MTLIndexTypeUInt16
                                  indexBuffer:mac_r2d_data.index_buffer
                            indexBufferOffset:0
                                instanceCount:quads_count];
}

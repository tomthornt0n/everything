
typedef struct W32_Texture W32_Texture;
struct W32_Texture
{
    V2I dimensions;
    W32_Texture *free_list_next;
    ID3D11Texture2D *texture;
    ID3D11ShaderResourceView *texture_view;
};

typedef struct W32_Renderer2DUniformBuffer W32_Renderer2DUniformBuffer;
struct W32_Renderer2DUniformBuffer
{
    V2F u_resolution;
};

typedef struct W32_Renderer2DData W32_Renderer2DData;
struct W32_Renderer2DData
{
    ID3D11Buffer *instance_buffer;
    
    // NOTE(tbt): shader
    ID3D11VertexShader *vertex_shader;
    ID3D11PixelShader *pixel_shader;
    ID3D11InputLayout *input_layout;
    
    // NOTE(tbt): uniforms
    W32_Renderer2DUniformBuffer uniforms;
    ID3D11Buffer *uniform_buffer;
    
    // NOTE(tbt): pipeline state
    ID3D11SamplerState *sampler;
    ID3D11BlendState *blend_state;
    ID3D11RasterizerState *rasteriser_state;
    ID3D11DepthStencilState *depth_stencil_state;
    
    // NOTE(tbt): textures
    W32_Texture nil_texture;
};

Global W32_Renderer2DData w32_r2d_data = {0};

ThreadLocalVar Global W32_Texture *w32_texture_free_list = 0;

Function void
R2D_Init(void)
{
    // NOTE(tbt): nil texture
    w32_r2d_data.nil_texture.dimensions = V2I(1, 1);
    w32_r2d_data.nil_texture.free_list_next = &w32_r2d_data.nil_texture;
    Pixel nil_texture_data = { 0xffffffff };
    D3D11_TEXTURE2D_DESC nil_texture_texture_desc =
    {
        .Width = w32_r2d_data.nil_texture.dimensions.x,
        .Height = w32_r2d_data.nil_texture.dimensions.y,
        .MipLevels = 1,
        .ArraySize = 1,
        .Format = DXGI_FORMAT_R8G8B8A8_UNORM,
        .SampleDesc = { 1, 0 },
        .Usage = D3D11_USAGE_IMMUTABLE,
        .BindFlags = D3D11_BIND_SHADER_RESOURCE,
    };
    D3D11_SUBRESOURCE_DATA nil_texture_subresource_data =
    {
        .pSysMem = (U32 *)&nil_texture_data,
        .SysMemPitch = sizeof(U32),
    };
    ID3D11Device_CreateTexture2D(w32_g_app.device,
                                 &nil_texture_texture_desc,
                                 &nil_texture_subresource_data,
                                 &w32_r2d_data.nil_texture.texture);
    ID3D11Device_CreateShaderResourceView(w32_g_app.device,
                                          (ID3D11Resource*)w32_r2d_data.nil_texture.texture, 0,
                                          &w32_r2d_data.nil_texture.texture_view);
    ID3D11Texture2D_Release(w32_r2d_data.nil_texture.texture);
    
    // NOTE(tbt): default shader
#if BuildModeDebug
# include "..\build\w32_vshader_debug.h"
# include "..\build\w32_pshader_debug.h"
#else
# include "..\build\w32_vshader.h"
# include "..\build\w32_pshader.h"
#endif
    D3D11_INPUT_ELEMENT_DESC input_layout_desc[] =
    {
        { "dst_min",       0, DXGI_FORMAT_R32G32_FLOAT,       0, OffsetOf(R2D_Quad, dst.min),       D3D11_INPUT_PER_INSTANCE_DATA, 1, },
        { "dst_max",       0, DXGI_FORMAT_R32G32_FLOAT,       0, OffsetOf(R2D_Quad, dst.max),       D3D11_INPUT_PER_INSTANCE_DATA, 1, },
        { "src_min",       0, DXGI_FORMAT_R32G32_FLOAT,       0, OffsetOf(R2D_Quad, src.min),       D3D11_INPUT_PER_INSTANCE_DATA, 1, },
        { "src_max",       0, DXGI_FORMAT_R32G32_FLOAT,       0, OffsetOf(R2D_Quad, src.max),       D3D11_INPUT_PER_INSTANCE_DATA, 1, },
        { "mask_min",      0, DXGI_FORMAT_R32G32_FLOAT,       0, OffsetOf(R2D_Quad, mask.min),      D3D11_INPUT_PER_INSTANCE_DATA, 1, },
        { "mask_max",      0, DXGI_FORMAT_R32G32_FLOAT,       0, OffsetOf(R2D_Quad, mask.max),      D3D11_INPUT_PER_INSTANCE_DATA, 1, },
        { "fill_colour",   0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, OffsetOf(R2D_Quad, fill_colour),   D3D11_INPUT_PER_INSTANCE_DATA, 1, },
        { "stroke_colour", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, OffsetOf(R2D_Quad, stroke_colour), D3D11_INPUT_PER_INSTANCE_DATA, 1, },
        { "stroke_width",  0, DXGI_FORMAT_R32_FLOAT,          0, OffsetOf(R2D_Quad, stroke_width),  D3D11_INPUT_PER_INSTANCE_DATA, 1, },
        { "corner_radius", 0, DXGI_FORMAT_R32_FLOAT,          0, OffsetOf(R2D_Quad, corner_radius), D3D11_INPUT_PER_INSTANCE_DATA, 1, },
        { "edge_softness", 0, DXGI_FORMAT_R32_FLOAT,          0, OffsetOf(R2D_Quad, edge_softness), D3D11_INPUT_PER_INSTANCE_DATA, 1, },
    };
    ID3D11Device_CreateVertexShader(w32_g_app.device,
                                    w32_vshader, sizeof(w32_vshader),
                                    0, &w32_r2d_data.vertex_shader);
    ID3D11Device_CreatePixelShader(w32_g_app.device,
                                   w32_pshader, sizeof(w32_pshader),
                                   0, &w32_r2d_data.pixel_shader);
    ID3D11Device_CreateInputLayout(w32_g_app.device,
                                   input_layout_desc,
                                   ArrayCount(input_layout_desc),
                                   w32_vshader, sizeof(w32_vshader),
                                   &w32_r2d_data.input_layout);
    
    // NOTE(tbt): uniform buffer
    D3D11_BUFFER_DESC uniform_buffer_desc =
    {
        .ByteWidth = Max(sizeof(W32_Renderer2DUniformBuffer), 16),
        .Usage = D3D11_USAGE_DYNAMIC,
        .BindFlags = D3D11_BIND_CONSTANT_BUFFER,
        .CPUAccessFlags = D3D11_CPU_ACCESS_WRITE,
    };
    ID3D11Device_CreateBuffer(w32_g_app.device,
                              &uniform_buffer_desc,
                              0, &w32_r2d_data.uniform_buffer);
    
    // NOTE(tbt): instance buffer
    D3D11_BUFFER_DESC instance_buffer_desc =
    {
        .ByteWidth = sizeof(R2D_Quad)*R2D_QuadListCap,
        .Usage = D3D11_USAGE_DYNAMIC,
        .BindFlags = D3D11_BIND_VERTEX_BUFFER,
        .CPUAccessFlags = D3D11_CPU_ACCESS_WRITE,
    };
    ID3D11Device_CreateBuffer(w32_g_app.device,
                              &instance_buffer_desc,
                              0, &w32_r2d_data.instance_buffer);
    
    // NOTE(tbt): texture sampler
    D3D11_SAMPLER_DESC sampler_desc =
    {
        .Filter = D3D11_FILTER_MIN_MAG_MIP_POINT,
        .AddressU = D3D11_TEXTURE_ADDRESS_WRAP,
        .AddressV = D3D11_TEXTURE_ADDRESS_WRAP,
        .AddressW = D3D11_TEXTURE_ADDRESS_WRAP,
    };
    ID3D11Device_CreateSamplerState(w32_g_app.device,
                                    &sampler_desc,
                                    &w32_r2d_data.sampler);
    
    // NOTE(tbt): enable alpha blending
    D3D11_BLEND_DESC blend_state_desc =
    {
        .RenderTarget[0] =
        {
            .BlendEnable = TRUE,
            .SrcBlend = D3D11_BLEND_ONE,
            .DestBlend = D3D11_BLEND_INV_SRC_ALPHA,
            .BlendOp = D3D11_BLEND_OP_ADD,
            .SrcBlendAlpha = D3D11_BLEND_ONE,
            .DestBlendAlpha = D3D11_BLEND_INV_SRC_ALPHA,
            .BlendOpAlpha = D3D11_BLEND_OP_ADD,
            .RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL,
        },
    };
    ID3D11Device_CreateBlendState(w32_g_app.device,
                                  &blend_state_desc,
                                  &w32_r2d_data.blend_state);
    
    // NOTE(tbt): disable culling
    D3D11_RASTERIZER_DESC rasteriser_state_desc =
    {
        .FillMode = D3D11_FILL_SOLID,
        .CullMode = D3D11_CULL_NONE,
        .AntialiasedLineEnable = TRUE,
    };
    ID3D11Device_CreateRasterizerState(w32_g_app.device,
                                       &rasteriser_state_desc,
                                       &w32_r2d_data.rasteriser_state);
    
    // NOTE(tbt): depth and stencil test
    D3D11_DEPTH_STENCIL_DESC depth_stencil_state_desc =
    {
        .DepthEnable = FALSE,
        .DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ALL,
        .DepthFunc = D3D11_COMPARISON_LESS,
        .StencilEnable = FALSE,
        .StencilReadMask = D3D11_DEFAULT_STENCIL_READ_MASK,
        .StencilWriteMask = D3D11_DEFAULT_STENCIL_WRITE_MASK,
    };
    ID3D11Device_CreateDepthStencilState(w32_g_app.device,
                                         &depth_stencil_state_desc,
                                         &w32_r2d_data.depth_stencil_state);
}

Function void
R2D_Cleanup(void)
{
    // NOTE(tbt): no point cleaning up when going to quit anyway
}

Function W32_Texture *
W32_TextureFromHandle(OpaqueHandle texture)
{
    W32_Texture *result = PtrFromInt(texture.p);
    return result;
}

Function OpaqueHandle
W32_HandleFromTexture(W32_Texture *texture)
{
    OpaqueHandle result = {0};
    result.p = IntFromPtr(texture);
    result.g = OS_TCtxGet()->logical_thread_index;
    return result;
}

Function W32_Texture *
W32_TextureAlloc(void)
{
    W32_Texture *result = w32_texture_free_list;
    if(0 == result || result == &w32_r2d_data.nil_texture)
    {
        result = PushArray(OS_TCtxGet()->permanent_arena, W32_Texture, 1);
    }
    else
    {
        w32_texture_free_list = w32_texture_free_list->free_list_next;
    }
    result->free_list_next = &w32_r2d_data.nil_texture;
    
    return result;
}

Function void
W32_TextureRelease(W32_Texture *texture)
{
    texture->free_list_next = w32_texture_free_list;
    w32_texture_free_list = texture;
}

Function OpaqueHandle
R2D_TextureNil(void)
{
    OpaqueHandle result = W32_HandleFromTexture(&w32_r2d_data.nil_texture);
    return result;
}

Function OpaqueHandle
R2D_TextureAlloc(Pixel *data, V2I dimensions)
{
    W32_Texture *texture = W32_TextureAlloc();
    
    texture->dimensions = dimensions;
    
    D3D11_TEXTURE2D_DESC desc =
    {
        .Width = dimensions.x,
        .Height = dimensions.y,
        .MipLevels = 1,
        .ArraySize = 1,
        .Format = DXGI_FORMAT_R8G8B8A8_UNORM,
        .SampleDesc = { 1, 0, },
        .Usage = D3D11_USAGE_DYNAMIC,
        .BindFlags = D3D11_BIND_SHADER_RESOURCE,
        .CPUAccessFlags = D3D11_CPU_ACCESS_WRITE,
    };
    D3D11_SUBRESOURCE_DATA subresource_data =
    {
        .pSysMem = (U32 *)data,
        .SysMemPitch = dimensions.x*sizeof(U32),
    };
    
    ID3D11Device_CreateTexture2D(w32_g_app.device,
                                 &desc, &subresource_data,
                                 &texture->texture);
    ID3D11Device_CreateShaderResourceView(w32_g_app.device,
                                          (ID3D11Resource*)texture->texture, 0,
                                          &texture->texture_view);
    //ID3D11Texture2D_Release(texture->texture);
    
    Assert(0 != texture->texture_view);
    
    OpaqueHandle result = W32_HandleFromTexture(texture);
    return result;
}

Function void
R2D_TextureRelease(OpaqueHandle texture)
{
    if(texture.g != OS_TCtxGet()->logical_thread_index)
    {
        OS_TCtxErrorPush(S8("Textures must be freed on the same thread they were allocated"));
    }
    else if(!R2D_TextureIsNil(texture))
    {
        W32_Texture *t = W32_TextureFromHandle(texture);
        ID3D11ShaderResourceView_Release(t->texture_view);
        ID3D11Texture2D_Release(t->texture);
        MemorySet(t, 0, sizeof(*t));
        W32_TextureRelease(t);
    }
}

Function V2I
R2D_TextureDimensionsGet(OpaqueHandle texture)
{
    W32_Texture *t = W32_TextureFromHandle(texture);
    V2I result = t->dimensions;
    return result;
}

Function void
R2D_TexturePixelsSet(OpaqueHandle texture, Pixel *data)
{
    if(!R2D_TextureIsNil(texture))
    {
        W32_Texture *t = W32_TextureFromHandle(texture);
        D3D11_MAPPED_SUBRESOURCE texture_mapped;
        ID3D11DeviceContext_Map(w32_g_app.device_ctx, (ID3D11Resource *)t->texture, 0, D3D11_MAP_WRITE_DISCARD, 0, &texture_mapped);
        
        U8 *src = (U8 *)data;
        U8 *dst = (U8 *)texture_mapped.pData;
        U64 row_size = t->dimensions.x*sizeof(Pixel);
        for(I32 y = 0; y < t->dimensions.y; y += 1)
        {
            MemoryCopy(dst, src, row_size);
            src += row_size;
            dst += texture_mapped.RowPitch;
        }
        
        ID3D11DeviceContext_Unmap(w32_g_app.device_ctx, (ID3D11Resource *)t->texture, 0);
    }
}

Function void
R2D_QuadListSubmit(OpaqueHandle window, R2D_QuadList list, OpaqueHandle texture)
{
    W32_Window *w = W32_WindowFromHandle(window);
    W32_Texture *t = W32_TextureFromHandle(texture);
    U64 instances_count = list.count;
    V2I window_dimensions = G_WindowDimensionsGet(window);
    
    //-NOTE(tbt): upload uniforms
    w32_r2d_data.uniforms.u_resolution = V2F(window_dimensions.x, window_dimensions.y);
    D3D11_MAPPED_SUBRESOURCE uniform_buffer_mapped;
    ID3D11DeviceContext_Map(w32_g_app.device_ctx, (ID3D11Resource*)w32_r2d_data.uniform_buffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &uniform_buffer_mapped);
    MemoryCopy(uniform_buffer_mapped.pData, &w32_r2d_data.uniforms, sizeof(w32_r2d_data.uniforms));
    ID3D11DeviceContext_Unmap(w32_g_app.device_ctx, (ID3D11Resource*)w32_r2d_data.uniform_buffer, 0);
    
    //-NOTE(tbt): upload instances
    D3D11_MAPPED_SUBRESOURCE instance_buffer_mapped;
    ID3D11DeviceContext_Map(w32_g_app.device_ctx, (ID3D11Resource*)w32_r2d_data.instance_buffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &instance_buffer_mapped);
    MemoryCopy(instance_buffer_mapped.pData, list.quads, instances_count*sizeof(R2D_Quad));
    ID3D11DeviceContext_Unmap(w32_g_app.device_ctx, (ID3D11Resource*)w32_r2d_data.instance_buffer, 0);
    
    //-NOTE(tbt): input assembler
    ID3D11DeviceContext_IASetInputLayout(w32_g_app.device_ctx, w32_r2d_data.input_layout);
    ID3D11DeviceContext_IASetPrimitiveTopology(w32_g_app.device_ctx, D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);
    ID3D11DeviceContext_IASetVertexBuffers(w32_g_app.device_ctx, 0, 1, &w32_r2d_data.instance_buffer, (UINT[1]){ sizeof(R2D_Quad) }, (UINT[1]){ 0 });
    
    //-NOTE(tbt): vertex shader
    ID3D11DeviceContext_VSSetConstantBuffers(w32_g_app.device_ctx, 0, 1, &w32_r2d_data.uniform_buffer);
    ID3D11DeviceContext_VSSetShader(w32_g_app.device_ctx, w32_r2d_data.vertex_shader, 0, 0);
    
    //-NOTE(tbt): rasteriser
    ID3D11DeviceContext_RSSetState(w32_g_app.device_ctx, w32_r2d_data.rasteriser_state);
    D3D11_VIEWPORT viewport =
    {
        .TopLeftX = 0,
        .TopLeftY = 0,
        .Width = window_dimensions.x,
        .Height = window_dimensions.y,
        .MinDepth = 0,
        .MaxDepth = 1,
    };
    ID3D11DeviceContext_RSSetViewports(w32_g_app.device_ctx, 1, &viewport);
    
    //-NOTE(tbt): pixel shader
    ID3D11DeviceContext_PSSetSamplers(w32_g_app.device_ctx, 0, 1, &w32_r2d_data.sampler);
    ID3D11DeviceContext_PSSetShaderResources(w32_g_app.device_ctx, 0, 1, &t->texture_view);
    ID3D11DeviceContext_PSSetShader(w32_g_app.device_ctx, w32_r2d_data.pixel_shader, 0, 0);
    
    //-NOTE(tbt): output merger
    ID3D11DeviceContext_OMSetBlendState(w32_g_app.device_ctx, w32_r2d_data.blend_state, 0, ~0U);
    ID3D11DeviceContext_OMSetDepthStencilState(w32_g_app.device_ctx, w32_r2d_data.depth_stencil_state, 0);
    ID3D11DeviceContext_OMSetRenderTargets(w32_g_app.device_ctx, 1, &w->render_target_view, w->depth_stencil_view);
    
    //-NOTE(tbt): draw
    ID3D11DeviceContext_DrawInstanced(w32_g_app.device_ctx, 4, instances_count, 0, 0);
}

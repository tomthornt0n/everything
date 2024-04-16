
typedef struct W32_RendererLineUniformBuffer W32_RendererLineUniformBuffer;
struct W32_RendererLineUniformBuffer
{
    V2F u_resolution;
};

typedef struct W32_RendererLineData W32_RendererLineData;
struct W32_RendererLineData
{
    ID3D11Buffer *instance_buffer;
    
    // NOTE(tbt): shader
    ID3D11VertexShader *vertex_shader;
    ID3D11PixelShader *pixel_shader;
    ID3D11InputLayout *input_layout;
    
    // NOTE(tbt): uniforms
    W32_RendererLineUniformBuffer uniforms;
    ID3D11Buffer *uniform_buffer;
    
    // NOTE(tbt): pipeline state
    ID3D11BlendState *blend_state;
    ID3D11RasterizerState *rasteriser_state;
    ID3D11DepthStencilState *depth_stencil_state;
};

Global W32_RendererLineData w32_r_line_data = {0};

Function void
LINE_Init(void)
{
    // NOTE(tbt): default shader
#if BuildModeDebug
# include "..\build\w32_vshader_line_debug.h"
# include "..\build\w32_pshader_line_debug.h"
#else
# include "..\build\w32_vshader_line.h"
# include "..\build\w32_pshader_line.h"
#endif
    D3D11_INPUT_ELEMENT_DESC input_layout_desc[] =
    {
        { "a_position", 0, DXGI_FORMAT_R32G32_FLOAT,       0, OffsetOf(LINE_Segment, a.pos),   D3D11_INPUT_PER_INSTANCE_DATA, 1, },
        { "a_colour",   0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, OffsetOf(LINE_Segment, a.col),   D3D11_INPUT_PER_INSTANCE_DATA, 1, },
        { "a_width",    0, DXGI_FORMAT_R32_FLOAT,          0, OffsetOf(LINE_Segment, a.width), D3D11_INPUT_PER_INSTANCE_DATA, 1, },
        { "b_position", 0, DXGI_FORMAT_R32G32_FLOAT,       0, OffsetOf(LINE_Segment, b.pos),   D3D11_INPUT_PER_INSTANCE_DATA, 1, },
        { "b_colour",   0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, OffsetOf(LINE_Segment, b.col),   D3D11_INPUT_PER_INSTANCE_DATA, 1, },
        { "b_width",    0, DXGI_FORMAT_R32_FLOAT,          0, OffsetOf(LINE_Segment, b.width), D3D11_INPUT_PER_INSTANCE_DATA, 1, },
    };
    ID3D11Device_CreateVertexShader(w32_g_app.device,
                                    w32_vshader_line, sizeof(w32_vshader_line),
                                    0, &w32_r_line_data.vertex_shader);
    ID3D11Device_CreatePixelShader(w32_g_app.device,
                                   w32_pshader_line, sizeof(w32_pshader_line),
                                   0, &w32_r_line_data.pixel_shader);
    ID3D11Device_CreateInputLayout(w32_g_app.device,
                                   input_layout_desc,
                                   ArrayCount(input_layout_desc),
                                   w32_vshader_line, sizeof(w32_vshader_line),
                                   &w32_r_line_data.input_layout);
    
    // NOTE(tbt): uniform buffer
    D3D11_BUFFER_DESC uniform_buffer_desc =
    {
        .ByteWidth = Max(sizeof(W32_RendererLineUniformBuffer), 16),
        .Usage = D3D11_USAGE_DYNAMIC,
        .BindFlags = D3D11_BIND_CONSTANT_BUFFER,
        .CPUAccessFlags = D3D11_CPU_ACCESS_WRITE,
    };
    ID3D11Device_CreateBuffer(w32_g_app.device,
                              &uniform_buffer_desc,
                              0, &w32_r_line_data.uniform_buffer);
    
    // NOTE(tbt): instance buffer
    D3D11_BUFFER_DESC instance_buffer_desc =
    {
        .ByteWidth = sizeof(LINE_Segment)*LINE_SegmentListCap,
        .Usage = D3D11_USAGE_DYNAMIC,
        .BindFlags = D3D11_BIND_VERTEX_BUFFER,
        .CPUAccessFlags = D3D11_CPU_ACCESS_WRITE,
    };
    ID3D11Device_CreateBuffer(w32_g_app.device,
                              &instance_buffer_desc,
                              0, &w32_r_line_data.instance_buffer);
    
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
                                  &w32_r_line_data.blend_state);
    
    // NOTE(tbt): disable culling
    D3D11_RASTERIZER_DESC rasteriser_state_desc =
    {
        .FillMode = D3D11_FILL_SOLID,
        .CullMode = D3D11_CULL_NONE,
        .ScissorEnable = TRUE,
        .MultisampleEnable = TRUE,
        .AntialiasedLineEnable = TRUE,
    };
    ID3D11Device_CreateRasterizerState(w32_g_app.device,
                                       &rasteriser_state_desc,
                                       &w32_r_line_data.rasteriser_state);
    
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
                                         &w32_r_line_data.depth_stencil_state);
}

Function void
LINE_Cleanup(void)
{
    // NOTE(tbt): everything will get cleaned up by the os when we exit anyway
}

Function void
LINE_SegmentListSubmit(OpaqueHandle window, LINE_SegmentList list, Range2F mask)
{
    W32_Window *w = W32_WindowFromHandle(window);
    U64 instances_count = list.count;
    V2I window_dimensions = G_WindowDimensionsGet(window);
    
    //-NOTE(tbt): upload uniforms
    w32_r_line_data.uniforms.u_resolution = V2F(window_dimensions.x, window_dimensions.y);
    D3D11_MAPPED_SUBRESOURCE uniform_buffer_mapped;
    ID3D11DeviceContext_Map(w32_g_app.device_ctx, (ID3D11Resource*)w32_r_line_data.uniform_buffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &uniform_buffer_mapped);
    MemoryCopy(uniform_buffer_mapped.pData, &w32_r_line_data.uniforms, sizeof(w32_r_line_data.uniforms));
    ID3D11DeviceContext_Unmap(w32_g_app.device_ctx, (ID3D11Resource*)w32_r_line_data.uniform_buffer, 0);
    
    //-NOTE(tbt): upload instances
    D3D11_MAPPED_SUBRESOURCE instance_buffer_mapped;
    ID3D11DeviceContext_Map(w32_g_app.device_ctx, (ID3D11Resource*)w32_r_line_data.instance_buffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &instance_buffer_mapped);
    MemoryCopy(instance_buffer_mapped.pData, list.segments, instances_count*sizeof(LINE_Segment));
    ID3D11DeviceContext_Unmap(w32_g_app.device_ctx, (ID3D11Resource*)w32_r_line_data.instance_buffer, 0);
    
    //-NOTE(tbt): input assembler
    ID3D11DeviceContext_IASetInputLayout(w32_g_app.device_ctx, w32_r_line_data.input_layout);
    ID3D11DeviceContext_IASetPrimitiveTopology(w32_g_app.device_ctx, D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);
    ID3D11DeviceContext_IASetVertexBuffers(w32_g_app.device_ctx, 0, 1, &w32_r_line_data.instance_buffer, (UINT[1]){ sizeof(LINE_Segment) }, (UINT[1]){0});
    
    //-NOTE(tbt): vertex shader
    ID3D11DeviceContext_VSSetConstantBuffers(w32_g_app.device_ctx, 0, 1, &w32_r_line_data.uniform_buffer);
    ID3D11DeviceContext_VSSetShader(w32_g_app.device_ctx, w32_r_line_data.vertex_shader, 0, 0);
    
    //-NOTE(tbt): rasteriser
    ID3D11DeviceContext_RSSetState(w32_g_app.device_ctx, w32_r_line_data.rasteriser_state);
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
    RECT scissor_rect =
    {
        .left = mask.min.x,
        .top = mask.min.y,
        .right = mask.max.x,
        .bottom = mask.max.y,
    };
    ID3D11DeviceContext_RSSetScissorRects(w32_g_app.device_ctx, 1, &scissor_rect);
    
    //-NOTE(tbt): pixel shader
    ID3D11DeviceContext_PSSetShader(w32_g_app.device_ctx, w32_r_line_data.pixel_shader, 0, 0);
    
    //-NOTE(tbt): output merger
    ID3D11DeviceContext_OMSetBlendState(w32_g_app.device_ctx, w32_r_line_data.blend_state, 0, ~0U);
    ID3D11DeviceContext_OMSetDepthStencilState(w32_g_app.device_ctx, w32_r_line_data.depth_stencil_state, 0);
    ID3D11DeviceContext_OMSetRenderTargets(w32_g_app.device_ctx, 1, &w->render_target_view, w->depth_stencil_view);
    
    //-NOTE(tbt): draw
    ID3D11DeviceContext_DrawInstanced(w32_g_app.device_ctx, 4, instances_count, 0, 0);
}

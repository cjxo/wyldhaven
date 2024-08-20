#include "ext/freetype/ft2build.h"
#include FT_FREETYPE_H

#define STB_IMAGE_IMPLEMENTATION
#include "ext/stb_image.h"

typedef HRESULT (*DXGIGetDebugInterface_Func)(REFIID,void**);

// TODO(christian): include samplers (maybe?)
typedef struct {
  s32 width, height;
  ID3D11Texture2D *texture;
  ID3D11ShaderResourceView *srv;
} R_D3D11_Texture2D;

typedef struct {
  OS_Window *associated_window;
  
  // NOTE(christian): d3d11
  IDXGISwapChain1 *dxgi_swap_chain;
  ID3D11Device *base_device;
  ID3D11Device1 *main_device;
  ID3D11DeviceContext *base_device_context;
  ID3D11Texture2D *back_buffer;
  ID3D11RenderTargetView *render_target_view;
  ID3D11RasterizerState1 *rasterizer_state;
  ID3D11BlendState1 *blend_state;
  ID3D11SamplerState *texture_sampler;
  D3D11_VIEWPORT viewport;
  
  // NOTE(christian): Common CBuffers
  ID3D11Buffer *main_constant_buffer;
  
  // NOTE(christian): Ortho2D
  ID3D11VertexShader *ortho2d_vertex_shader;
  ID3D11PixelShader *ortho2d_pixel_shader;
  ID3D11Buffer *ortho2d_quad_buffer;
  ID3D11ShaderResourceView *ortho2d_quad_buffer_srv;
  ID3D11Buffer *ortho2d_light_constant_buffer;
  
  // NOTE(christian): Ortho3D
  ID3D11VertexShader *ortho3d_vertex_shader;
  ID3D11PixelShader *ortho3d_pixel_shader;
  ID3D11Buffer *ortho3d_instance_buffer;
  ID3D11Buffer *ortho3d_light_constant_buffer;
  
	// NOTE(christian): UI renderer
  ID3D11VertexShader *ui_vertex_shader;
  ID3D11PixelShader *ui_pixel_shader;
  ID3D11Buffer *ui_quad_buffer;
  ID3D11ShaderResourceView *ui_quad_buffer_srv;
  
  // NOTE(christian): FONT stuff
  //R_D3D11_Texture2D font_tex;
} R_D3D11_State;

__declspec(align(16)) typedef struct {
  m44 proj;
} R_D3D11_Constants_Main;

typedef u16 R_D3D11_BufferType;
enum {
  R_D3D11_BufferType_Structured,
  R_D3D11_BufferType_Constant,
  R_D3D11_BufferType_Count,
};

fun HRESULT
r_d3d11_create_buffer(ID3D11Device1 *device, R_D3D11_BufferType buf_type, u64 struct_count, u64 struct_size,
                      ID3D11Buffer **result_buffer, ID3D11ShaderResourceView **result_srv) {
  HRESULT hr;
  D3D11_BUFFER_DESC buf_desc;
  
  buf_desc.ByteWidth = (UINT)(struct_count * struct_size);
  buf_desc.Usage = D3D11_USAGE_DYNAMIC;
  buf_desc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
  
  switch (buf_type) {
    case R_D3D11_BufferType_Structured: {
      buf_desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
      buf_desc.MiscFlags = D3D11_RESOURCE_MISC_BUFFER_STRUCTURED;
      buf_desc.StructureByteStride = (UINT)struct_size;
    } break;
    
    case R_D3D11_BufferType_Constant: {
      buf_desc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
      buf_desc.MiscFlags = 0;
      buf_desc.StructureByteStride = 0;
    } break;
  }
  
  hr = ID3D11Device1_CreateBuffer(device, &buf_desc, null, result_buffer);
  
  if (result_srv && (buf_type == R_D3D11_BufferType_Constant)) {
    assert_msgbox(0, "r_d3d11_create_buffer error.", "Buffer Type is Constant and being provided with a non-null SRV.");
  }
  
  if ((hr == S_OK) && (result_srv) && (buf_type == R_D3D11_BufferType_Structured)) {
    D3D11_SHADER_RESOURCE_VIEW_DESC srv_desc = {0};
    srv_desc.Format = DXGI_FORMAT_UNKNOWN;
    srv_desc.ViewDimension = D3D11_SRV_DIMENSION_BUFFER;
    srv_desc.Buffer.NumElements = (UINT)struct_count;
    
    hr = ID3D11Device1_CreateShaderResourceView(device, (ID3D11Resource *)(*result_buffer), &srv_desc, result_srv);
  }
  
  return(hr);
}

// TODO(christian): custom error codes / messages
fun HRESULT
r_d3d11_acquire_shader_handles(ID3D11Device1 *device, char *hlsl, u64 hlsl_size_in_bytes,
                               ID3D11VertexShader **vshader, ID3D11PixelShader **pshader,
                               char *vshader_main_func_name, char *pshader_main_func_name) {
  
  UINT hlsl_compile = D3DCOMPILE_PACK_MATRIX_COLUMN_MAJOR | D3DCOMPILE_ENABLE_STRICTNESS;
#if defined(WC_DEBUG)
  hlsl_compile |= D3DCOMPILE_SKIP_OPTIMIZATION | D3DCOMPILE_WARNINGS_ARE_ERRORS;
#else
  hlsl_compile |= D3DCOMPILE_OPTIMIZATION_LEVEL3;
#endif
  
  OutputDebugStringA(hlsl);
  ID3DBlob *bytecode_blob;
  ID3DBlob *error_blob;
  
  HRESULT result = D3DCompile(hlsl, hlsl_size_in_bytes, null, null, D3D_COMPILE_STANDARD_FILE_INCLUDE,
                              vshader_main_func_name, "vs_5_0", hlsl_compile, 0, &bytecode_blob, &error_blob);
  
  if (result == S_OK) {
    result = ID3D11Device1_CreateVertexShader(device, ID3D10Blob_GetBufferPointer(bytecode_blob),
                                              ID3D10Blob_GetBufferSize(bytecode_blob), null, vshader);
    
    ID3D10Blob_Release(bytecode_blob);
    if (result == S_OK) {
      result = D3DCompile(hlsl, hlsl_size_in_bytes, null, null, D3D_COMPILE_STANDARD_FILE_INCLUDE,
                          pshader_main_func_name, "ps_5_0", hlsl_compile, 0, &bytecode_blob, &error_blob);
      
      if (result == S_OK) {
        result = ID3D11Device1_CreatePixelShader(device, ID3D10Blob_GetBufferPointer(bytecode_blob),
                                                 ID3D10Blob_GetBufferSize(bytecode_blob), null, pshader);
        ID3D10Blob_Release(bytecode_blob);
      } else {
        OutputDebugStringA(ID3D10Blob_GetBufferPointer(error_blob));
      }
    }
  } else {
    OutputDebugStringA(ID3D10Blob_GetBufferPointer(error_blob));
  }
  
  return(result);
}

// TODO(christian): detailed error messages. Perhaps we can query HRESULT errors messages!
fun R_Buffer *
r_buffer_create(OS_Window *window) {
  R_Buffer *result = null;
  Memory_Arena *arena = arena_reserve(mb(16));
  result = arena_push_struct(arena, R_Buffer);
  result->arena = arena;
  result->reserved = arena_push_struct(arena, R_D3D11_State);
  result->free_texture_flag = 255;
  
  R_D3D11_State *d3d11_state = (R_D3D11_State *)result->reserved;
  d3d11_state->associated_window = window;
  HRESULT hr = S_OK;
  
  {
    D3D_FEATURE_LEVEL feature_level = D3D_FEATURE_LEVEL_11_0;
    
    UINT flags = D3D11_CREATE_DEVICE_BGRA_SUPPORT;
#if defined(WC_DEBUG)
    flags |= D3D11_CREATE_DEVICE_DEBUG;
#endif
    hr = D3D11CreateDevice(null, D3D_DRIVER_TYPE_HARDWARE, null, flags,
                           &feature_level, 1, D3D11_SDK_VERSION, &d3d11_state->base_device,
                           null, &d3d11_state->base_device_context);
    
    assert_msgbox(hr == S_OK, "D3D11CreateDevice Error", "Failed To Create ID3D11Device");
    
    hr = ID3D11Device_QueryInterface(d3d11_state->base_device, &IID_ID3D11Device1, &d3d11_state->main_device);
    assert_msgbox(hr == S_OK, "ID3D11Device_QueryInterface Error", "Failed To Query ID3D11Device1");
  }
  
#if defined(WC_DEBUG)
  {
    ID3D11InfoQueue *info_queue = null;
    hr = ID3D11Device_QueryInterface(d3d11_state->base_device, &IID_ID3D11InfoQueue, &info_queue);
    assert_msgbox(hr == S_OK, "ID3D11Device_QueryInterface Error",
                  "Failed To Create ID3D11InfoQueue");
    
    ID3D11InfoQueue_SetBreakOnSeverity(info_queue, D3D11_MESSAGE_SEVERITY_CORRUPTION, TRUE);
    ID3D11InfoQueue_SetBreakOnSeverity(info_queue, D3D11_MESSAGE_SEVERITY_ERROR, TRUE);
    
    ID3D11InfoQueue_Release(info_queue);
    
    IDXGIInfoQueue *dxgi_info_queue = null;
    HMODULE debug_module = LoadLibraryA("Dxgidebug.dll");
    assert_msgbox(debug_module != null, "LoadLibraryA Error", "Failed to load Dxgidebug.dll");
    
    DXGIGetDebugInterface_Func DXGIGetDebugInterface = (DXGIGetDebugInterface_Func)GetProcAddress(debug_module,
                                                                                                  "DXGIGetDebugInterface");
    
    assert_msgbox(DXGIGetDebugInterface != null,
                  "GetProcAddress Error",
                  "Failed to load DXGIGetDebugInterface");
    
    hr = DXGIGetDebugInterface(&IID_IDXGIInfoQueue, &dxgi_info_queue);
    
    assert_msgbox(hr == S_OK,
                  "DXGIGetDebugInterface Error",
                  "Failed to acquire IDXGIInfoQueue");
    
#if 0
    hr = DXGIGetDebugInterface1(0, &IID_IDXGIInfoQueue, &dxgi_info_queue);
    assert_true(hr == S_OK);
#endif
    
    IDXGIInfoQueue_SetBreakOnSeverity(dxgi_info_queue, DXGI_DEBUG_ALL, D3D11_MESSAGE_SEVERITY_CORRUPTION, TRUE);
    IDXGIInfoQueue_SetBreakOnSeverity(dxgi_info_queue, DXGI_DEBUG_ALL, D3D11_MESSAGE_SEVERITY_ERROR, TRUE);
    
    IDXGIInfoQueue_Release(dxgi_info_queue);
  }
#endif
  
  {
    
    W32_Window *w32_window = (W32_Window *)window;
    
    IDXGIDevice *dxgi_device = null;
    IDXGIAdapter *dxgi_adapter = null;
    IDXGIFactory2 *dxgi_factory = null;
    
    DXGI_SWAP_CHAIN_DESC1 swap_chain_desc;
    swap_chain_desc.Width = window->client_width;
    swap_chain_desc.Height = window->client_height;
    swap_chain_desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    swap_chain_desc.Stereo = FALSE;
    swap_chain_desc.SampleDesc.Count = 1;
    swap_chain_desc.SampleDesc.Quality = 0;
    swap_chain_desc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    swap_chain_desc.BufferCount = 2;
    swap_chain_desc.Scaling = DXGI_SCALING_STRETCH;
    swap_chain_desc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
    swap_chain_desc.AlphaMode = DXGI_ALPHA_MODE_UNSPECIFIED;
    swap_chain_desc.Flags = 0;
    
    hr = ID3D11Device_QueryInterface(d3d11_state->base_device, &IID_IDXGIDevice, &dxgi_device);
    assert_msgbox(hr == S_OK,
                  "ID3D11Device_QueryInterface Error",
                  "Failed to acquire IDXGIDevice");
    
    hr = IDXGIDevice_GetAdapter(dxgi_device, &dxgi_adapter);
    assert_msgbox(hr == S_OK,
                  "IDXGIDevice_GetAdapter Error",
                  "Failed to acquire IDXGIAdapter");
    
    hr = IDXGIAdapter_GetParent(dxgi_adapter, &IID_IDXGIFactory2, &dxgi_factory);
    assert_msgbox(hr == S_OK,
                  "IDXGIAdapter_GetParent Error",
                  "Failed to acquire IDXGIFactory2");
    
    hr = IDXGIFactory2_CreateSwapChainForHwnd(dxgi_factory, (IUnknown *)d3d11_state->base_device,
                                              w32_window->handle, &swap_chain_desc, null, null,
                                              &d3d11_state->dxgi_swap_chain);
    assert_msgbox(hr == S_OK,
                  "IDXGIFactory2_CreateSwapChainForHwnd Error",
                  "Failed to acquire IDXGISwapChain1");
    
    hr = IDXGIFactory2_MakeWindowAssociation(dxgi_factory, w32_window->handle, DXGI_MWA_NO_ALT_ENTER);
    assert_msgbox(hr == S_OK,
                  "IDXGIFactory2_MakeWindowAssociation Error",
                  "Failed to disable ALT ENTER Fullscreen");
    
    hr = IDXGISwapChain1_GetBuffer(d3d11_state->dxgi_swap_chain, 0,
                                   &IID_ID3D11Texture2D, &d3d11_state->back_buffer);
    assert_msgbox(hr == S_OK,
                  "IDXGISwapChain1_GetBuffer Error",
                  "Failed to acquire back buffer texture");
    
    D3D11_RENDER_TARGET_VIEW_DESC rtv_desc = {0};
    rtv_desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
    rtv_desc.ViewDimension = D3D11_RTV_DIMENSION_TEXTURE2D; 
    ID3D11Device_CreateRenderTargetView(d3d11_state->base_device, (ID3D11Resource *)d3d11_state->back_buffer,
                                        &rtv_desc, &d3d11_state->render_target_view);
    assert_msgbox(hr == S_OK,
                  "ID3D11Device_CreateRenderTargetView Error",
                  "Failed to acquire create render target view from back buffer texture");
    
    IDXGIFactory2_Release(dxgi_factory);
    IDXGIAdapter_Release(dxgi_adapter);
    IDXGIDevice_Release(dxgi_device);
  }
  
  {
    // TODO(christian): this UI code isnt done. address the sdf renderer code.
    // NOTE(christian): UI Renderer
    char ui_hlsl[] = 
      "#line " stringify(__LINE__) "\n"
      "cbuffer UI_Constants : register(b0) {\n"
      "  float4x4 ortho;\n"
      "};\n"
      "\n"
      "struct UI_Quad {\n"
      "  float2 p;\n"
      "  float2 p_unbiased;\n"
      "  float2 dims;\n"
      "  float2 dims_unbiased;\n"
      "  float4 vertex_colours[4];\n"
      "  float vertex_roundness;\n"
      "  float side_thickness;\n"
      "  float side_smoothness;\n"
      "\n"
      "  float2 shadow_offset;\n"
      "  float2 shadow_dim_offset;\n"
      "  float4 shadow_colours[4];\n"
      "  float shadow_roundness;\n"
      "  float shadow_smoothness;\n"
      "\n"
      "  float2 uvs[4];\n"
      "  uint texture_id;\n"
      "};\n"
      "\n"
      "struct UI_VSQuadVert {\n"
      "  float4 position : SV_Position;\n"
      "  float4 colour : colour;\n"
      "  float2 center : Center;\n"
      "  float2 p_unbiased : PUnbiased;\n"
      "  float2 half_dim : HalfDim;\n"
      "  float roundness : Roundness;\n"
      "  float thickness : Thickness;\n"
      "  float smoothness : Smoothness;\n"
      "  float2 shadow_offset : ShadowOffset;\n"
      "  float2 shadow_dim_offset : ShadowDimOffset;\n"
      "  float4 shadow_colour : ShadowColour;\n"
      "  float shadow_roundness : ShadowRoundness;\n"
      "  float shadow_smoothness : ShadowSmoothness;\n"
      "  float2 uv : UVTex;\n"
      "  uint texture_id : TextureID;\n"
      "};\n"
      "\n"
      "float\n"
      "sdf_rect(float2 sample, float2 center, float2 half_dim, float roundness) {\n"
      "  float2 rel_p = abs(center - sample);\n"
      "  float x_dist = rel_p.x - half_dim.x + roundness;\n"
      "  float y_dist = rel_p.y - half_dim.y + roundness;\n"
      "  float center_dist = length(rel_p - half_dim + roundness);\n"
      "  if ((x_dist > 0.0f) && (y_dist > 0.0f)) {\n"
      "    return center_dist - roundness;\n"
      "  } else {\n"
      "    return max(x_dist, y_dist) - roundness;\n"
      "  }\n"
      "}\n"
      "\n"
      "float4\n"
      "composite(float4 back, float4 front) {\n"
      "  float4 result = front.a * front + (1.0f - front.a) * back;\n"
      "  if (back.a == 0.0f) {\n"
      "    result = front;\n"
      "  }\n"
      "  return(result);\n"
      "}\n"
      "\n"
      "float4\n"
      "apply_sdf_alpha(float4 colour, float a) {\n"
      "  float4 result = float4(colour.rgb, colour.a * a);\n"
      "  return(result);\n"
      "}\n"
      "\n"
      "StructuredBuffer<UI_Quad> quad_buffer : register(t0);\n"
      "SamplerState point_sampler : register(s0);\n"
      "Texture2D<float4> tex0 : register(t1);\n"
      "Texture2D<float4> tex1 : register(t2);\n"
      "Texture2D<float4> tex2 : register(t3);\n"
      "Texture2D<float4> tex3 : register(t4);\n"
      "\n"
      "UI_VSQuadVert\n"
      "ui_vs_main(uint vid : SV_VertexID, uint iid : SV_InstanceID) {\n"
      "  UI_Quad quad = quad_buffer[iid];\n"
      "  UI_VSQuadVert result = (UI_VSQuadVert)0;\n"
      "  float2 p = (float2)0;\n"
      "  switch (vid) {\n"
      "    case 0: {\n"
      "      p = quad.p;\n"
      "    } break;\n"
      "    case 1: {\n"
      "      p = quad.p + float2(quad.dims.x, 0);\n"
      "    } break;\n"
      "    case 2: {\n"
      "      p = quad.p + float2(0, quad.dims.y);\n"
      "    } break;\n"
      "    case 3: {\n"
      "      p = quad.p + quad.dims;\n"
      "    } break;\n"
      "  }\n"
      "  result.position = mul(ortho, float4(p, 0, 1));\n"
      "  result.colour = quad.vertex_colours[vid];\n"
      "  result.p_unbiased = quad.p_unbiased;\n"
      "  result.half_dim = quad.dims_unbiased * 0.5f;\n"
      "  result.center = quad.p_unbiased + result.half_dim;\n"
      "  result.roundness = quad.vertex_roundness;\n"
      "  result.thickness = quad.side_thickness;\n"
      "  result.smoothness = quad.side_smoothness;\n"
      "  result.shadow_offset = quad.shadow_offset;\n"
      "  result.shadow_dim_offset = quad.shadow_dim_offset;\n"
      "  result.shadow_colour = quad.shadow_colours[vid];\n"
      "  result.shadow_roundness = quad.shadow_roundness;\n"
      "  result.shadow_smoothness = quad.shadow_smoothness;\n"
      "  result.uv = quad.uvs[vid];\n"
      "  result.texture_id = quad.texture_id;\n"
      "  return(result);\n"
      "}\n"
      "\n"
      "float4\n"
      "linear_from_srgb(float4 r) {\n"
      "  float4 result = float4(pow(r.rgb, 2.2f), r.a);\n"
      "  return(result);\n"
      "}\n"
      "\n"
      "float4\n"
      "ui_ps_main(UI_VSQuadVert inp) : SV_Target {\n"
      "  float2 pixel_p = inp.position.xy;\n"
      "  float4 bg_colour = float4(0.0f, 0.0f, 0.0f, 0.0f);\n"
      "  float4 final_colour = bg_colour;\n"
      "  float4 colour = linear_from_srgb(inp.colour);\n"
      "  float4 shadow_colour = linear_from_srgb(inp.shadow_colour);\n"
      "	if (inp.texture_id == 1) {\n"
      "		colour *= linear_from_srgb(tex0.Sample(point_sampler, inp.uv));\n"
      "	} else if (inp.texture_id == 2) {\n"
      "		colour *= linear_from_srgb(tex1.Sample(point_sampler, inp.uv));\n"
      "	} else if (inp.texture_id == 3) {\n"
      "		colour *= linear_from_srgb(tex2.Sample(point_sampler, inp.uv));\n"
      "	} else if (inp.texture_id == 4) {\n"
      "		colour *= linear_from_srgb(tex3.Sample(point_sampler, inp.uv));\n"
      "	}\n"
      
      "\n"
      
      "  if (inp.shadow_smoothness > 0.0f) {\n"
      "    float shadow_dist = sdf_rect(pixel_p, inp.center + inp.shadow_offset, inp.half_dim + inp.shadow_dim_offset, inp.shadow_roundness);\n"
      "    float shadow_alpha = 1.0f - smoothstep(-inp.shadow_smoothness, inp.shadow_smoothness, shadow_dist);\n"
      "    final_colour = composite(final_colour, apply_sdf_alpha(shadow_colour, shadow_alpha));\n"
      "  }\n"
      
      "\n"
      "  float side_thickness = inp.thickness * 0.5f;\n"
      "  float rect_dist = sdf_rect(pixel_p, inp.center, inp.half_dim, inp.roundness);\n"
      "  float rect_alpha = 1.0f - smoothstep(-inp.smoothness, inp.smoothness, rect_dist);\n"
      "  final_colour = composite(final_colour, apply_sdf_alpha(colour, rect_alpha));\n"
      "\n"
      
      "\n"
      "  if (inp.thickness > 0.0f) {\n"
      "    float2 border_v = float2(side_thickness, side_thickness);\n"
      "    float length_border = length(border_v);\n"
      "    float border_alpha = sdf_rect(pixel_p, inp.center, inp.half_dim - length_border, inp.roundness - length_border);\n"
      "    border_alpha = smoothstep(-inp.smoothness, inp.smoothness, border_alpha);\n"
      "    final_colour = apply_sdf_alpha(final_colour, border_alpha);\n"
      
      "    if (inp.shadow_smoothness > 0.0f) {\n"
      "      float shadow_dist = sdf_rect(pixel_p, inp.center + inp.shadow_offset, inp.half_dim + inp.shadow_dim_offset, inp.shadow_roundness);\n"
      "      float shadow_alpha = smoothstep(-inp.shadow_smoothness, inp.shadow_smoothness, shadow_dist);\n"
      "      final_colour = composite(final_colour, apply_sdf_alpha(apply_sdf_alpha(shadow_colour, rect_alpha), shadow_alpha));\n"
      "    }\n"
      "  }\n"
      
      "  return(final_colour);\n"
      "}\n"
      ;
    
    hr = r_d3d11_acquire_shader_handles(d3d11_state->main_device, ui_hlsl, sizeof(ui_hlsl),
                                        &(d3d11_state->ui_vertex_shader), &(d3d11_state->ui_pixel_shader),
                                        "ui_vs_main", "ui_ps_main");
    
    assert_msgbox(hr == S_OK,
                  "r_d3d11_acquire_shader_handles Error",
                  "Failed to compile UI shader. Please see the error message printed in the logs.");
  }
  
  {
    // NOTE(christian): Ortho2D
    char game2D_hlsl[] = 
      "#line " stringify(__LINE__) "\n"
      "cbuffer UI_Constants : register(b0) {\n"
      "  float4x4 ortho;\n"
      "};\n"
      "\n"
      "struct Game2D_Quad {\n"
      "  float2 p;\n"
      "  float2 dims;\n"
      "  float4 vertex_colours[4];\n"
      "  float2 uvs[4];\n"
      "  uint texture_id;\n"
      "};\n"
      "\n"
      "struct Game2D_VSQuadVert {\n"
      "  float4 position : SV_Position;\n"
      "  float4 colour : colour;\n"
      "  float2 center : Center;\n"
      "  float2 half_dim : HalfDim;\n"
      "  float2 uv : UVTex;\n"
      "  uint texture_id : TextureID;\n"
      "};\n"
      "\n"
      "StructuredBuffer<Game2D_Quad> quad_buffer : register(t0);\n"
      "SamplerState point_sampler : register(s0);\n"
      "Texture2D<float4> tex0 : register(t1);\n"
      "Texture2D<float4> tex1 : register(t2);\n"
      "Texture2D<float4> tex2 : register(t3);\n"
      "Texture2D<float4> tex3 : register(t4);\n"
      "\n"
      "Game2D_VSQuadVert\n"
      "game2D_vs_main(uint vid : SV_VertexID, uint iid : SV_InstanceID) {\n"
      "  Game2D_Quad quad = quad_buffer[iid];\n"
      "  Game2D_VSQuadVert result = (Game2D_VSQuadVert)0;\n"
      "  float2 p = (float2)0;\n"
      "  switch (vid) {\n"
      "    case 0: {\n"
      "      p = quad.p;\n"
      "    } break;\n"
      "    case 1: {\n"
      "      p = quad.p + float2(quad.dims.x, 0);\n"
      "    } break;\n"
      "    case 2: {\n"
      "      p = quad.p + float2(0, quad.dims.y);\n"
      "    } break;\n"
      "    case 3: {\n"
      "      p = quad.p + quad.dims;\n"
      "    } break;\n"
      "  }\n"
      "  result.position = mul(ortho, float4(p, 0, 1));\n"
      "  result.colour = quad.vertex_colours[vid];\n"
      "  result.half_dim = quad.dims * 0.5f;\n"
      "  result.center = quad.p + result.half_dim;\n"
      "  result.uv = quad.uvs[vid];\n"
      "  result.texture_id = quad.texture_id;\n"
      "  return(result);\n"
      "}\n"
      "\n"
      "float4\n"
      "linear_from_srgb(float4 r) {\n"
      "  float4 result = float4(pow(r.rgb, 2.2f), r.a);\n"
      "  return(result);\n"
      "}\n"
      "\n"
      "float4\n"
      "game2D_ps_main(Game2D_VSQuadVert inp) : SV_Target {\n"
      "  float4 sample = linear_from_srgb(inp.colour);\n"
      "	if (inp.texture_id == 1) {\n"
      "		sample *= linear_from_srgb(tex0.Sample(point_sampler, inp.uv));\n"
      "	} else if (inp.texture_id == 2) {\n"
      "		sample *= linear_from_srgb(tex1.Sample(point_sampler, inp.uv));\n"
      "	} else if (inp.texture_id == 3) {\n"
      "		sample *= linear_from_srgb(tex2.Sample(point_sampler, inp.uv));\n"
      "	} else if (inp.texture_id == 4) {\n"
      "		sample *= linear_from_srgb(tex3.Sample(point_sampler, inp.uv));\n"
      "	}\n"
      "  return(sample);\n"
      "}\n"
      ;
    
    hr = r_d3d11_acquire_shader_handles(d3d11_state->main_device, game2D_hlsl, sizeof(game2D_hlsl),
                                        &(d3d11_state->ortho2d_vertex_shader), &(d3d11_state->ortho2d_pixel_shader),
                                        "game2D_vs_main", "game2D_ps_main");
    
    assert_msgbox(hr == S_OK,
                  "r_d3d11_acquire_shader_handles Error",
                  "Failed to compile Ortho2D shader. Please see the error message printed in the logs.");
  }
  
  // NOTE(christian): Setup raster
  {
    
    D3D11_RASTERIZER_DESC1 rasterizer_desc;
    rasterizer_desc.FillMode = D3D11_FILL_SOLID;
    rasterizer_desc.CullMode = D3D11_CULL_NONE;
    rasterizer_desc.FrontCounterClockwise = TRUE;
    rasterizer_desc.DepthBias = 0;
    rasterizer_desc.DepthBiasClamp = 0.0f;
    rasterizer_desc.SlopeScaledDepthBias = 0.0f;
    rasterizer_desc.DepthClipEnable = FALSE;
    rasterizer_desc.ScissorEnable = FALSE;
    rasterizer_desc.MultisampleEnable = FALSE;
    rasterizer_desc.AntialiasedLineEnable = FALSE;
    rasterizer_desc.ForcedSampleCount = 0;
    
    hr = ID3D11Device1_CreateRasterizerState1(d3d11_state->main_device, &rasterizer_desc, &d3d11_state->rasterizer_state);
    assert_msgbox(hr == S_OK,
                  "ID3D11Device1_CreateRasterizerState1 Error",
                  "Failed to create ID3D11RasterizerState1.");
    
    D3D11_BLEND_DESC1 blend_desc;
    blend_desc.AlphaToCoverageEnable = FALSE;
    blend_desc.IndependentBlendEnable = FALSE;
    blend_desc.RenderTarget->BlendEnable = TRUE;
    blend_desc.RenderTarget->LogicOpEnable = FALSE;
    blend_desc.RenderTarget->SrcBlend = D3D11_BLEND_SRC_ALPHA;
    blend_desc.RenderTarget->DestBlend = D3D11_BLEND_INV_SRC_ALPHA;
    blend_desc.RenderTarget->BlendOp = D3D11_BLEND_OP_ADD;
    blend_desc.RenderTarget->SrcBlendAlpha = D3D11_BLEND_SRC_ALPHA;
    blend_desc.RenderTarget->DestBlendAlpha = D3D11_BLEND_INV_SRC_ALPHA;
    blend_desc.RenderTarget->BlendOpAlpha = D3D11_BLEND_OP_ADD;
    blend_desc.RenderTarget->LogicOp = D3D11_LOGIC_OP_NOOP;
    blend_desc.RenderTarget->RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;
    
    hr = ID3D11Device1_CreateBlendState1(d3d11_state->main_device, &blend_desc, &d3d11_state->blend_state);
    assert_msgbox(hr == S_OK,
                  "ID3D11Device1_CreateBlendState1 Error",
                  "Failed to create ID3D11BlendState1.");
    
    D3D11_SAMPLER_DESC sampler_desc;
    sampler_desc.Filter = D3D11_FILTER_MIN_LINEAR_MAG_MIP_POINT;
    sampler_desc.AddressU = D3D11_TEXTURE_ADDRESS_CLAMP;
    sampler_desc.AddressV = D3D11_TEXTURE_ADDRESS_CLAMP;
    sampler_desc.AddressW = D3D11_TEXTURE_ADDRESS_CLAMP;
    sampler_desc.MipLODBias = 0.0f;
    sampler_desc.MaxAnisotropy = 1;
    sampler_desc.ComparisonFunc = D3D11_COMPARISON_NEVER;
    sampler_desc.BorderColor[0] = 1.0f;
    sampler_desc.BorderColor[1] = 1.0f;
    sampler_desc.BorderColor[2] = 1.0f;
    sampler_desc.BorderColor[3] = 1.0f;
    sampler_desc.MinLOD = 0;
    sampler_desc.MaxLOD = D3D11_FLOAT32_MAX;
    
    hr = ID3D11Device1_CreateSamplerState(d3d11_state->main_device, &sampler_desc, &d3d11_state->texture_sampler);
    assert_msgbox(hr == S_OK,
                  "ID3D11Device1_CreateSamplerState Error",
                  "Failed to create ID3D11SamplerState.");
    
  }
  
  {
    // NOTE(christian): ORTHO2D BUFFER
    hr = r_d3d11_create_buffer(d3d11_state->main_device, R_D3D11_BufferType_Structured,
                               array_count(((R_RenderPass_Game_Ortho2D *)0)->rects),
                               sizeof(R_O2D_Rect),
                               &d3d11_state->ortho2d_quad_buffer,
                               &d3d11_state->ortho2d_quad_buffer_srv);
    
    assert_msgbox(hr == S_OK,
                  "r_d3d11_create_buffer_structured Error",
                  "Failed to create Structured Buffer for Ortho2D.");
    
    // TODO(christian): ORTHO3D BUFFER
    
    // NOTE(christian): UI BUFFER
    hr = r_d3d11_create_buffer(d3d11_state->main_device, R_D3D11_BufferType_Structured,
                               array_count(((R_RenderPass_UI *)0)->rects),
                               sizeof(R_UI_Rect),
                               &d3d11_state->ui_quad_buffer,
                               &d3d11_state->ui_quad_buffer_srv);
    
    assert_msgbox(hr == S_OK,
                  "r_d3d11_create_buffer_structured Error",
                  "Failed to create Structured Buffer for UI.");
  }
  
  {
    // NOTE(christian): CONST BUFFS UI
    hr = r_d3d11_create_buffer(d3d11_state->main_device, R_D3D11_BufferType_Constant,
                               1, sizeof(R_D3D11_Constants_Main),
                               &(d3d11_state->main_constant_buffer),
                               null);
    
    assert_msgbox(hr == S_OK,
                  "r_d3d11_create_buffer_structured Error",
                  "Failed to create Structured Buffer for UI.");
  }
  
  {
    // NOTE(christian): FREETYPE
    read_only u32 face_sizes[R_FontSize_Count] = {
      8, 10, 12
    };
    
    FT_Library library;
    FT_Face face;
    FT_Error error;
    
    error = FT_Init_FreeType(&library);
    assert_msgbox(error == FT_Err_Ok, "FT_Init_FreeType Error", "Failed to initialize FreeType.");
    
    error = FT_New_Face(library, "..\\data\\assets\\fonts\\dos437.ttf", 0, &face);
    assert_msgbox(error == FT_Err_Ok, "FT_New_Face Error", "Failed to load dos437.ttf");
    
    Memory_Arena *font_parse_scratch = arena_get_scratch(null, 0);
    Temporary_Memory font_parse_mem = temp_mem_begin(font_parse_scratch);
    u32 bytes_per_pixel = 4;
    u32 atlas_width = 512;
    u32 atlas_height = 512;
    
    u32 glyph_atlas_gap = 4;
    u8 *glyph_atlas = (u8 *)(arena_push(font_parse_scratch, bytes_per_pixel * atlas_width * atlas_height));
    
    for (u32 font_size_index = 0;
         font_size_index < R_FontSize_Count;
         ++font_size_index) {
      u32 glyph_atlas_col = glyph_atlas_gap;
      u32 glyph_atlas_row = glyph_atlas_gap;
      R2D_FontParsed *font_parsed = result->font + font_size_index;
      font_parsed->ascent = face->ascender >> 6;
      font_parsed->descent = face->descender >> 6;
      s32 line_space = face_sizes[font_size_index] + (font_parsed->ascent - font_parsed->descent);
      
      error = FT_Set_Pixel_Sizes(face, 0, face_sizes[font_size_index]);
      assert_msgbox(error == FT_Err_Ok, "FT_Set_Pixel_Sizes Error", "Failed to set font size.");
      
      for (u32 codepoint = 0; codepoint < 128; ++codepoint) {
        if ((glyph_atlas_col + glyph_atlas_gap + face_sizes[font_size_index]) >= atlas_width) {
          glyph_atlas_col = glyph_atlas_gap;
          glyph_atlas_row += face_sizes[font_size_index] + line_space;
          
          assert_msgbox(error == FT_Err_Ok, "Glyph Atlas Pen Overflow Error", "Consider Resizing the glyph atlas.");
        }
        
        u32 glyph_index = FT_Get_Char_Index(face, codepoint);
        error = FT_Load_Glyph(face, glyph_index, FT_LOAD_DEFAULT);
        assert_msgbox(error == FT_Err_Ok, "FT_Load_Glyph Error", "Failed to load glyph");
        
        error = FT_Render_Glyph(face->glyph, FT_RENDER_MODE_MONO);
        assert_msgbox(error == FT_Err_Ok, "FT_Render_Glyph Error", "Failed to render glyph");
        
        FT_GlyphSlotRec *slot = face->glyph;
        FT_Bitmap *bitmap = &(slot->bitmap);
        
        u8 *alpha_and_stuff = bitmap->buffer;
        u8 *out_pixels = glyph_atlas + ((glyph_atlas_row * atlas_width + glyph_atlas_col) * bytes_per_pixel);
        u32 out_pitch = atlas_width * bytes_per_pixel;
        u32 src_pitch = bitmap->pitch;
        
        for (u32 pix_y = 0; pix_y < bitmap->rows; ++pix_y) {
          u8 *src_row = alpha_and_stuff;
          u8 *out_row = out_pixels;
          for (u32 pix_x = 0; pix_x < bitmap->width; ++pix_x) {
            u8 color = (src_row[pix_x / 8] & (1 << (7 - (pix_x % 8)))) ? 255 : 0;
            *out_row++ = color;
            *out_row++ = color;
            *out_row++ = color;
            *out_row++ = color;
          }
          out_pixels += out_pitch;
          alpha_and_stuff += src_pitch;
        }
        
        FT_Glyph_Metrics *metrics = &(face->glyph->metrics);
        R2D_GlyphInfo *glyph_info = font_parsed->glyphs + codepoint;
        glyph_info->x_p_in_atlas = glyph_atlas_col;
        glyph_info->y_p_in_atlas = glyph_atlas_row;
        glyph_info->width = (f32)bitmap->width;
        glyph_info->height = (f32)bitmap->rows;
        glyph_info->advance = (f32)(metrics->horiAdvance >> 6);
        glyph_info->bearing_x = (f32)(metrics->horiBearingX >> 6);
        glyph_info->bearing_y = (f32)(metrics->horiBearingY >> 6);
        
        glyph_atlas_col += bitmap->width + glyph_atlas_gap;
      }
      
      font_parsed->atlas = r_texture_alloc(result, atlas_width, atlas_height, glyph_atlas);
      assert_msgbox(!r_handle_match(font_parsed->atlas, r_handle_make_bad()), "r_texture_alloc Error", "Failed to create texture.");
      
      clear_memory(glyph_atlas, bytes_per_pixel * atlas_width * atlas_height);
    }
    
    FT_Done_Face(face);
    FT_Done_FreeType(library);
    temp_mem_end(font_parse_mem);
  }
  
  return(result);
}

fun R_Handle
r_texture_alloc(R_Buffer *buffer, s32 width, s32 height, void *data) {
  R_Handle result = r_handle_make_bad();
  
  unsigned long enabled_p = 0;
  if (_BitScanForward(&enabled_p, (unsigned long)buffer->free_texture_flag) == 1) {
    if (enabled_p < array_count(buffer->textures)) {
      buffer->free_texture_flag &= ~(1 << enabled_p);
      R_D3D11_Texture2D *texture = arena_push_struct(buffer->arena, R_D3D11_Texture2D);
      D3D11_TEXTURE2D_DESC texture_desc = {
        .Width = (u32)(width),
        .Height = (u32)(height),
        .MipLevels = 1,
        .ArraySize = 1, 
        .Format = DXGI_FORMAT_R8G8B8A8_UNORM,
        .SampleDesc = { 1, 0 },
        .Usage = D3D11_USAGE_IMMUTABLE,
        .BindFlags = D3D11_BIND_SHADER_RESOURCE,
        .CPUAccessFlags = 0,
        .MiscFlags = 0,
      };
      
      D3D11_SUBRESOURCE_DATA texture_data = {
        data, (u32)(width * 4), 0
      };
      
      texture->width = (u32)width;
      texture->height = (u32)height;
      
      R_D3D11_State *d3d11_state = (R_D3D11_State *)buffer->reserved;
      
      if (ID3D11Device1_CreateTexture2D(d3d11_state->main_device, &texture_desc, &texture_data, &(texture->texture)) == S_OK) {
        if (ID3D11Device1_CreateShaderResourceView(d3d11_state->main_device, (ID3D11Resource *)texture->texture, null, &(texture->srv)) == S_OK) {
          //ID3D11DeviceContext_GenerateMips(d3d11_base_device_context, texture->srv);
          
          buffer->textures[enabled_p] = (void *)texture;
          result.h64[0] = (u64)enabled_p + 1; // one-bsed index for texture ids
          result.h64[1] = (u64)texture;
        }
      }
    }
  }
  
  return(result);
}


fun R_Handle
r_texture_from_file(R_Buffer *buffer, String_U8_Const filename) {
  R_Handle result = r_handle_make_bad();
  s32 image_width = 0;
  s32 image_height = 0;
  s32 image_num_comps = 0;
  u8 *image_data = stbi_load((const char *)(filename.string), 
                             &image_width, &image_height, &image_num_comps, 0);
  
  if (image_data) {
    result = r_texture_alloc(buffer, image_width, image_height, image_data);
    stbi_image_free(image_data);
  }
  
  return (result);
}

fun b32
r_texture_get_dims(R_Handle handle, s32 *width, s32 *height) {
  if ((handle.h64[0] >= 1) && (handle.h64[0] <= 4)) {
    R_D3D11_Texture2D *texture = (R_D3D11_Texture2D *)(handle.h64[1]);
    if (width) {
      *width = texture->width;
    }
    
    if (height) {
      *height = texture->height;
    }
    return(true);
  } else {
    return(false);
  }
}

inl void
r_viewport_set_dims(R_Buffer *buffer, u32 width, u32 height) {
  R_D3D11_State *d3d11_state = (R_D3D11_State *)buffer->reserved;
  
  D3D11_VIEWPORT viewport;
  viewport.TopLeftX = 0;
  viewport.TopLeftY = 0;
  viewport.Width = (f32)width;
  viewport.Height = (f32)height;
  viewport.MinDepth = 0.0f;
  viewport.MaxDepth = 1.0f;
  
  d3d11_state->viewport = viewport;
}

inl void
r_viewport_get_dims(R_Buffer *buffer, u32 *width, u32 *height) {
  R_D3D11_State *d3d11_state = (R_D3D11_State *)buffer->reserved;
  
  if (width) {
    *width = (u32)d3d11_state->viewport.Width;
  }
  
  if (height) {
    *height = (u32)d3d11_state->viewport.Height;
  }
}


fun void
r_submit_passes_to_gpu(R_Buffer *buffer, b32 vsync, f32 clear_r, f32 clear_g, f32 clear_b, f32 clear_a) {
  R_D3D11_State *state = (R_D3D11_State *)buffer->reserved;
  OS_Window *window = state->associated_window;
  assert_true(window != null);
  
  if (window->resized_this_frame) {
    assert_true(state->back_buffer != null);
		assert_true(state->render_target_view != null);
    
    // https://learn.microsoft.com/en-us/windows/win32/api/dxgi/nf-dxgi-idxgifactory-makewindowassociation
    // "on a WM_SIZE message, the application should release any outstanding swap-chain back buffers, call
    // IDXGISwapChain::ResizeBuffers, then re-acquire the back buffers from the swap chain(s)"
    
    HRESULT result = IDXGISwapChain1_ResizeBuffers(state->dxgi_swap_chain,
                                                   0,
                                                   window->client_width,
                                                   window->client_height,
                                                   DXGI_FORMAT_UNKNOWN,
                                                   DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH);
    
    assert_true(result == S_OK);
    
    result = IDXGISwapChain1_GetBuffer(state->dxgi_swap_chain, 0, &IID_ID3D11Texture2D, &state->back_buffer);
    assert_true(result == S_OK);
    
		D3D11_RENDER_TARGET_VIEW_DESC rtv_desc = {0};
		rtv_desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
		rtv_desc.ViewDimension = D3D11_RTV_DIMENSION_TEXTURE2D;
		result = ID3D11Device_CreateRenderTargetView(state->base_device, (ID3D11Resource *)state->back_buffer,
                                                 &rtv_desc, &state->render_target_view);
		assert_true(result == S_OK);
    
    r_viewport_set_dims(buffer, window->client_width, window->client_height);
  }
  
  D3D11_VIEWPORT viewport = state->viewport;
  
  f32 clear_colour[] = { clear_r * clear_r, clear_g * clear_g, clear_b * clear_b, clear_a };
  ID3D11DeviceContext_ClearRenderTargetView(state->base_device_context, state->render_target_view, clear_colour);
  
  for (R_RenderPass *curr_pass = buffer->first_pass;
       curr_pass;
       ) {
    R_RenderPass *pass = curr_pass;
    curr_pass = curr_pass->next;
    
    pass->next = buffer->free_passes;
    buffer->free_passes = pass;
    
    switch (pass->type) {
      case R_RenderPassType_Game_Ortho2D: {
        R_RenderPass_Game_Ortho2D *ortho2D = &(pass->game_ortho_2D);
        D3D11_MAPPED_SUBRESOURCE mapped_subresource;
        
        // NOTE(christian): constant buffer proj
        if (ID3D11DeviceContext_Map(state->base_device_context, (ID3D11Resource *)state->main_constant_buffer, 0,
                                    D3D11_MAP_WRITE_DISCARD, 0, &mapped_subresource) == S_OK) {
          m44 ortho = m44_make_ortho_lh_z01(0.0f, viewport.Width, 0.0f, viewport.Height, 0.0f, 1.0f);
          *(m44 *)mapped_subresource.pData = ortho;
          
          ID3D11DeviceContext_Unmap(state->base_device_context, (ID3D11Resource *)state->main_constant_buffer, 0);
        }
        
        // TODO(christian): MAP LIGHTS
        
        // NOTE(christian): rects drawn by game
        if (ID3D11DeviceContext_Map(state->base_device_context, (ID3D11Resource *)state->ortho2d_quad_buffer, 0,
                                    D3D11_MAP_WRITE_DISCARD, 0, &mapped_subresource) == S_OK) {
          copy_memory(mapped_subresource.pData, ortho2D->rects, ortho2D->rect_count * sizeof(R_O2D_Rect));
          //copy_memory(mapped_subresource.pData, ortho2D->rects, sizeof(ortho2D->rects));
          ID3D11DeviceContext_Unmap(state->base_device_context, (ID3D11Resource *)state->ortho2d_quad_buffer, 0);
        }
        
        {
          ID3D11DeviceContext_IASetPrimitiveTopology(state->base_device_context, D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);
        }
        
        {
          ID3D11DeviceContext_VSSetConstantBuffers(state->base_device_context, 0, 1, &(state->main_constant_buffer));
          ID3D11DeviceContext_VSSetShaderResources(state->base_device_context, 0, 1, &(state->ortho2d_quad_buffer_srv));
          ID3D11DeviceContext_VSSetShader(state->base_device_context, state->ortho2d_vertex_shader, null, 0);
        }
        
        {
          ID3D11DeviceContext_RSSetState(state->base_device_context, (ID3D11RasterizerState *)state->rasterizer_state);
          ID3D11DeviceContext_RSSetViewports(state->base_device_context, 1, &viewport);
        }
        
        {
          for (u32 texture_index = 0;
               texture_index < array_count(buffer->textures);
               ++texture_index) {
            if ((buffer->free_texture_flag & (1 << texture_index)) == 0) {
              R_D3D11_Texture2D *texture = (R_D3D11_Texture2D *)buffer->textures[texture_index];
              ID3D11DeviceContext_PSSetShaderResources(state->base_device_context, texture_index + 1, 1, &texture->srv);
            }
          }
          
          ID3D11DeviceContext_PSSetSamplers(state->base_device_context, 0, 1, &state->texture_sampler);
          //ID3D11DeviceContext_PSSetConstantBuffers(state->base_device_context, 1, 1, &state->ortho2d_light_constant_buffer);
          ID3D11DeviceContext_PSSetShader(state->base_device_context, state->ortho2d_pixel_shader, null, 0);
        }
        
        {
          ID3D11DeviceContext_OMSetBlendState(state->base_device_context, (ID3D11BlendState *)state->blend_state, null, 0xffffffff);
          ID3D11DeviceContext_OMSetRenderTargets(state->base_device_context, 1, &(state->render_target_view), null);
        }
        
        {
          ID3D11DeviceContext_DrawInstanced(state->base_device_context, 4, (UINT)ortho2D->rect_count, 0, 0);
        }
        
      } break;
      
      case R_RenderPassType_UI: {
        R_RenderPass_UI *ui = &(pass->ui);
        D3D11_MAPPED_SUBRESOURCE mapped_subresource;
        
        {
          if (ID3D11DeviceContext_Map(state->base_device_context, (ID3D11Resource *)state->main_constant_buffer, 0,
                                      D3D11_MAP_WRITE_DISCARD, 0, &mapped_subresource) == S_OK) {
            m44 ortho = m44_make_ortho_lh_z01(0.0f, viewport.Width, 0.0f, viewport.Height, 0.0f, 1.0f);
            *(m44 *)mapped_subresource.pData = ortho;
            
            ID3D11DeviceContext_Unmap(state->base_device_context, (ID3D11Resource *)state->main_constant_buffer, 0);
          }
        }
        
        {
          if (ID3D11DeviceContext_Map(state->base_device_context, (ID3D11Resource *)state->ui_quad_buffer, 0,
                                      D3D11_MAP_WRITE_DISCARD, 0, &mapped_subresource) == S_OK) {
            copy_memory(mapped_subresource.pData, ui->rects, ui->rect_count * sizeof(R_UI_Rect));
            ID3D11DeviceContext_Unmap(state->base_device_context, (ID3D11Resource *)state->ui_quad_buffer, 0);
          }
        }
        
        {
          ID3D11DeviceContext_IASetPrimitiveTopology(state->base_device_context, D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);
        }
        
        {
          ID3D11DeviceContext_VSSetConstantBuffers(state->base_device_context, 0, 1, &(state->main_constant_buffer));
          ID3D11DeviceContext_VSSetShaderResources(state->base_device_context, 0, 1, &(state->ui_quad_buffer_srv));
          ID3D11DeviceContext_VSSetShader(state->base_device_context, state->ui_vertex_shader, null, 0);
        }
        
        {
          ID3D11DeviceContext_RSSetState(state->base_device_context, (ID3D11RasterizerState *)state->rasterizer_state);
          ID3D11DeviceContext_RSSetViewports(state->base_device_context, 1, &viewport);
        }
        
        {
          for (u32 texture_index = 0;
               texture_index < array_count(buffer->textures);
               ++texture_index) {
            if ((buffer->free_texture_flag & (1 << texture_index)) == 0) {
              R_D3D11_Texture2D *texture = (R_D3D11_Texture2D *)buffer->textures[texture_index];
              ID3D11DeviceContext_PSSetShaderResources(state->base_device_context, texture_index + 1, 1, &(texture->srv));
            }
          }
          
          ID3D11DeviceContext_PSSetSamplers(state->base_device_context, 0, 1, &state->texture_sampler);
          ID3D11DeviceContext_PSSetShader(state->base_device_context, state->ui_pixel_shader, null, 0);
        }
        
        {
          ID3D11DeviceContext_OMSetRenderTargets(state->base_device_context, 1, &(state->render_target_view), null);
          ID3D11DeviceContext_OMSetBlendState(state->base_device_context, (ID3D11BlendState *)state->blend_state, null, 0xFFFFFFFF);
        }
        
        {
          ID3D11DeviceContext_DrawInstanced(state->base_device_context, 4, (UINT)ui->rect_count, 0, 0);
        }
        
      } break;
      
      default: {
        invalid_code_path();
      } break;
    }
    
    ID3D11DeviceContext_ClearState(state->base_device_context);
  }
  
  buffer->first_pass = buffer->last_pass = null;
  IDXGISwapChain1_Present(state->dxgi_swap_chain, vsync == true, 0);
}
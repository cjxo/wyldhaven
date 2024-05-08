#include "ext/freetype/ft2build.h"
#include FT_FREETYPE_H

#define STB_IMAGE_IMPLEMENTATION
#include "ext/stb_image.h"

typedef struct {
    Memory_Arena *arena;
    OS_Window *associated_window;
    
    // NOTE(christian): d3d11
    IDXGISwapChain1 *dxgi_swap_chain;
    ID3D11Device *base_device;
    ID3D11Device1 *main_device;
    ID3D11DeviceContext *base_device_context;
    ID3D11Texture2D *back_buffer;
    ID3D11RenderTargetView *render_target_view;
    ID3D11RasterizerState1 *rasterizer_state;
    D3D11_VIEWPORT viewport;
    
	// NOTE(christian): Shared buffers for game and UI
    ID3D11Buffer *main_constant_buffer;

	// NOTE(christian): UI renderer
//    ID3D11VertexShader *ui_vertex_shader;
  //  ID3D11PixelShader *ui_pixel_shader;
  //  ID3D11Buffer *ui_quad_buffer;

    //~ NOTE(christian): game renderer
    ID3D11BlendState1 *quad_blend_state;
    ID3D11SamplerState *quad_texture_sampler_state;
    ID3D11VertexShader *quad_vertex_shader;
    ID3D11PixelShader *quad_pixel_shader;
    ID3D11Buffer *game_quad_buffer;
    ID3D11ShaderResourceView *game_quad_srv;
    ID3D11Buffer *game_light_constant_buffer;
} D3D11_State;

typedef struct {
    u32 width;
    u32 height;
    ID3D11Texture2D *texture;
    ID3D11ShaderResourceView *srv;
} D3D11_Texture;

fun b32
d3d11_create_devices(D3D11_State *state) {
    b32 result = false;
    
    D3D_FEATURE_LEVEL feature_level = D3D_FEATURE_LEVEL_11_0;
    
    UINT flags = D3D11_CREATE_DEVICE_BGRA_SUPPORT;
#if defined(WC_DEBUG)
    flags |= D3D11_CREATE_DEVICE_DEBUG;
#endif
    HRESULT hr = D3D11CreateDevice(null, D3D_DRIVER_TYPE_HARDWARE, null, flags,
                                   &feature_level, 1, D3D11_SDK_VERSION, &state->base_device,
                                   null, &state->base_device_context);
    if (hr == S_OK) {
        hr = ID3D11Device_QueryInterface(state->base_device, &IID_ID3D11Device1, &state->main_device);
    }
    
    return(result);
}

#if defined(WC_DEBUG)
fun void
d3d11_enable_debug(D3D11_State *state) {
    ID3D11InfoQueue *info_queue = null;
    HRESULT hr = ID3D11Device_QueryInterface(state->base_device, &IID_ID3D11InfoQueue, &info_queue);
    assert_true(hr == S_OK);
    
    ID3D11InfoQueue_SetBreakOnSeverity(info_queue, D3D11_MESSAGE_SEVERITY_CORRUPTION, TRUE);
    ID3D11InfoQueue_SetBreakOnSeverity(info_queue, D3D11_MESSAGE_SEVERITY_ERROR, TRUE);
    
    ID3D11InfoQueue_Release(info_queue);
    
    IDXGIInfoQueue *dxgi_info_queue = null;
    hr = DXGIGetDebugInterface1(0, &IID_IDXGIInfoQueue, &dxgi_info_queue);
    assert_true(hr == S_OK);
    
    IDXGIInfoQueue_SetBreakOnSeverity(dxgi_info_queue, DXGI_DEBUG_ALL, D3D11_MESSAGE_SEVERITY_CORRUPTION, TRUE);
    IDXGIInfoQueue_SetBreakOnSeverity(dxgi_info_queue, DXGI_DEBUG_ALL, D3D11_MESSAGE_SEVERITY_ERROR, TRUE);
    
    IDXGIInfoQueue_Release(dxgi_info_queue);
}
#else
#define d3d11_enable_debug(...)
#endif

fun b32
d3d11_initialize_swapchain(D3D11_State *state, OS_Window *window) {
    b32 result = false;

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
    
    if (ID3D11Device_QueryInterface(state->base_device, &IID_IDXGIDevice, &dxgi_device) == S_OK) {
        if (IDXGIDevice_GetAdapter(dxgi_device, &dxgi_adapter) == S_OK) {
            if (IDXGIAdapter_GetParent(dxgi_adapter, &IID_IDXGIFactory2, &dxgi_factory) == S_OK) {
				IDXGIFactory2_MakeWindowAssociation(dxgi_factory, w32_window->handle, DXGI_MWA_NO_ALT_ENTER);
                if (IDXGIFactory2_CreateSwapChainForHwnd(dxgi_factory, (IUnknown *)state->base_device, w32_window->handle, &swap_chain_desc, null, null, &state->dxgi_swap_chain) == S_OK) {
                    if (IDXGISwapChain1_GetBuffer(state->dxgi_swap_chain, 0, &IID_ID3D11Texture2D, &state->back_buffer) == S_OK) {
                        D3D11_RENDER_TARGET_VIEW_DESC rtv_desc = {0};
                        rtv_desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
                        rtv_desc.ViewDimension = D3D11_RTV_DIMENSION_TEXTURE2D;
                        if (ID3D11Device_CreateRenderTargetView(state->base_device, (ID3D11Resource *)state->back_buffer,
                                                                &rtv_desc, &state->render_target_view) == S_OK) {
                            result = true;
                        }
                    }
                }
                
                IDXGIFactory2_Release(dxgi_factory);
            }
            
            IDXGIAdapter_Release(dxgi_adapter);
        }
        
        IDXGIDevice_Release(dxgi_device);
    }
    
    return(result);
}

inl b32
d3d11_create_quad_shaders(D3D11_State *state) {
    b32 result = false;
    
    ID3DBlob *bytecode_blob = null;
    ID3DBlob *error_blob = null;
    
    u32 flags = D3DCOMPILE_PACK_MATRIX_COLUMN_MAJOR | D3DCOMPILE_ENABLE_STRICTNESS;
#if defined(WC_DEBUG)
    flags |= D3DCOMPILE_DEBUG;
    flags |= D3DCOMPILE_SKIP_OPTIMIZATION;
    flags |= D3DCOMPILE_WARNINGS_ARE_ERRORS;
#else
    flags |= D3DCOMPILE_OPTIMIZATION_LEVEL3;
#endif

    const char hlsl[] =
        "#line " stringify(__LINE__) " \n"
        "#define max_lights 8\n"
        "#define LightInfo_EnableLightingForMe 0x1\n"
        "#define LightInfo_IsNotAffectedByLightIndex 0x2\n"
        "#define LightInfo_LightIndexNotAffectedMask 0x3C\n"
        "\n"
        "struct Light\n"
        "{\n"
        "    float4 p : Position;\n"
        "    float4 colour : Colour;\n"
        "    float constant_attenuation : ConstAtten;\n"
        "    float linear_attenuation : LinAtten;\n"
        "    float2 unused_a : UnusedA;\n"
        "    uint type : TypeOfLight;\n"
        "    int is_enabled : IsEnabled;\n"
        "    float2 unused_b : UnusedB;\n"
        "};\n"
        
        "cbuffer Constants : register(b0)\n"
        "{\n"
        "    float4x4 ortho;\n"
        "};\n"
        
        "cbuffer Light_Constants : register(b1)\n"
        "{\n"
        "    float4 ambient_colour : AmbientColour;\n"
        "    Light lights[max_lights] : LightsToSim;\n"
        "    int enabled_lights : EnableLights;\n"
        "    int3 unused : Unused;\n"
        "};\n"
        
        "struct Quad\n"
        "{\n"
        "    float2 origin : Origin;\n"
        "    float2 x_axis : X_Axis;\n"
        "    float2 y_axis : Y_Axis;\n"
        "    float4 corner_colours[4] : Colours;\n"
        "    float corner_roundness : Corner_Roundness;\n"
        "    float side_thickness : Side_Thickness;\n"
        "    float2 uvs[4] : UVS;\n"
        "    uint texture_id : Texture_ID;\n"
        "    uint lighting_info : LightingInfo;\n"
        "};\n"
        
        "struct VS_Out\n"
        "{\n"
        "    float4 position : SV_Position;\n"
        "    float4 colour : colour;\n"
        "\n"// NOTE(christian): for calculating sdf rect (not parallel!)
        "    float2 rect_center : Center;\n"
        "    float2 rect_half_dims : Half_Dims;\n"
        "    float corner_roundness : Corner_Roundness;\n"
        "    float side_thickness : Side_Thickness;\n"
        
        "    float2 uv : UV;\n"
        "    uint texture_id : Texture_ID;\n"
        
        "    uint lighting_info : LightingInfo;"
        "};\n"
        
        "StructuredBuffer<Quad> quad_buffer : register(t0);\n"
        "SamplerState point_sampler : register(s0);\n"
        "Texture2D<float4> tex0 : register(t1);\n"
        "Texture2D<float4> tex1 : register(t2);\n"
        "Texture2D<float4> tex2 : register(t3);\n"
        "Texture2D<float4> tex3 : register(t4);\n"
        
        "float sdf_rect(float2 sample, float2 center, float2 half_dims, float roundness)\n"
        "{\n"
        "    float2 rel_p = abs(center - sample);\n"
        "    float result = length(max(rel_p - half_dims + float2(roundness, roundness), float2(0, 0))) - roundness;\n"
        "    return result;\n"
        "}\n"
        
        "static float2 dim_multiplier[] = \n"
        "{\n"
        "    float2(0, 0),\n"
        "    float2(1, 0),\n"
        "    float2(0, 1),\n"
        "    float2(1, 1),\n"
        "};\n"
        
        "VS_Out vs_main(uint vid : SV_VertexID, uint iid : SV_InstanceID)\n"
        "{\n"
        "    VS_Out result;\n"
        "    Quad quad = quad_buffer[iid];\n"
        "    float2x2 coord = float2x2(quad.x_axis.x, quad.y_axis.x, quad.x_axis.y, quad.y_axis.y);\n"
        "    float2 offset = mul(coord, dim_multiplier[vid]);\n"
        "    result.position = mul(ortho, float4(quad.origin + offset, 0.0f, 1.0f));\n"
        "    result.colour = quad.corner_colours[vid];\n"
        "    result.rect_half_dims = mul(coord, float2(0.5f, 0.5f));\n"
        "    result.rect_center = quad.origin + result.rect_half_dims;\n"
        "    result.corner_roundness = quad.corner_roundness;\n"
        "    result.side_thickness = quad.side_thickness;\n"
        "    result.uv = quad.uvs[vid];\n"
        "    result.texture_id = quad.texture_id;\n"
        "    result.lighting_info = quad.lighting_info;\n"
        "    return result;\n"
        "}\n"
        
        "float4 ps_main(VS_Out input) : SV_Target\n"
        "{\n"
		"   float4 sample = float4(pow(input.colour.xyz, 2.2f), input.colour.w);\n"
		"	if (input.texture_id == 1) {\n"
		"		sample *= pow(tex0.Sample(point_sampler, input.uv), 2.2f);\n"
		"	} else if (input.texture_id == 2) {\n"
		"		sample *= pow(tex1.Sample(point_sampler, input.uv), 2.2f);\n"
		"	} else if (input.texture_id == 3) {\n"
		"		sample *= pow(tex2.Sample(point_sampler, input.uv), 2.2f);\n"
		"	} else if (input.texture_id == 4) {\n"
		"		sample *= pow(tex3.Sample(point_sampler, input.uv), 2.2f);\n"
		"	}\n"
		"\n"
		"	float smoothness = 0.85f;\n"
		"	float2 smoothnessv2 = float2(smoothness, smoothness);\n"
		"	if (input.corner_roundness <= 0.0f) {\n"
		"		input.corner_roundness = 1.0f;\n"
		"	}\n"
		"\n"
		"	float sdf = sdf_rect(input.position.xy, input.rect_center, input.rect_half_dims, input.corner_roundness);\n"
		"	float x = 1.0f - smoothstep(-smoothness, smoothness, sdf);\n"
		"	sample *= x;\n"
		"\n"
		"	if (input.side_thickness >= 1.0f) {\n"
		"		float2 new_half_dim = input.rect_half_dims - 0.75f * float2(input.side_thickness, input.side_thickness);\n"
		"		float roundness_ratio = pow(min(new_half_dim.x / input.rect_half_dims.x, new_half_dim.y / input.rect_half_dims.y), 2);\n"
		"		float new_roundness = input.corner_roundness * roundness_ratio;\n"
		"		sdf = sdf_rect(input.position.xy, input.rect_center, new_half_dim - smoothnessv2, new_roundness);\n"
		"		x = smoothstep(-smoothness * 0.5f, smoothness * 0.5f, sdf);"
		"		sample *= x;\n"
		"	}\n"
		"\n"
		"\n"
        "	if (enabled_lights && (input.lighting_info & LightInfo_EnableLightingForMe))\n"
        "   {\n"
        "       Light light = lights[0];\n"
        "       bool is_not_affected_by_index = input.lighting_info & LightInfo_IsNotAffectedByLightIndex;\n"
        "       uint light_index = (input.lighting_info & LightInfo_LightIndexNotAffectedMask) >> 2;\n"
        "       if (!(is_not_affected_by_index && (light_index == 0)))\n"
        "       {\n"
        "           float2 surface_to_light = light.p.xy - input.position.xy;\n"
        "           float dist_to_light = length(surface_to_light);\n"
        "           surface_to_light /= dist_to_light;\n"
        "           float attenuation = 1.0f / (light.constant_attenuation + light.linear_attenuation * dist_to_light);\n"
        "\n"
        "           float4 light_final_colour = attenuation * light.colour;\n"
        "           sample = sample * (ambient_colour + light_final_colour);\n"
        "       }\n"
        "   } else if (enabled_lights) \n"
        "   {\n"
        "       sample *= ambient_colour;\n"
        "   }\n"
        "\n"
        "   return sample;\n"
        "}\n"
        ;
    
    OutputDebugStringA(hlsl);
    
    HRESULT hr = D3DCompile(hlsl, sizeof(hlsl), null, null, D3D_COMPILE_STANDARD_FILE_INCLUDE,
                            "vs_main", "vs_5_0", flags, 0, &bytecode_blob, &error_blob);
    
    if (hr == S_OK) {
        assert_true(bytecode_blob != null);
        ID3D11Device1_CreateVertexShader(state->main_device, ID3D10Blob_GetBufferPointer(bytecode_blob), 
                                         ID3D10Blob_GetBufferSize(bytecode_blob),
                                         null, &state->quad_vertex_shader);
        
        ID3D10Blob_Release(bytecode_blob);
        bytecode_blob = null;
    }
    
    if (error_blob) {
        OutputDebugStringA(ID3D10Blob_GetBufferPointer(error_blob));
        ID3D10Blob_Release(error_blob);
        error_blob = null;
        
        os_message_box(str8("Error"), str8("Failed to create vertex shader. See error message"));
        os_exit_process(1);
    }
    
    hr = D3DCompile(hlsl, sizeof(hlsl), null, null, D3D_COMPILE_STANDARD_FILE_INCLUDE,
                    "ps_main", "ps_5_0", flags, 0, &bytecode_blob, &error_blob);
    if (hr == S_OK) {
        assert_true(bytecode_blob != null);
        ID3D11Device1_CreatePixelShader(state->main_device, ID3D10Blob_GetBufferPointer(bytecode_blob), 
                                        ID3D10Blob_GetBufferSize(bytecode_blob),
                                        null, &state->quad_pixel_shader);
        
        ID3D10Blob_Release(bytecode_blob);
        bytecode_blob = null;
    }
    
    if (error_blob) {
        OutputDebugStringA(ID3D10Blob_GetBufferPointer(error_blob));
        ID3D10Blob_Release(error_blob);
        error_blob = null;
        
        os_message_box(str8("Error"), str8("Failed to create pixel shader. See error message"));
        os_exit_process(1);
    }
    
    return result;
}

inl b32
d3d11_setup_rasterizer(D3D11_State *state) {
    b32 result = false;
    
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
    
    if (ID3D11Device1_CreateRasterizerState1(state->main_device, &rasterizer_desc, &state->rasterizer_state) == S_OK) {
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
        if (ID3D11Device1_CreateBlendState1(state->main_device, &blend_desc, &state->quad_blend_state) == S_OK) {
            D3D11_SAMPLER_DESC sampler_desc;
            sampler_desc.Filter = D3D11_FILTER_MIN_LINEAR_MAG_MIP_POINT;
            sampler_desc.AddressU = D3D11_TEXTURE_ADDRESS_WRAP;
            sampler_desc.AddressV = D3D11_TEXTURE_ADDRESS_WRAP;
            sampler_desc.AddressW = D3D11_TEXTURE_ADDRESS_WRAP;
            sampler_desc.MipLODBias = 0.0f;
            sampler_desc.MaxAnisotropy = 1;
            sampler_desc.ComparisonFunc = D3D11_COMPARISON_NEVER;
            sampler_desc.BorderColor[0] = 1.0f;
            sampler_desc.BorderColor[1] = 1.0f;
            sampler_desc.BorderColor[2] = 1.0f;
            sampler_desc.BorderColor[3] = 1.0f;
            sampler_desc.MinLOD = 0;
            sampler_desc.MaxLOD = D3D11_FLOAT32_MAX;
            if (ID3D11Device1_CreateSamplerState(state->main_device, &sampler_desc, &state->quad_texture_sampler_state) == S_OK) {
                result = true;
            }
        }
    }
    
    return(result);
}

typedef struct {
    m44 orho_projection;
} D3D11_Constants;

fun void
d3d11_initialize_buffers(R2D_Buffer *buffer, u32 num_quads) {
	HRESULT hr;
    
	// GAME BUFFERS
    D3D11_State *state = (D3D11_State *)buffer->reserved;
    D3D11_BUFFER_DESC quad_buffer_desc;
    quad_buffer_desc.ByteWidth = num_quads * sizeof(R2D_Quad);
    quad_buffer_desc.Usage = D3D11_USAGE_DYNAMIC;
    quad_buffer_desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
    quad_buffer_desc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
    quad_buffer_desc.MiscFlags = D3D11_RESOURCE_MISC_BUFFER_STRUCTURED;
    quad_buffer_desc.StructureByteStride = sizeof(R2D_Quad);

	hr = ID3D11Device1_CreateBuffer(state->main_device, &quad_buffer_desc, null, &state->game_quad_buffer);
	if (hr != S_OK) {
		os_message_box(str8("ID3D11Device1_CreateBuffer Error"), str8("Failed to create game quads"));
		os_exit_process(1);
	}

	D3D11_SHADER_RESOURCE_VIEW_DESC quad_srv_desc = { 0 };
	quad_srv_desc.Format = DXGI_FORMAT_UNKNOWN;
	quad_srv_desc.ViewDimension = D3D11_SRV_DIMENSION_BUFFER;
	quad_srv_desc.Buffer.NumElements = num_quads;

	hr = ID3D11Device1_CreateShaderResourceView(state->main_device, (ID3D11Resource *)state->game_quad_buffer,
											    &quad_srv_desc, &state->game_quad_srv);

	if (hr != S_OK) {
		os_message_box(str8("ID3D11Device1_CreateShaderResourceView Error"), str8("Failed to create game quad SRV"));
		os_exit_process(1);
	}

	// UI BUFFERS

	// CONSTANT BUFFERS
	D3D11_BUFFER_DESC constant_buffer_desc;
	constant_buffer_desc.ByteWidth = sizeof(D3D11_Constants);
	constant_buffer_desc.Usage = D3D11_USAGE_DYNAMIC;
	constant_buffer_desc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
	constant_buffer_desc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
	constant_buffer_desc.MiscFlags = 0;
	constant_buffer_desc.StructureByteStride = 0;
	
	hr = ID3D11Device1_CreateBuffer(state->main_device, &constant_buffer_desc, null, &state->main_constant_buffer);
	if (hr != S_OK) {
		os_message_box(str8("ID3D11Device1_CreateBuffer Error"), str8("Failed to create common constants constant buffer"));
		os_exit_process(1);
	}

	constant_buffer_desc.ByteWidth = sizeof(R2D_LightConstants);
	hr = ID3D11Device1_CreateBuffer(state->main_device, &constant_buffer_desc, null, &state->game_light_constant_buffer);
	if (hr != S_OK) {
		os_message_box(str8("ID3D11Device1_CreateBuffer Error"), str8("Failed to create light constant buffer"));
		os_exit_process(1);
	}
}

inl void
d3d11_initialize(R2D_Buffer *buffer, OS_Window *window, u32 quads_to_render) {
    if (!buffer->reserved) {
        Memory_Arena *d3d11_arena = arena_reserve(mb(16));
        buffer->reserved = arena_push(d3d11_arena, sizeof(D3D11_State));
        D3D11_State *d3d11_state = (D3D11_State *)buffer->reserved;
        d3d11_state->associated_window = window;
        d3d11_state->arena = d3d11_arena;
        
        d3d11_create_devices(d3d11_state);
        
        d3d11_enable_debug(d3d11_state);
        
        d3d11_initialize_swapchain(d3d11_state, window);
        d3d11_create_quad_shaders(d3d11_state);
        d3d11_setup_rasterizer(d3d11_state);
        d3d11_initialize_buffers(buffer, quads_to_render);
    }
}

inl b32
r2d_initialize(R2D_Buffer *buffer, OS_Window *window) {
    clear_struct(buffer);
    u32 quads_to_render = 4096 * 2;
    if (!buffer->reserved) {
        d3d11_initialize(buffer, window, quads_to_render);
        D3D11_State *state = (D3D11_State *)buffer->reserved;
        
        buffer->game_quads.quad_count = 0;
        buffer->game_quads.quad_capacity = quads_to_render;
        buffer->game_quads.quads = arena_push_array(state->arena, R2D_Quad, quads_to_render);
        
        buffer->ui_quads.quad_count = 0;
        buffer->ui_quads.quad_capacity = quads_to_render / 2;
        buffer->ui_quads.quads = arena_push_array(state->arena, R2D_Quad, buffer->ui_quads.quad_capacity);
        
        for (u32 t = 0; t < array_count(buffer->textures); ++t) {
            buffer->textures[t] = null;
        }
        
        buffer->free_texture_flag = 0xFF;
        
        FT_Library ft_library;
        FT_Face ft_face;
        u32 face_size = 8;
        FT_Error ft_error_code = FT_Init_FreeType(&ft_library);
        if (ft_error_code == FT_Err_Ok) {
            ft_error_code = FT_New_Face(ft_library, "..\\data\\assets\\fonts\\dos437.ttf", 0, &ft_face);
            if (ft_error_code == FT_Err_Ok) {
                Memory_Arena *font_parse_scratch = arena_get_scratch(null, 0);
                Temporary_Memory font_parse_mem = temp_mem_begin(font_parse_scratch);
                u32 bytes_per_pixel = 4;
                u32 atlas_width = 512;
                u32 atlas_height = 512;
                u32 atlas_pitch = atlas_width * bytes_per_pixel;
                
                u32 glyph_atlas_gap = 4;
                u32 glyph_atlas_col = glyph_atlas_gap;
                u32 glyph_atlas_row = glyph_atlas_gap;
                u8 *glyph_atlas = (u8 *)(arena_push(font_parse_scratch, bytes_per_pixel * atlas_width * atlas_height));
                
                buffer->font.ascent = ft_face->ascender >> 6;
                buffer->font.descent = ft_face->descender >> 6;
                s32 line_space = face_size + (buffer->font.ascent - buffer->font.descent);
                
                ft_error_code = FT_Set_Pixel_Sizes(ft_face, 0, face_size);
                if (ft_error_code == FT_Err_Ok) {
                    for (u32 codepoint = 0; codepoint < 128; codepoint += 1) {
                        u32 glyph_index = FT_Get_Char_Index(ft_face, codepoint);
                        
                        ft_error_code = FT_Load_Glyph(ft_face, glyph_index, FT_LOAD_DEFAULT);
                        if (ft_error_code == FT_Err_Ok) {
                            ft_error_code = FT_Render_Glyph(ft_face->glyph, FT_RENDER_MODE_NORMAL);
                            if (ft_error_code == FT_Err_Ok) {
                                FT_Bitmap *bitmap = &(ft_face->glyph->bitmap);
                                FT_Glyph_Metrics *metrics = &(ft_face->glyph->metrics);
                                
                                if ((glyph_atlas_col + bitmap->width + glyph_atlas_gap) >= atlas_width) {
                                    glyph_atlas_col = glyph_atlas_gap;
                                    glyph_atlas_row += line_space + glyph_atlas_gap;
                                    assert_true(glyph_atlas_row < atlas_height);
                                }
                                
                                u8 *source_pixels = bitmap->buffer;
                                u8 *dest_pixels = glyph_atlas + bytes_per_pixel * (glyph_atlas_row * atlas_width + glyph_atlas_col);
                                
                                for (u32 row = 0; row < bitmap->rows; row += 1) {
                                    u8 *source_row = source_pixels;
                                    u8 *dest_row = dest_pixels;
                                    for (u32 col = 0; col < bitmap->width; col += 1) {
                                        u8 color = *source_row++;
                                        *dest_row++ = color;
                                        *dest_row++ = color;
                                        *dest_row++ = color;
                                        *dest_row++ = color;
                                    }
                                    
                                    source_pixels += bitmap->pitch;
                                    dest_pixels += atlas_pitch;
                                }
                                
                                R2D_GlyphInfo *glyph = buffer->font.glyphs + codepoint;
                                glyph->x_p_in_atlas = glyph_atlas_col;
                                glyph->y_p_in_atlas = glyph_atlas_row;
                                glyph->advance = (f32)(metrics->horiAdvance >> 6);
                                glyph->bearing_x = (f32)(metrics->horiBearingX >> 6);
                                glyph->bearing_y = (f32)(metrics->horiBearingY >> 6);
                                glyph->width = (f32)(metrics->width >> 6);
                                glyph->height = (f32)(metrics->height >> 6);
                                
                                if (glyph->height > buffer->font.max_glyph_height) {
                                    buffer->font.max_glyph_height = glyph->height;
                                }
                                
                                glyph_atlas_col += bitmap->width + glyph_atlas_gap;
                            }
                        }
                    }
                }
                
                buffer->font.atlas = r2d_alloc_texture(buffer, atlas_width, atlas_height, glyph_atlas);
                FT_Done_Face(ft_face);
                temp_mem_end(font_parse_mem);
            } else if (ft_error_code == FT_Err_Unknown_File_Format) {
                fprintf(stderr, "FT_New_Face: Unsupported font format.\n");
            } else {
                fprintf(stderr, "FT_New_Face: Couldn't open font file.\n");
            }
            
            FT_Done_FreeType(ft_library);
        }
    }
    
    return true;
}

inl R2D_Handle
r2d_alloc_texture(R2D_Buffer *buffer, s32 width, s32 height, void *data) {
    R2D_Handle result = r2d_bad_handle();
    
    D3D11_State *d3d11_state = (D3D11_State *)buffer->reserved;
    unsigned long enabled_p = 0;
    if (_BitScanForward(&enabled_p, (unsigned long)buffer->free_texture_flag) == 1) {
        if (enabled_p < array_count(buffer->textures)) {
            buffer->free_texture_flag &= ~(1 << enabled_p);
            
            D3D11_Texture *texture = arena_push_struct(d3d11_state->arena, D3D11_Texture);
            D3D11_TEXTURE2D_DESC texture_desc = {
                .Width = (u32)(width),
                .Height = (u32)(height),
                .MipLevels = 1,
                .ArraySize = 1, 
                .Format = DXGI_FORMAT_R8G8B8A8_UNORM,
                .SampleDesc = { 1, 0 },
                //.Usage = D3D11_USAGE_DEFAULT,
                .Usage = D3D11_USAGE_IMMUTABLE,
                //.BindFlags = D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_RENDER_TARGET,
                .BindFlags = D3D11_BIND_SHADER_RESOURCE,
                .CPUAccessFlags = 0,
                //.MiscFlags = D3D11_RESOURCE_MISC_GENERATE_MIPS,
                .MiscFlags = 0,
            };
            
            D3D11_SUBRESOURCE_DATA texture_data = {
                data, (u32)(width * 4), 0
            };
            
            texture->width = (u32)width;
            texture->height = (u32)height;
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

// TODO(christian): nasty nasty! getting texture dims shouldnt depend on R2D_Buffer! Find a better way!
inl b32
r2d_get_texture_dims(R2D_Handle tex, s32 *width, s32 *height) {
    if ((tex.h64[0] >= 1) && (tex.h64[0] <= 4)) {
        D3D11_Texture *texture = (D3D11_Texture *)(tex.h64[1]);
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

inl R2D_Handle
r2d_texture_from_file(R2D_Buffer *buffer, String_U8_Const filename) {
    R2D_Handle result = r2d_bad_handle();
    s32 image_width = 0;
    s32 image_height = 0;
    s32 image_num_comps = 0;
    u8 *image_data = stbi_load((const char *)(filename.string), 
                               &image_width, &image_height, &image_num_comps, 0);
    
    if (image_data) {
        result = r2d_alloc_texture(buffer, image_width, image_height, image_data);
        stbi_image_free(image_data);
    }
    
    return (result);
}

inl void
r2d_set_viewport_dims(R2D_Buffer *buffer, u32 width, u32 height) {
    D3D11_State *d3d11_state = (D3D11_State *)buffer->reserved;
    
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
r2d_get_viewport_dims(R2D_Buffer *buffer, u32 *width, u32 *height) {
    D3D11_State *d3d11_state = (D3D11_State *)buffer->reserved;
    if (width) {
        *width = (u32)d3d11_state->viewport.Width;
    }
    
    if (height) {
        *height = (u32)d3d11_state->viewport.Height;
    }
}

// TODO(christian): should I use different shaders for UI and game?
inl b32
r2d_upload_to_gpu(R2D_Buffer *buffer, b32 vsync, f32 clear_r, f32 clear_g, f32 clear_b, f32 clear_a) {
    D3D11_State *state = (D3D11_State *)buffer->reserved;
    
    OS_Window *window = state->associated_window;
    assert_true(window != null);
    
	if (window->resized_this_frame) {
		assert_true(state->back_buffer != null);
		assert_true(state->render_target_view != null);
		ID3D11Texture2D_Release(state->back_buffer);
		ID3D11RenderTargetView_Release(state->render_target_view);
		
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

		r2d_set_viewport_dims(buffer, window->client_width, window->client_height);
	}

    D3D11_VIEWPORT viewport = state->viewport;
    
    D3D11_MAPPED_SUBRESOURCE mapped_subresource;
    if (ID3D11DeviceContext_Map(state->base_device_context, (ID3D11Resource *)state->main_constant_buffer, 0,
                                D3D11_MAP_WRITE_DISCARD, 0, &mapped_subresource) == S_OK) {
        m44 ortho = m44_make_ortho_lh_z01(0.0f, viewport.Width, 0.0f, viewport.Height, 0.0f, 1.0f);
        *(m44 *)mapped_subresource.pData = ortho;
        
        ID3D11DeviceContext_Unmap(state->base_device_context, (ID3D11Resource *)state->main_constant_buffer, 0);
    }
    
    if (ID3D11DeviceContext_Map(state->base_device_context, (ID3D11Resource *)state->game_light_constant_buffer, 0,
                                D3D11_MAP_WRITE_DISCARD, 0, &mapped_subresource) == S_OK) {
        copy_memory(mapped_subresource.pData, &buffer->light_constants, sizeof(buffer->light_constants));
        ID3D11DeviceContext_Unmap(state->base_device_context, (ID3D11Resource *)state->game_light_constant_buffer, 0);
    }
    
    if (ID3D11DeviceContext_Map(state->base_device_context, (ID3D11Resource *)state->game_quad_buffer, 0,
                                D3D11_MAP_WRITE_DISCARD, 0, &mapped_subresource) == S_OK) {
        copy_memory(mapped_subresource.pData, buffer->game_quads.quads, buffer->game_quads.quad_count * sizeof(R2D_Quad));
        ID3D11DeviceContext_Unmap(state->base_device_context, (ID3D11Resource *)state->game_quad_buffer, 0);
    }
    
    f32 clear_colour[] = { clear_r * clear_r, clear_g * clear_g, clear_b * clear_b, clear_a };
    ID3D11DeviceContext_ClearRenderTargetView(state->base_device_context, state->render_target_view, clear_colour);
    
    ID3D11DeviceContext_IASetPrimitiveTopology(state->base_device_context, D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);
    
    ID3D11DeviceContext_VSSetShaderResources(state->base_device_context, 0, 1, &state->game_quad_srv);
    ID3D11DeviceContext_VSSetConstantBuffers(state->base_device_context, 0, 1, &state->main_constant_buffer);
    ID3D11DeviceContext_VSSetShader(state->base_device_context, state->quad_vertex_shader, null, 0);
    
    for (u32 texture_index = 0; texture_index < array_count(buffer->textures); ++texture_index) {
        if ((buffer->free_texture_flag & (1 << texture_index)) == 0) {
            D3D11_Texture *texture = (D3D11_Texture *)buffer->textures[texture_index];
            ID3D11DeviceContext_PSSetShaderResources(state->base_device_context, texture_index + 1, 1, &texture->srv);
        }
    }
    
    ID3D11DeviceContext_PSSetSamplers(state->base_device_context, 0, 1, &state->quad_texture_sampler_state);
    ID3D11DeviceContext_PSSetConstantBuffers(state->base_device_context, 1, 1, &state->game_light_constant_buffer);
    ID3D11DeviceContext_PSSetShader(state->base_device_context, state->quad_pixel_shader, null, 0);
    
    ID3D11DeviceContext_RSSetState(state->base_device_context, (ID3D11RasterizerState *)state->rasterizer_state);
    
    ID3D11DeviceContext_RSSetViewports(state->base_device_context, 1, &viewport);
    
    ID3D11DeviceContext_OMSetBlendState(state->base_device_context, (ID3D11BlendState *)state->quad_blend_state, null, 0xffffffff);
    ID3D11DeviceContext_OMSetRenderTargets(state->base_device_context, 1, &state->render_target_view, null);
    
    ID3D11DeviceContext_DrawInstanced(state->base_device_context, 4, (UINT)buffer->game_quads.quad_count, 0, 0);
    
    // ui
    if (ID3D11DeviceContext_Map(state->base_device_context, (ID3D11Resource *)state->game_quad_buffer, 0,
                                D3D11_MAP_WRITE_DISCARD, 0, &mapped_subresource) == S_OK) {
        copy_memory(mapped_subresource.pData, buffer->ui_quads.quads, buffer->ui_quads.quad_count * sizeof(R2D_Quad));
        ID3D11DeviceContext_Unmap(state->base_device_context, (ID3D11Resource *)state->game_quad_buffer, 0);
    }
    
    if (ID3D11DeviceContext_Map(state->base_device_context, (ID3D11Resource *)state->game_light_constant_buffer, 0,
                                D3D11_MAP_WRITE_DISCARD, 0, &mapped_subresource) == S_OK) {
        R2D_LightConstants light;
        light.enable_lights[0] = 0;
        copy_memory(mapped_subresource.pData, &light, sizeof(R2D_LightConstants));
        ID3D11DeviceContext_Unmap(state->base_device_context, (ID3D11Resource *)state->game_light_constant_buffer, 0);
    }
    
    ID3D11DeviceContext_DrawInstanced(state->base_device_context, 4, (UINT)buffer->ui_quads.quad_count, 0, 0);
    
    buffer->game_quads.quad_count = 0;
    buffer->ui_quads.quad_count = 0;
    //clear_struct(&buffer->light_constants);
    b32 result = IDXGISwapChain1_Present(state->dxgi_swap_chain, vsync == true, 0) == S_OK;
    return(result);
}

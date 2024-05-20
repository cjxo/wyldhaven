/* date = February 2nd 2024 11:23 pm */

#ifndef WYLD_RENDER_H
#define WYLD_RENDER_H

typedef union {
    u64 h64[2];
} R2D_Handle;

typedef u32 Light_Type;
enum {
    LightType_Direction,
    LightType_Point,
};

typedef u32 Light_Info;
enum {
    LightInfo_EnableLightingForMe = 0x1,
    LightInfo_IsNotAffectedByLightIndex = 0x2,
    LightInfo_LightIndexNotAffectedMask = 0x3C,
};

#define max_lights 8

typedef struct {
    v2f origin;
    v2f x_axis;
    v2f y_axis;
    v4f corner_colours[4];
    f32 corner_roundness;
    f32 side_thickness;
    v2f uvs[4];
    u32 texture_id; // 0 if no texture, > 0 if yes texture
    u32 lighting_info;
} R2D_Quad;

typedef struct {
    u32 x_p_in_atlas;
    u32 y_p_in_atlas;
    f32 advance;
    f32 bearing_x, bearing_y;
    f32 width, height;
} R2D_GlyphInfo;

typedef struct {
    R2D_GlyphInfo glyphs[128];
    R2D_Handle atlas;
    s32 ascent, descent;
    f32 max_glyph_height;
} R2D_FontParsed;

typedef struct {
    v4f p;
    // ----- 16 byte ----- //
    v4f colour;
    // ----- 16 byte ----- //
    f32 constant_attenuation;
    f32 linear_attenuation;
    f32 _unused_a[2];
    // ----- 16 byte ----- //
    Light_Type type; // 4 bytes
    s32 is_enabled; // 4 bytes
    f32 _unused_b[2]; // 8 bytes
    // ----- 16 byte ----- //
} Light;

typedef struct {
    v4f ambient_colour; // ambient colour for dungeon / overworld / whatever
    Light lights[max_lights];
    s32 enable_lights[4];
} R2D_LightConstants;

typedef struct {
    u64 quad_count;
    u64 quad_capacity;
    R2D_Quad *quads;
} R2D_QuadArray;

typedef u8 Font_Size;
enum {
    FontSize_Small,
    FontSize_Medium,
    FontSize_Large,
    FontSize_Count,
};

typedef struct {
    void *textures[4]; // allow only four textures
    u8 free_texture_flag;
    R2D_FontParsed font[FontSize_Count];
    R2D_QuadArray game_quads;
    R2D_QuadArray ui_quads;
    R2D_LightConstants light_constants;
    
    void *reserved;
} R2D_Buffer;

//~ NOTE(christian): handle
inl R2D_Handle r2d_bad_handle(void);
inl b32 r2d_handles_match(R2D_Handle a, R2D_Handle b);
inl void r2d_set_viewport_dims(R2D_Buffer *buffer, u32 width, u32 height);
inl void r2d_get_viewport_dims(R2D_Buffer *buffer, u32 *width, u32 *height);

//~ NOTE(christian): init
inl b32 r2d_initialize(R2D_Buffer *buffer, OS_Window *window);

//~ NOTE(christian): tex: all point sampled, DXGI_FORMAT_R8G8B8A8_UNORM, immytable. fow now.
inl R2D_Handle r2d_alloc_texture(R2D_Buffer *buffer, s32 width, s32 height, void *data);
inl b32 r2d_get_texture_dims(R2D_Handle tex, s32 *width, s32 *height);
inl v2f r2d_get_texture_dims_v(R2D_Handle tex);
inl R2D_Handle r2d_texture_from_file(R2D_Buffer *buffer, String_U8_Const filename);

//~ NOTE(christian): draw
inl void r2d_begin_render(R2D_Buffer *buffer);
inl void r2d_end_render(R2D_Buffer *buffer);

inl R2D_Quad *r2d_acquire_quad(R2D_QuadArray *quad_array);
inl R2D_Quad *r2d_rect_filled(R2D_QuadArray *quad_array, v2f p, v2f dims, v4f colour, f32 corner_roundness);
inl R2D_Quad *r2d_rect_outline(R2D_QuadArray *quad_array, v2f p, v2f dims, v4f colour, f32 corner_roundness, f32 side_thickness);
inl R2D_Quad *r2d_line(R2D_QuadArray *quad_array, v2f v0, v2f v1, v4f colour);
inl R2D_Quad *r2d_texture_clipped(R2D_QuadArray *quad_array, R2D_Handle texture_handle, v2f p, v2f dims, v2f clip_p, v2f clip_dims, v4f mod);
inl R2D_Quad *r2d_texture(R2D_QuadArray *quad_array, R2D_Handle texture_handle, v2f p, v2f dims, v4f mod);
fun void r2d_text(R2D_QuadArray *quad_array, R2D_FontParsed *font, v2f p, v4f colour, String_U8_Const text);
inl v2f r2d_get_text_dims(R2D_FontParsed *font, String_U8_Const string);

//~ NOTE(christian): send
inl b32 r2d_upload_to_gpu(R2D_Buffer *buffer, b32 vsync, f32 clear_r, f32 clear_g, f32 clear_b, f32 clear_a);

#endif //WYLD_RENDER_H

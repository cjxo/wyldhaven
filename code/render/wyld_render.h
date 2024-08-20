/* date = February 2nd 2024 11:23 pm */

#ifndef WYLD_RENDER_H
#define WYLD_RENDER_H

typedef union {
  u64 h64[2];
} R_Handle;

typedef u8 R_Font_Size;
enum {
  R_FontSize_Small,
  R_FontSize_Medium,
  R_FontSize_Large,
  R_FontSize_Count,
};

typedef struct {
  u32 x_p_in_atlas;
  u32 y_p_in_atlas;
  f32 advance;
  f32 bearing_x, bearing_y;
  f32 width, height;
} R2D_GlyphInfo;

typedef struct {
  R2D_GlyphInfo glyphs[128];
  R_Handle atlas;
  s32 ascent, descent;
  f32 max_glyph_height;
} R2D_FontParsed;

//~ NOTE(christian): new api area
typedef struct {
  v2f p;
  v2f p_unbiased;
  v2f dims;
  v2f dims_unbiased;
  v4f vertex_colours[4];
  f32 vertex_roundness;
  f32 side_thickness;
  f32 side_smoothness;
  
  v2f shadow_offset;
  v2f shadow_dim_offset;
  v4f shadow_colours[4];
  f32 shadow_roundness;
  f32 shadow_smoothness;
  
  v2f uvs[4];
  u32 texture_id; // 0 if no texture, > 0 if yes texture
} R_UI_Rect;

typedef struct {
  v2f p;
  v2f dims;
  v4f vertex_colours[4];
  v2f uvs[4];
  u32 texture_id; // 0 if no texture, > 0 if yes texture
} R_O2D_Rect;

typedef u32 R_RenderPass_GameMesh_Type;
enum {
  R_RenderPass_GameMesh_Plane,
  R_RenderPass_GameMesh_Count,
};
typedef struct {
  v3f p;
  v3f scale;
  v3f rotate;
} R3D_MeshInstance;

typedef u16 R_Light_Type;
enum {
  R_LightType_Point,
  R_LightType_Direction,
  R_LightType_Spotlight,
  R_LightType_Count
};

typedef struct {
  R_Light_Type type;
} R3D_Light;

#define r_max_lights 8
typedef struct {
  v4f ambient_colour; // ambient colour for dungeon / overworld / whatever
  R3D_Light lights[r_max_lights];
} R3D_LightConstants;

typedef struct {
  // TODO: we even need roundness here in this rect?
  R_O2D_Rect rects[512];
  u64 rect_count;
} R_RenderPass_Game_Ortho2D;

typedef struct {
  //R_RenderPass_GameMesh_Type mesh_type;
  R3D_LightConstants light_constants;
  R3D_MeshInstance instances[512];
} R_RenderPass_Game_Ortho3D;

typedef struct {
  R_UI_Rect rects[512];
  u64 rect_count;
} R_RenderPass_UI;

typedef u32 R_RenderPassType;
enum {
  R_RenderPassType_Game_Ortho2D,
  R_RenderPassType_Game_Ortho3D,
  R_RenderPassType_UI,
  R_RenderPassType_ScreenBlur,
  R_RenderPassType_Count,
};

typedef struct R_RenderPass R_RenderPass;
struct R_RenderPass {
  R_RenderPassType type;
  
  union {
    R_RenderPass_Game_Ortho2D game_ortho_2D;
    R_RenderPass_Game_Ortho3D game_ortho_3D;
    R_RenderPass_UI ui;
  };
  
  R_RenderPass *next;
};

typedef struct {
  void *textures[4]; // allow only four textures
  u8 free_texture_flag;
  R2D_FontParsed font[R_FontSize_Count];
  
  Memory_Arena *arena;
  R_RenderPass *free_passes;
  R_RenderPass *first_pass;
  R_RenderPass *last_pass;
  void *reserved;
} R_Buffer;

inl R_Handle r_handle_make_bad(void);
inl b32 r_handle_match(R_Handle a, R_Handle b);
inl b32 r_handle_is_bad(R_Handle a);

fun R_Buffer *r_buffer_create(OS_Window *window);

fun b32 r_texture_get_dims(R_Handle handle, s32 *width, s32 *height);
inl v2f r_texture_get_dims_v(R_Handle handle);
fun R_Handle r_texture_alloc(R_Buffer *buffer, s32 width, s32 height, void *data);

fun v2f r_text_get_dims(R2D_FontParsed *font, String_U8_Const string);

// ORTHO2D
fun R_O2D_Rect *r_O2D_acquire_rect(R_RenderPass *pass);
inl R_O2D_Rect *r_O2D_rect_filled(R_RenderPass *pass, v2f p, v2f dims, v4f colour);
fun R_O2D_Rect *r_O2D_texture_clipped(R_RenderPass *pass, R_Handle texture_handle, v2f p, v2f dims, v2f clip_p, v2f clip_dims, v4f mod);
fun R_O2D_Rect *r_O2D_texture(R_RenderPass *pass, R_Handle texture_handle, v2f p, v2f dims, v4f mod);

fun R_RenderPass *r_begin_pass(R_Buffer *buffer, R_RenderPassType type);
fun void r_submit_passes_to_gpu(R_Buffer *buffer, b32 vsync, f32 clear_r, f32 clear_g, f32 clear_b, f32 clear_a);

#endif //WYLD_RENDER_H
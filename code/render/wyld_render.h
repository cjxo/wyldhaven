/* date = February 2nd 2024 11:23 pm */

#ifndef WYLD_RENDER_H
#define WYLD_RENDER_H

#define r_max_lights 8

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
  v4f colour;
  v2f p;
  f32 radius;
  f32 _unused_a;
} R_O2D_Light;

typedef struct {
  v2f p;
  v2f dims;
  v4f vertex_colours[4];
  v2f uvs[4];
  u32 texture_id; // 0 if no texture, > 0 if yes texture
  u32 enable_lighting_for_me;
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

typedef struct {
  v4f ambient_colour; // ambient colour for dungeon / overworld / whatever
  R3D_Light lights[r_max_lights];
} R3D_LightConstants;

typedef struct {
  R_O2D_Rect rects[512];
  u64 rect_count;
  
  R_O2D_Light lights[r_max_lights];
  u64 light_count;
  u32 enable_lighting;
} R_RenderPass_Game_Ortho2D;

typedef struct {
  //R_RenderPass_GameMesh_Type mesh_type;
  R3D_LightConstants light_constants;
  R3D_MeshInstance instances[512];
} R_RenderPass_Game_Ortho3D;

typedef struct {
  R_UI_Rect rects[1024];
  u64 rect_count;
} R_RenderPass_UI;

typedef u32 R_CurveType;
enum {
  R_CurveType_Hermite,
  R_CurveType_Bezier,
};

typedef struct {
  R_CurveType type;
  union {
    Hermite_Curve hermite;
    Bezier_Curve bezier;
  };
  v2f *points;
  u64 capacity;
} R_Curve;

typedef struct {
  R_Curve *curves;
  u64 curve_capacity;
  u64 curve_count;
} R_RenderPass_TestScene_Curve;

typedef u32 R_RenderPassType;
enum {
  R_RenderPassType_Game_Ortho2D,
  R_RenderPassType_Game_Ortho3D,
  R_RenderPassType_UI,
  R_RenderPassType_ScreenBlur,
  R_RenderPassType_TestScene_Curve,
  R_RenderPassType_Count,
};

typedef struct R_RenderPass R_RenderPass;
struct R_RenderPass {
  R_RenderPassType type;
  
  union {
    R_RenderPass_Game_Ortho2D game_ortho_2D;
    R_RenderPass_Game_Ortho3D game_ortho_3D;
    R_RenderPass_UI ui;
    R_RenderPass_TestScene_Curve curve;
  };
  
  R_RenderPass *next;
};

typedef struct {
  void *textures[4]; // allow only four textures
  u8 free_texture_flag;
  R2D_FontParsed font[R_FontSize_Count];
  
  Memory_Arena *render_arena;
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
fun R_O2D_Light *r_O2D_light_add(R_RenderPass *pass, v2f p, f32 radius, v4f colour);

//~ Test Scene Curve
// NOTE(Christian):
// If Hermite, then, P_1 = Start Point, P_2 = end point, P_3 = tangent vector of start, P_4 = tangent vector of end.
fun R_Curve *r_curve_add(R_RenderPass *curve_pass, R_CurveType type, v2f P_1, v2f P_2, v2f P_3, v2f P_4);
//fun void r_curve_add_hermite(R_RenderPass *curve_pass, v2f begin, v2f end, v2f begin_tangent, v2f end_tangent);
// TODO(Christian): should this be internal to D3d11/ogl?
//fun void r_curve_generate_points(R_RenderPass *curve_pass);

fun R_RenderPass *r_begin_pass(R_Buffer *buffer, R_RenderPassType type);
fun R_RenderPass *r_begin_pass_curve(R_Buffer *buffer, u64 curve_count, u64 step_count);
fun void r_submit_passes_to_gpu(R_Buffer *buffer, b32 vsync, f32 clear_r, f32 clear_g, f32 clear_b, f32 clear_a);

#endif //WYLD_RENDER_H
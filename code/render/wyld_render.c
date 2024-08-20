//#include "impl/wyld_render_d3d11.c"
#include "impl/wyld_render_d3d11_2.c"

inl R_Handle
r_handle_make_bad(void) {
  R_Handle result;
  result.h64[0] = 0xFFFFFFFFFFFFFFFF;
  result.h64[1] = 0xFFFFFFFFFFFFFFFF;
  return(result);
}

inl b32
r_handle_match(R_Handle a, R_Handle b) {
  b32 result = (a.h64[0] == b.h64[0]) && (a.h64[1] == b.h64[1]);
  return(result);
}

inl b32
r_handle_is_bad(R_Handle a) {
  b32 result = r_handle_match(a, r_handle_make_bad());
  return(result);
}

#if 0
inl R2D_Quad *
r2d_line(R2D_QuadArray *quad_array, v2f v0, v2f v1, v4f colour) {
  v2f direction = v2f_sub(v1, v0);
  
  R2D_Quad *quad = r2d_acquire_quad(quad_array);
  quad->origin = v0;
  quad->y_axis = direction;
  v2f_normalize(&direction);
  quad->x_axis = v2f_make(-direction.y, direction.x);
  quad->corner_colours[0] = quad->corner_colours[1] = 
    quad->corner_colours[2] = quad->corner_colours[3] = colour;
  
  quad->corner_roundness = 1.0f;
  return(quad);
}
#endif

//~ NOTE(christian): new api area
fun R_RenderPass *
r_begin_pass(R_Buffer *buffer, R_RenderPassType type) {
  R_RenderPass *result = null;
  if (buffer->free_passes) {
    result = buffer->free_passes;
    buffer->free_passes = buffer->free_passes->next;
  } else {
    result = arena_push_struct(buffer->arena, R_RenderPass);
  }
  
  clear_memory(result, sizeof(R_RenderPass));
  result->type = type;
  sll_push_back(result, buffer->first_pass, buffer->last_pass);
  
  return(result);
}

inl void
r_uvs_from_clip(v2f clip_p, v2f clip_dims, R_Handle texture_handle, v2f uvs[4]) {
  s32 width, height;
  r_texture_get_dims(texture_handle, &width, &height);
  
  v2f begin = v2f_make(clip_p.x / (f32)width, clip_p.y / (f32)height);
  v2f end = v2f_make((clip_p.x + clip_dims.x) / (f32)width, (clip_p.y + clip_dims.y) / (f32)height);
  
  uvs[0] = begin;
  uvs[1] = v2f_make(end.x, begin.y);
  uvs[2] = v2f_make(begin.x, end.y);
  uvs[3] = end;
}

fun v2f
r_text_get_dims(R2D_FontParsed *font, String_U8_Const string) {
  v2f final_dim = { 0 };
  for (u32 char_index = 0; char_index < string.char_count; char_index += 1) {
    R2D_GlyphInfo *info = font->glyphs + string.string[char_index];
    
    if ((string.string[char_index] != '\0')) {
      final_dim.x += info->advance;
    }
    
    if (info->height > final_dim.y) {
      final_dim.y = info->height;
    }
  }
  
  return(final_dim);
}

inl v2f
r_texture_get_dims_v(R_Handle handle) {
  v2f result;
  s32 width, height;
  r_texture_get_dims(handle, &width, &height);
  
  result.x = (f32)width;
  result.y = (f32)height;
  return(result);
}

// ORTHO2D
fun R_O2D_Rect *
r_O2D_acquire_rect(R_RenderPass *pass) {
  assert_true(pass->type == R_RenderPassType_Game_Ortho2D);
  
  assert_true(pass->game_ortho_2D.rect_count < array_count(pass->game_ortho_2D.rects));
  R_O2D_Rect *rect = pass->game_ortho_2D.rects + pass->game_ortho_2D.rect_count++;
  clear_memory(rect, sizeof(R_O2D_Rect));
  return(rect);
}

inl R_O2D_Rect *
r_O2D_rect_filled(R_RenderPass *pass, v2f p, v2f dims, v4f colour) {
  R_O2D_Rect *rect = r_O2D_acquire_rect(pass);
  rect->p = p;
  rect->dims = dims;
  rect->vertex_colours[0] = rect->vertex_colours[1] = rect->vertex_colours[2] = rect->vertex_colours[3] = colour;
  
  return(rect);
}

fun R_O2D_Rect *
r_O2D_texture_clipped(R_RenderPass *pass, R_Handle texture_handle, v2f p, v2f dims,
                      v2f clip_p, v2f clip_dims, v4f mod) {
  R_O2D_Rect *rect = r_O2D_rect_filled(pass, p, dims, mod);
  r_uvs_from_clip(clip_p, clip_dims, texture_handle, rect->uvs);
  rect->texture_id = (u32)texture_handle.h64[0];
  
  return(rect);
}

fun R_O2D_Rect *
r_O2D_texture(R_RenderPass *pass, R_Handle texture_handle, v2f p, v2f dims, v4f mod) {
  R_O2D_Rect *rect = r_O2D_rect_filled(pass, p, dims, mod);
  
  rect->uvs[0] = v2f_make(0.0f, 0.0f);
  rect->uvs[1] = v2f_make(1.0f, 0.0f);
  rect->uvs[2] = v2f_make(0.0f, 1.0f);
  rect->uvs[3] = v2f_make(1.0f, 1.0f);
  
  rect->texture_id = (u32)texture_handle.h64[0];
  return(rect);
}
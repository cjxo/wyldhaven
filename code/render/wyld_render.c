#include "impl/wyld_render_d3d11.c"

inl R2D_Handle
r2d_bad_handle(void) {
  R2D_Handle handle;
  handle.h64[0] = 0xFFFFFFFFFFFFFFFF;
  handle.h64[1] = 0xFFFFFFFFFFFFFFFF;
  return(handle);
}

inl b32
r2d_handles_match(R2D_Handle a, R2D_Handle b) {
  b32 result = (a.h64[0] == b.h64[0]) && (a.h64[1] == b.h64[1]);
  return(result);
}

inl b32
r2d_handle_is_bad(R2D_Handle a) {
  b32 result = r2d_handles_match(a, r2d_bad_handle());
  return(result);
}

inl v2f
r2d_get_texture_dims_v(R2D_Handle tex) {
  s32 width, height;
  r2d_get_texture_dims(tex, &width, &height);
  
  v2f result;
  result.x = (f32)width;
  result.y = (f32)height;
  
  return(result);
}

inl R2D_Quad *
r2d_acquire_quad(R2D_QuadArray *quad_array) {
  assert_true(quad_array->quad_count < quad_array->quad_capacity);
  R2D_Quad *result = quad_array->quads + quad_array->quad_count++;
  
  // NOTE(christian): tells the shader that do not apply these things.
  // we only use these values for rect because idk how to make this work in 
  // paralelograms
  result->corner_roundness = 0.0f;
  result->side_thickness = -1.0f;
  
  // NOTE(christian): tells the shader that this quad doesn't use textures
  result->uvs[0] = result->uvs[1] = result->uvs[2] = result->uvs[3] = v2f_make(-1, -1);
  result->texture_id = 0;
  result->lighting_info = 0;
  return(result);
}

inl R2D_Quad *
r2d_rect_filled(R2D_QuadArray *quad_array, v2f p, v2f dims, v4f colour, f32 corner_roundness) {
  R2D_Quad *rect = r2d_acquire_quad(quad_array);
  rect->origin = p;
  rect->x_axis = v2f_make(dims.x, 0.0f);
  rect->y_axis = v2f_make(0.0f, dims.y);
  rect->corner_colours[0] = rect->corner_colours[1] = rect->corner_colours[2] = rect->corner_colours[3] = colour;
  rect->corner_roundness = corner_roundness;
  rect->side_thickness = 0.0f;
  
  return(rect);
}

inl R2D_Quad *
r2d_texture_clipped(R2D_QuadArray *quad_array, R2D_Handle texture_handle, v2f p, v2f dims, v2f clip_p, v2f clip_dims, v4f mod) {
  R2D_Quad *quad = r2d_rect_filled(quad_array, p, dims, mod, 0.0f);
  
  s32 width, height;
  r2d_get_texture_dims(texture_handle, &width, &height);
  
  f32 dim_bias = 0.0f;
  v2f dims_biased = v2f_make(clip_dims.x + dim_bias, clip_dims.y + dim_bias);
  v2f p_biased = v2f_sub(clip_p, v2f_make(dim_bias * 2.0f, dim_bias * 2.0f));;
  //v2f p_biased = clip_p;
  
  v2f begin = v2f_make(p_biased.x / (f32)width, p_biased.y / (f32)height);
  v2f end = v2f_make((p_biased.x + dims_biased.x) / (f32)width, (p_biased.y + dims_biased.y) / (f32)height);
  
  quad->uvs[0] = begin;
  quad->uvs[1] = v2f_make(end.x, begin.y);
  quad->uvs[2] = v2f_make(begin.x, end.y);
  quad->uvs[3] = end;
  
  quad->texture_id = (u32)texture_handle.h64[0];
  
  return(quad);
}

inl R2D_Quad *
r2d_texture(R2D_QuadArray *quad_array, R2D_Handle texture_handle, v2f p, v2f dims, v4f mod) {
  R2D_Quad *quad = r2d_rect_filled(quad_array, p, dims, mod, 0.0f);
  
  ExitProcess(0);
  quad->uvs[0] = v2f_make(0.0f, 0.0f);
  quad->uvs[1] = v2f_make(1.0f, 0.0f);
  quad->uvs[2] = v2f_make(0.0f, 1.0f);
  quad->uvs[3] = v2f_make(1.0f, 1.0f);
  
  quad->texture_id = (u32)texture_handle.h64[0];
  
  return(quad);
}

inl R2D_Quad *
r2d_rect_outline(R2D_QuadArray *quad_array, v2f p, v2f dims, v4f colour, f32 corner_roundness, f32 side_thickness) {
  R2D_Quad *rect = r2d_rect_filled(quad_array, p, dims, colour, corner_roundness);
  rect->side_thickness = side_thickness;
  return(rect);
}

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

// TODO(christian): a way to specify the size of text
fun void
r2d_text(R2D_QuadArray *quad_array, R2D_FontParsed *font, v2f p, v4f colour, String_U8_Const text) {
  u32 ascent_plus_descent = (font->ascent + font->descent);
  for (u32 char_index = 0; char_index < text.char_count; char_index += 1) {
    R2D_GlyphInfo *info = font->glyphs + text.string[char_index];
    
    if (text.string[char_index] != ' ') {
      v2f final_p = p;
      // am I doing this right???????
      final_p.x += info->bearing_x;
      final_p.y = final_p.y - (info->bearing_y - ascent_plus_descent);
      final_p.y -= 4.0f;
      
      r2d_texture_clipped(quad_array, font->atlas, final_p, v2f_make(info->width, info->height),
                          v2f_make((f32)info->x_p_in_atlas, (f32)info->y_p_in_atlas),
                          v2f_make(info->width, info->height), colour);
    }
    
    p.x += info->advance;
  }
}

inl v2f
r2d_get_text_dims(R2D_FontParsed *font, String_U8_Const string) {
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

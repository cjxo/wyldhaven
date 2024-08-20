//~ NOTE(christian): draw stuff
inl UI_RectColours
ui_rect_colours(v4f tl, v4f tr, v4f bl, v4f br) {
  UI_RectColours colours;
  colours.tl = tl;
  colours.tr = tr;
  colours.bl = bl;
  colours.br = br;
  return(colours);
}

inl UI_Anchor
ui_make_anchor(UI_MetricType metric, f32 value) {
  UI_Anchor result;
  result.metric = metric;
  result.value = value;
  return(result);
}

inl UI_Offset
ui_make_offset(UI_OffsetType type, UI_MetricType metric, f32 value) {
  UI_Offset result;
  result.type = type;
  result.metric = metric;
  result.value = value;
  return(result);
}

inl UI_ClippedTexture
ui_make_clipped_texture(R_Handle handle, v2f clip_p, v2f clip_dims) {
  UI_ClippedTexture result;
  result.handle = handle;
  result.clip_p = clip_p;
  result.clip_dims = clip_dims;
  return(result);
}

inl UI_ClippedTexture
ui_make_clipped_texture_null(void) {
  UI_ClippedTexture result;
  result.handle = r_handle_make_bad();
  return(result);
}

fun UI_DragAndDropHandle
ui_make_null_drag_and_drop_handle(void) {
  UI_DragAndDropHandle result = 0xFFFFFFFFFFFFFFFFllu;
  return(result);
}

fun UI_DragAndDropItem
ui_make_null_drag_and_drop_item(void) {
  UI_DragAndDropItem result;
  result.item_props = ui_make_null_drag_and_drop_handle();
  result.texture = ui_make_clipped_texture_null();
  return(result);
}

inl UI_DragAndDropItem
ui_make_drag_and_drop_item(UI_ClippedTexture texture, UI_DragAndDropFlags flags) {
  UI_DragAndDropItem result;
  result.texture = texture;
  result.item_props = flags;
  return(result);
}

inl b32
ui_clipped_texture_is_null(UI_ClippedTexture test) {
  b32 result = r_handle_match(test.handle, r_handle_make_bad());
  return(result);
}

inl UI_IndividualSize
ui_make_individual_size(UI_IndividualSizeType type, f32 value) {
  UI_IndividualSize result;
  result.type = type;
  result.value = value;
  return(result);
}

inl R_UI_Rect *
ui_acquire_rect(R_RenderPass_UI *ui_pass) {
  assert_true(ui_pass->rect_count < array_count(ui_pass->rects));
  R_UI_Rect *rect = ui_pass->rects + ui_pass->rect_count++;
  clear_memory(rect, sizeof(R_UI_Rect));
  return(rect);
}

#define ui_rect_no_shadow(state,p,dims,colours,vertex_roundness,side_thickness)\
ui_rect((state),(p),(dims),(colours),\
(vertex_roundness),(side_thickness),1.0f,\
v2f_make(0,0),v2f_make(0,0),ui_rect_colour(v4f_make(1,1,1,1)),\
0.0f,0.0f)

fun R_UI_Rect *
ui_rect(UI_State *state, v2f p, v2f dims, UI_RectColours colours,
        f32 vertex_roundness, f32 side_thickness, f32 side_smoothness,
        v2f shadow_offset, v2f shadow_dim_offset, UI_RectColours shadow_colours,
        f32 shadow_roundness, f32 shadow_smoothness) {
  R_RenderPass_UI *ui_pass = state->pass;
  R_UI_Rect *rect = ui_acquire_rect(ui_pass);
  
  f32 dim_bias = maximum(side_smoothness,
                         shadow_smoothness +
                         maximum(shadow_offset.x, shadow_offset.y)) * 2.0f;
  v2f dims_biased = v2f_add(dims, v2f_make(dim_bias, dim_bias));
  
  v2f p_biased = p;
  p_biased = v2f_sub(p_biased, v2f_make(dim_bias * 0.5f, dim_bias * 0.5f));
  
  rect->p = p_biased;
  rect->p_unbiased = p;
  rect->dims = dims_biased;
  rect->dims_unbiased = dims;
  rect->vertex_colours[0] = colours.tl;
  rect->vertex_colours[1] = colours.tr;
  rect->vertex_colours[2] = colours.bl;
  rect->vertex_colours[3] = colours.br;
  rect->vertex_roundness = vertex_roundness;
  rect->side_thickness = side_thickness;
  rect->side_smoothness = side_smoothness;
  
  rect->shadow_offset = shadow_offset;
  rect->shadow_dim_offset = shadow_dim_offset;
  rect->shadow_colours[0] = shadow_colours.tl;
  rect->shadow_colours[1] = shadow_colours.tr;
  rect->shadow_colours[2] = shadow_colours.bl;
  rect->shadow_colours[3] = shadow_colours.br;
  rect->shadow_roundness = shadow_roundness;
  rect->shadow_smoothness = shadow_smoothness;
  
  return(rect);
}

inl R_UI_Rect *
ui_texture_mod(UI_State *state, UI_ClippedTexture texture, v2f p, v2f dims, v4f mod, f32 vertex_roundness) {
  R_UI_Rect *rect = ui_rect(state, p, dims, ui_rect_colour(mod), vertex_roundness, 0.0f, 0.0f,
                            v2f_make(0, 0), v2f_make(0, 0), ui_rect_colour(mod),
                            0, 0);
  r_uvs_from_clip(texture.clip_p, texture.clip_dims, texture.handle, rect->uvs);
  rect->texture_id = (u32)texture.handle.h64[0];
	return(rect);
}

inl R_UI_Rect *
ui_texture(UI_State *state, UI_ClippedTexture texture, v2f p, v2f dims, f32 vertex_roundness) {
  R_UI_Rect *result = ui_texture_mod(state, texture, p, dims,
                                     v4f_make(1, 1, 1, 1),
                                     vertex_roundness);
  return(result);
}

fun void
ui_text(UI_State *state, R_Font_Size font_size, v2f p, v4f colour, String_U8_Const string) {
  assert_true(font_size < R_FontSize_Count);
  
  f32 max_height = 0.0f;
  for (u32 char_index = 0;
       char_index < string.char_count;
       ++char_index) {
    R2D_GlyphInfo *glyph = state->buffer->font[font_size].glyphs + string.string[char_index];
    if (glyph->height > max_height) {
      max_height = glyph->height;
    }
  }
  
  for (u32 char_index = 0;
       char_index < string.char_count;
       ++char_index) {
    R2D_GlyphInfo *glyph = state->buffer->font[font_size].glyphs + string.string[char_index];
    if (string.string[char_index] != ' ') {
      v2f final_p = p;
      final_p.x += glyph->bearing_x;
      final_p.y -= glyph->bearing_y;
      final_p.y += max_height;
      
      UI_ClippedTexture ct = ui_make_clipped_texture(state->buffer->font[font_size].atlas,
                                                     v2f_make((f32)glyph->x_p_in_atlas,
                                                              (f32)glyph->y_p_in_atlas),
                                                     v2f_make(glyph->width, glyph->height));
      ui_texture_mod(state, ct, final_p, v2f_make(glyph->width, glyph->height), colour, 0.0f);
    }
    
    p.x += glyph->advance;
  }
}

// TODO(christian): man ..  should i allocate a big block at the beginning instead of doing this?
inl void
ui_initialize(UI_State *state, OS_Input *input, R_Buffer *buffer) {
  clear_struct(state);
  state->widget_arena = arena_reserve(align_a_to_b(1024 * sizeof(UI_Widget), 4096));
  state->util_arena = arena_reserve(kb(32));
  state->buffer = buffer;
  state->input = input;
}

// NOTE(christian): Here is our hash rule for ui
// We can place triple pounds (###) in the identifier to tell
// this routine that we display only anything after the triple pound.
// The hash value is calculated for the entire string for triple pound and for none.
fun UI_Key
ui_hash_string(String_U8_Const identifier) {
  UI_Key result;
  
  result.key = str8_compute_hash(0, identifier);
  
  return(result);
}

fun String_U8_Const
ui_string_as_content(String_U8_Const string) {
  u64 pound_index = str8_find_first_string(string, str8("###"), 0);
  if (pound_index != invalid_index_u64) {
    string.string += pound_index + 3;
    string.char_count -= pound_index + 3;
  }
  
  //String_U8 result = StringU8_CopyString(ui_state->util_arena, string);
  return(string);
}

inl UI_Key
ui_zero_key(void) {
  UI_Key result;
  result.key = 0;
  return(result);
}

inl b32
ui_match_keys(UI_Key a, UI_Key b) {
  b32 result = a.key == b.key;
  return(result);
}

// NOTE(christian): thoughts on hashing strings
// When we input an identifier string, it could contain
// triple pounds (###). That indicates to this routine (and others)
// that only the characters AFTER the triple pounds are rendered / used for other stuff.
// But the entire string is used for hashing. This is done because we want
// to avoid duplicates.
// Knowing that, heres how we'll handle string hashing / searching through the cache.
// 1. We calculate the hash value for the entire identifier.
// 2. We extract the "content" of that string (if any).
// 3. We search through the cache to find if the requested identifier exists.
//    - We search through the list while
//       a. The result is not null,
//       b. The old unextracted string is not equal the result's identifier
// 
// Now if the result is not null, then it either is a duplicate or unique.
// It is a duplicate if result->last_frame_touched_index == ui_state->current_frame_index

fun u64
ui_hash_slot_from_key(UI_State *state, UI_Key key) {
  u64 result = key.key % array_count(state->cache);
  if (ui_match_keys(key, ui_zero_key())) {
    result += 1;
  }
  
  return(result);
}

fun u64
ui_hash_slot_from_drag_and_drop_handle(UI_State *state, UI_DragAndDropHandle handle) {
  UI_DragAndDropHandle key = handle;
  u64 hash_slot = key % array_count(state->drag_and_drop_associations);
  return(hash_slot);
}

// NOTE(christian): so, this function associates a widget with a drag-and-drop box state.
fun UI_DragAndDropGroupNode *
ui_add_drag_and_drop_group_node(UI_State *state, UI_DragAndDropGroupNode **first,
                                UI_Widget *widget, UI_DragAndDropItem item) {
  UI_DragAndDropGroupNode *node = *first;
  for (; node; node = node->next) {
    if (str8_equal_strings(widget->identifier_string, node->widget_identifier)) {
      break;
    }
  }
  
  if (!node) {
    UI_DragAndDropGroupNode *new_node = arena_push_struct(state->widget_arena, UI_DragAndDropGroupNode);
    new_node->widget_identifier = str8_copy(state->widget_arena, widget->identifier_string); // we need this to persist!
    new_node->item = item;
    new_node->next = *first;
    (*first) = new_node;
    
    widget->bound_texture = item.texture;
    return new_node;
  } else {
    widget->bound_texture = node->item.texture;
    
    return node;
  }
}

// NOTE(christian): so this function finds an appropriate hash slot for the new item.
// Thus, all items in the "result" variables should match the handle.
fun void
ui_associate_drag_and_drop_tex(UI_State *state, UI_Widget *widget, UI_DragAndDropItem item) {
  assert_true((widget->flags & UIWidget_DragAndDrop) != 0);
  u64 hash_slot = ui_hash_slot_from_drag_and_drop_handle(state, widget->d_and_d_handle);
  UI_DragAndDropBoxNode *result = state->drag_and_drop_associations[hash_slot];
  
  while (result && (widget->d_and_d_handle != result->handle)) {
    result = result->next_in_hash;
  }
  
  if (!result) {
    result = arena_push_struct(state->widget_arena, UI_DragAndDropBoxNode);
    result->handle = widget->d_and_d_handle;
    result->next_in_hash = state->drag_and_drop_associations[hash_slot];
    result->first = null;
    state->drag_and_drop_associations[hash_slot] = result;
  }
  
  // NOTE(christian): ITEM IS INCOMPATIBLE WITH THE DRAG AND DROP BOX!
  assert_true((item.item_props & result->handle) != 0);
  
  assert_true(result != null);
  ui_add_drag_and_drop_group_node(state, &(result->first), widget, item);
}

fun UI_DragAndDropGroupNode *
ui_find_associated_drag_and_drop_group_node(UI_State *state, UI_Widget *widget) {
  UI_DragAndDropGroupNode *associated = null;
  
  u64 hash_slot = ui_hash_slot_from_drag_and_drop_handle(state, widget->d_and_d_handle);
  UI_DragAndDropBoxNode *result = state->drag_and_drop_associations[hash_slot];
  while (result && (widget->d_and_d_handle != result->handle)) {
    result = result->next_in_hash;
  }
  
  if (result) {
    for (UI_DragAndDropGroupNode  *node = result->first; node; node = node->next) {
      if (str8_equal_strings(node->widget_identifier, widget->identifier_string)) {
        associated = node;
        break;
      }
    }
  }
  
  return(associated);
}

// This is how this works:
// We first check if two widget drag and drop ID's are equal. If yes,
// then search their corresponding drag and drop node in the hash table.
// we then swap texture to both of them and update the texture handles to avoid
// flickering.
fun void
ui_exchange_drag_and_drop_data(UI_State *state, UI_Widget *input, UI_Widget *drop_dest) {
  assert_true(input != null);
  assert_true(drop_dest != null);
  
  assert_true((input->flags & UIWidget_DragAndDrop) != 0);
  assert_true((drop_dest->flags & UIWidget_DragAndDrop) != 0);
  
  if (input->d_and_d_handle & drop_dest->d_and_d_handle) {
    UI_DragAndDropGroupNode *associated_with_input = ui_find_associated_drag_and_drop_group_node(state, input);
    UI_DragAndDropGroupNode *associated_with_dest = ui_find_associated_drag_and_drop_group_node(state, drop_dest);
    
    assert_true(associated_with_input != null);
    assert_true(associated_with_dest != null);
    assert_true((associated_with_input->item.item_props & drop_dest->d_and_d_handle) != 0);
    assert_true((associated_with_dest->item.item_props & input->d_and_d_handle) != 0);
    
    UI_DragAndDropItem temp = associated_with_input->item;
    associated_with_input->item = associated_with_dest->item;
    associated_with_dest->item = temp;
    
    // NOTE(christian): to avoid flickering 
    input->bound_texture = associated_with_input->item.texture;
    drop_dest->bound_texture = associated_with_dest->item.texture;
  }
}

fun UI_Widget *
ui_create_widget(UI_State *state, UI_WidgetFlag widget_flags, String_U8_Const identifier) {
  UI_Key widget_key = ui_hash_string(identifier);
  u64 hash_slot = ui_hash_slot_from_key(state, widget_key);
  UI_Widget *result = state->cache[hash_slot];
  while (result && !str8_equal_strings(result->identifier_string, identifier)) {
    result = result->next_in_hash;
  }
  
  if (!result) {
    result = state->free_widgets;
    if (result) {
      state->free_widgets = state->free_widgets->next_in_free;
    } else {
      result = arena_push_struct(state->widget_arena, UI_Widget);
    }
    
    result->hot = 0;
    result->active = 0;
    result->next_in_hash = state->cache[hash_slot];
    state->cache[hash_slot] = result;
  }
  
  // NOTE(christian): don't allow duplicate identifiers
  if (result->last_frame_touched_index != state->current_frame_index) {
    //sll_push_back(state->first, state->last);
    result->left_sibling = null;
    result->right_sibling = null;
    result->leftmost_child = null;
    result->rightmost_child = null;
    result->next_in_list = null;
    result->progression.type = UIProgression_Null;
    
    result->rel_p_to_parent = v2f_make(0, 0);
    result->final_size = v2f_make(0, 0);
    result->text_dims = v2f_make(0, 0);
    result->children_padding = result->children_gap = v2f_make(0, 0);
    
    UI_Widget *top_parent = null;
    if (state->parent_count > 0) {
      top_parent = ui_peek_parent_top(state);
    }
    
    if (top_parent && (top_parent != result)) {
      result->parent = top_parent;
      if (top_parent->leftmost_child == null) {
        top_parent->leftmost_child = top_parent->rightmost_child = result;
      } else {
        assert_true(top_parent->rightmost_child != null);
        top_parent->rightmost_child->right_sibling = result;
        result->left_sibling = top_parent->rightmost_child;
        top_parent->rightmost_child = result;
      }
    }
    
    sll_push_back_N(result, state->first, state->last, next_in_list);
    ++state->widget_count;
    
    clear_memory(result->semantic_size, sizeof(result->semantic_size));
    
    result->flags = widget_flags;
    result->last_frame_touched_index = state->current_frame_index;
    result->key = widget_key;
    result->identifier_string = str8_copy(state->util_arena, identifier);
    result->content_string = ui_string_as_content(result->identifier_string);
    result->bound_texture = ui_make_clipped_texture_null();
    if (result->flags & UIWidget_TextContent) {
      result->semantic_size[UIAxis_X].container.type = UIContainerSize_TextContent;
      result->semantic_size[UIAxis_X].container.value = ui_peek_text_padding_x_top_auto_pop(state);
      result->semantic_size[UIAxis_Y].container.type = UIContainerSize_TextContent;
      result->semantic_size[UIAxis_Y].container.value = ui_peek_text_padding_y_top_auto_pop(state);
      
      R2D_FontParsed *parsed_font = state->buffer->font + ui_peek_font_size_top_auto_pop(state);
      result->text_dims = r_text_get_dims(parsed_font, result->content_string);
      //result->text_dims.y = state->buffer->font.max_glyph_height;
      
      if (!((result->text_dims.x == 0) && (result->text_dims.y == 0))) {
        result->text_dims.y = (f32)(parsed_font->ascent + parsed_font->descent);
      }
    } else {
      // NOTE(christian): this thing isn't used yet.
      result->children_padding.v[UIAxis_X] = ui_peek_padding_x_top_auto_pop(state);
      result->children_padding.v[UIAxis_Y] = ui_peek_padding_y_top_auto_pop(state);
      
      //result->semantic_size[UIAxis_X].individual_type = UIIndividualSize_Pixels;
      //result->semantic_size[UIAxis_X].individual_value = ui_peek_size_x_top(state);
      //result->semantic_size[UIAxis_Y].individual_type = UIIndividualSize_Pixels;
      //result->semantic_size[UIAxis_Y].individual_value = ui_peek_size_y_top(state);
    }
  }
  
  result->corner_roundness = ui_peek_corner_roundness_top_auto_pop(state);
  result->edge_thickness = ui_peek_edge_thickness_top_auto_pop(state);
  result->border_colour.tl = ui_peek_tl_border_colour_top_auto_pop(state);
  result->border_colour.tr = ui_peek_tr_border_colour_top_auto_pop(state);
  result->border_colour.bl = ui_peek_bl_border_colour_top_auto_pop(state);
  result->border_colour.br = ui_peek_br_border_colour_top_auto_pop(state);
  result->bg_colour.tl = ui_peek_tl_background_colour_top_auto_pop(state);
  result->bg_colour.tr = ui_peek_tr_background_colour_top_auto_pop(state);
  result->bg_colour.bl = ui_peek_bl_background_colour_top_auto_pop(state);
  result->bg_colour.br = ui_peek_br_background_colour_top_auto_pop(state);
  result->text_colour = ui_peek_text_colour_top_auto_pop(state);
  result->anchor[UIAxis_X] = ui_peek_anchor_x_top_auto_pop(state);
  result->anchor[UIAxis_Y] = ui_peek_anchor_y_top_auto_pop(state);
  result->offset[UIAxis_X] = ui_peek_offset_x_top_auto_pop(state);
  result->offset[UIAxis_Y] = ui_peek_offset_y_top_auto_pop(state);
  result->font_size = ui_peek_font_size_top_auto_pop(state);
  result->semantic_size[UIAxis_X].individual = ui_peek_individual_size_x_top_auto_pop(state);
  result->semantic_size[UIAxis_Y].individual = ui_peek_individual_size_y_top_auto_pop(state);
  
  // NOTE(christian): drag-and-drop problem:
  // I want to drag and drop textures / more complicated stuff from one drag-and-droppable
  // to another. How can I do this, given my UI Library? Problems:
  // 
  // 1. Clearly, we cannot just immediately set the texture for drag and drops!
  // Well, when the first time the user called the ui_do_drag_and_drop_tex(...) function,
  // we somehow need to store the information in some typeof cache. The next time we look at it,
  // we just get the texture data there. If the texture data is null, then it has been transfered
  // to another drag and droppable.
  //
  // 2. Now, we got #1 done, how do we actually know the we have dragged and dropped??????
  // I suppose that we do some drag-and-drop cache traversal and check if the we have dropped it
  // to some drag-and-droppable and exchange data such as texture! I said exchange because both
  // source and destination could have data. If the destination data has no data, we just say "transfer"
  // instead of "exchange".
  
  if (result->flags & UIWidget_DragAndDrop) {
    result->d_and_d_handle = ui_peek_drag_and_drop_handle_top_auto_pop(state);
    //assert_true(!ui_drag_and_drop_handle_is_null(result->d_and_d_handle));
    ui_associate_drag_and_drop_tex(state, result, ui_peek_drag_and_drop_item_top_auto_pop(state));
  } else {
    result->d_and_d_handle = ui_make_null_drag_and_drop_handle();
    result->bound_texture = ui_peek_texture_top_auto_pop(state);
  }
  
  result->bound_texture_p = result->rect.p;
  //result->semantic_size[UIAxis_X] = ui_peek_size_x_top(state);
  //result->semantic_size[UIAxis_Y] = ui_peek_size_y_top(state);
  
  
  // TODO(christian): should we get rid of clamping?
  for (UI_AxisType axis_index = UIAxis_X;
       axis_index < UIAxis_Count;
       ++axis_index) {
    switch (result->anchor[axis_index].metric) {
      case UIMetricType_Pixels: {
        result->anchor[axis_index].value = clamp(result->anchor[axis_index].value,
                                                 -result->rect.dims.v[axis_index],
                                                 result->rect.dims.v[axis_index]);
      } break;
      
      case UIMetricType_Percentage: {
        result->anchor[axis_index].value = clamp(result->anchor[axis_index].value,
                                                 -1.0f,
                                                 1.0f);
      } break;
    }
  }
  
  if (result->edge_thickness < 1.0f) {
    result->edge_thickness = 0.0f;
  }
  return(result);
}

inl void
ui_set_hot(UI_State *state, UI_Key key) {
  state->hot_key = key;
}

inl void
ui_set_active(UI_State *state, UI_Key key) {
  state->active_key = key;
}

inl b32
ui_is_hot(UI_State *state, UI_Key key) {
  return ui_match_keys(state->hot_key, key);
}

inl b32
ui_is_focus_hot(UI_State *state, UI_Key key) {
  return ui_match_keys(state->focus_hot_key, key);
}

inl b32
ui_is_focus_active(UI_State *state, UI_Key key) {
  return ui_match_keys(state->focus_active_key, key);
}

inl b32
ui_is_active(UI_State *state, UI_Key key) {
  return ui_match_keys(state->active_key, key);
}

inl void
ui_set_focus_hot(UI_State *state, UI_Key key) {
  state->focus_hot_key = key;
}

inl void
ui_set_focus_active(UI_State *state, UI_Key key) {
  state->focus_active_key = key;
}

fun b32
ui_check_children_hover(UI_Widget *widget, v2f mouse_p) {
  if ((widget->flags & UIWidget_Clickable) &&
      point_in_rect(&mouse_p, &widget->rect)) {
    return true;
  }
  
  for (UI_Widget *child = widget->leftmost_child;
       child;
       child = child->right_sibling) {
    if (ui_check_children_hover(child, mouse_p)) {
      return true;
    }
  }
  
  return false;
}

typedef struct UI_Widget_Node UI_Widget_Node;
struct UI_Widget_Node {
  UI_Widget *widget;
  UI_Widget_Node *next;
};

fun void
push_widget_in_queue(Memory_Arena *arena, UI_Widget_Node **start,
                     UI_Widget_Node **end, UI_Widget *widget) {
  if (widget && (widget->graph_algo_state == UIWidgetState_Undiscovered)) {
    widget->graph_algo_state = UIWidgetState_Discovered;
    
    UI_Widget_Node *new_queue_node = arena_push_struct(arena, UI_Widget_Node);
    new_queue_node->widget = widget;
    new_queue_node->next = null;
    
    sll_push_back(new_queue_node, *start, *end);
  }
}

// TODO(christian): this function simulates widget interactions.
// it returns the result of how the user interacted with it.
fun UI_Interact
ui_interact_from_widget(UI_State *state, UI_Widget *input) {
  UI_Interact result = { 0 };
  
  u8 mouse_released = (u8)(os_mouse_released(state->input, OSInputMouse_Left));
  u8 mouse_pressed = (u8)(os_mouse_pressed(state->input, OSInputMouse_Left));
  u8 mouse_held = (u8)(os_mouse_held(state->input, OSInputMouse_Left));
  
  v2f mouse_p = os_get_mouse_v2f(state->input);
  if (point_in_rect(&mouse_p, &input->rect)) {
    b32 hover_on_children = false;
    for (UI_Widget *child = input->leftmost_child;
         child;
         child = child->right_sibling) {
      if (ui_check_children_hover(child, mouse_p)) {
        hover_on_children = true;
        break;
      }
    }
    
    if (!hover_on_children) {
      result.hover = 1;
      
      if (input->flags & UIWidget_Clickable) {
        result.released = mouse_released;
        result.pressed = mouse_pressed;
        result.held = mouse_held;
        
        if ((input->flags & UIWidget_DragAndDrop) && 
            str8_is_null(state->current_dragging_identifier)) {
          if (result.held && !ui_clipped_texture_is_null(input->bound_texture)) {
            if (!str8_equal_strings(state->current_dragging_identifier, input->identifier_string)) {
              state->mouse_p_to_tl_d_and_d = v2f_sub(input->rect.p, mouse_p);
            }
            
            state->current_dragging_identifier = input->identifier_string;
            result.is_dragging = 1;
          }
        }
      }
      
      if (input->flags & UIWidget_HotAnimation) {
        ui_set_hot(state, input->key);
      }
      
      if (input->flags & UIWidget_ActiveAnimation) {
        if (result.held) {
          ui_set_active(state, input->key);
        }
      }
      
      if (input->flags & UIWidget_FocusOnClick) {
        ui_set_focus_hot(state, input->key); // hover
        
        if (os_mouse_released(state->input, OSInputMouse_Left)) {
          ui_set_focus_active(state, input->key);
        }
      }
    }
  }
  
  return(result);
}

inl void
ui_push_hori_layout(UI_State *state, String_U8_Const identifier, v2f padding, v2f gap) {
  UI_Widget *layout = ui_create_widget(state, UIWidget_BorderColour |
                                       UIWidget_BackgroundColour,
                                       identifier);
  
  layout->children_layout_direction = UIAxis_X;
  layout->children_padding = padding;
  layout->children_gap = gap;
  layout->semantic_size[UIAxis_X].container.type = UIContainerSize_ChildrenSum;
  //layout->semantic_size[UIAxis_X].value = gap_x_in_px;
  layout->semantic_size[UIAxis_Y].container.type = UIContainerSize_ChildrenSum;
  //layout->semantic_size[UIAxis_Y].value = gap_y_in_px;
  
  ui_push_parent(state, layout);
}

inl void
ui_push_vert_layout(UI_State *state, String_U8_Const identifier, v2f padding, v2f gap) {
  UI_Widget *layout = ui_create_widget(state, UIWidget_BorderColour |
                                       UIWidget_BackgroundColour,
                                       identifier);
  
  layout->children_layout_direction = UIAxis_Y;
  layout->children_padding = padding;
  layout->children_gap = gap;
  
  layout->semantic_size[UIAxis_X].container.type = UIContainerSize_ChildrenSum;
  //layout->semantic_size[UIAxis_X].value = gap_x_in_px;
  layout->semantic_size[UIAxis_Y].container.type = UIContainerSize_ChildrenSum;
  //layout->semantic_size[UIAxis_Y].value = gap_y_in_px;
  
  ui_push_parent(state, layout);
}

fun UI_Interact
ui_do_button(UI_State *state, String_U8_Const string) {
  UI_Widget *button = ui_create_widget(state,
                                       UIWidget_TextContent |
                                       UIWidget_Clickable |
                                       UIWidget_HotAnimation |
                                       UIWidget_ActiveAnimation |
                                       UIWidget_BorderColour |
                                       UIWidget_BackgroundColour,
                                       string);
  
  return ui_interact_from_widget(state, button);
}

fun UI_Interact
ui_do_buttonf(UI_State *state, String_U8_Const string, ...) {
  Memory_Arena *scratch = arena_get_scratch(null, 0);
  Temporary_Memory temp = temp_mem_begin(scratch);
  
  va_list args;
  va_start(args, string);
  String_U8 format = str8_format_va(scratch, string, args);
  va_end(args);
  
  UI_Interact clicked = ui_do_button(state, format);
  
  temp_mem_end(temp);
  
  return(clicked);
}

fun UI_Interact
ui_do_button_tex(UI_State *state, String_U8_Const identifier) {
  UI_Widget *button = ui_create_widget(state,
                                       UIWidget_Clickable |
                                       UIWidget_HotAnimation |
                                       UIWidget_ActiveAnimation |
                                       UIWidget_BorderColour |
                                       UIWidget_BackgroundColour |
                                       UIWidget_AttatchTexture,
                                       identifier);
  
	return ui_interact_from_widget(state, button);
}

fun UI_Interact
ui_do_button_texf(UI_State *state, String_U8_Const identifier, ...) {
  Memory_Arena *scratch = arena_get_scratch(null, 0);
  Temporary_Memory temp = temp_mem_begin(scratch);
  
  va_list args;
  va_start(args, identifier);
  String_U8 format = str8_format_va(scratch, identifier, args);
  va_end(args);
  
  UI_Interact clicked = ui_do_button_tex(state, format);
  
  temp_mem_end(temp);
  
  return(clicked);
}

fun UI_Interact
ui_do_label(UI_State *state, String_U8_Const string) {
  UI_Widget *label = ui_create_widget(state,
                                      UIWidget_TextContent |
                                      UIWidget_BorderColour |
                                      UIWidget_BackgroundColour,
                                      string);
  
  return ui_interact_from_widget(state, label);
}

fun UI_Interact
ui_do_label_tex(UI_State *state, String_U8_Const identifier) {
  UI_Widget *label = ui_create_widget(state,
                                      UIWidget_HotAnimation |
                                      UIWidget_ActiveAnimation |
                                      UIWidget_BorderColour |
                                      UIWidget_BackgroundColour |
                                      UIWidget_AttatchTexture,
                                      identifier);
  
  return ui_interact_from_widget(state, label);
}

fun UI_Interact
ui_do_label_texf(UI_State *state, String_U8_Const identifier, ...) {
  Memory_Arena *scratch = arena_get_scratch(null, 0);
  Temporary_Memory temp = temp_mem_begin(scratch);
  
  va_list args;
  va_start(args, identifier);
  String_U8 format = str8_format_va(scratch, identifier, args);
  va_end(args);
  
  UI_Interact interact = ui_do_label_tex(state, format);
  
  temp_mem_end(temp);
  
  return(interact);
}

fun UI_Interact
ui_do_labelf(UI_State *state, String_U8_Const string, ...) {
  Memory_Arena *scratch = arena_get_scratch(null, 0);
  Temporary_Memory temp = temp_mem_begin(scratch);
  
  va_list args;
  va_start(args, string);
  String_U8 format = str8_format_va(scratch, string, args);
  va_end(args);
  
  UI_Interact interact = ui_do_label(state, format);
  
  temp_mem_end(temp);
  
  return(interact);
}

fun UI_Widget *
ui_find_widget_from_string(UI_State *state, String_U8_Const string) {
  UI_Key key = ui_hash_string(string);
  u64 hash_slot = ui_hash_slot_from_key(state, key);
  UI_Widget *result = state->cache[hash_slot];
  while (result && !str8_equal_strings(result->identifier_string, string)) {
    result = result->next_in_hash;
  }
  
  return(result);
}

// NOTE(christian): OK So we define how this works.
// Lets go with preamble first. So, we cannot use the String_U8 char count member variable as 
// "string cursor" because the user might want to shift the cursor to some reasonable range
// [0, char_count). So we need a user-provided cursor_idx that the UI system uses to shift the
// cursor. Bam. 
// Now, we have a problem. The way ui_create_widget widget works is that it creates
// a new widget if UI WIDGET identifier_string does not exist in the cache. Well, identifier string
// in this case contains user input int string. This implies that whenever a user enters new data,
// the previous int_input widget will be evicted from the cache and thus create a new int_input widget,
// with new user provided data. Now, the ui_interact_from_widget checks to see if we indeed
// clicked int_input for interaction based on the int_input widget. Well, its a new widget and
// the old one is not interactable. Thus wehenever a user types something, the widget will register
// the user's input THIS FRAME, but next frame it will no longer recives input unless we interact with it.
// BAD BAD BAD!
// Whats the solution to this based on what we have? Heres my solution:
// Get the previous UI_Widget and if its currently active, swap with new UI_Widget as active.
fun UI_Interact
ui_do_int_input(UI_State *state, String_U8 *string_to_edit, u64 *cursor_idx) {
  UI_Widget *input = ui_create_widget(state,
                                      UIWidget_TextContent |
                                      UIWidget_HotAnimation |
                                      UIWidget_ActiveAnimation |
                                      UIWidget_BorderColour |
                                      UIWidget_BackgroundColour |
                                      UIWidget_FocusOnClick,
                                      *string_to_edit);
  
  if (ui_is_focus_active(state, input->key)) {
    for (u8 c = '0'; c < ':'; c += 1) {
      if (os_key_pressed(state->input, (OS_Input_Key_Type)(c))) {
        // ...........
        // 1111.......
        // 111111.....
        // 111111.....
        if ((*cursor_idx <= string_to_edit->char_count) && 
            ((*cursor_idx < string_to_edit->char_capacity))) {
          string_to_edit->string[(*cursor_idx)++] = c;
          string_to_edit->char_count += 1;
        }
        
        break;
      }
    }
    
    if (os_key_pressed(state->input, OSInputKey_Backspace)) {
      if ((*cursor_idx > 0) && (string_to_edit->char_count > 0)) {
        string_to_edit->string[--(*cursor_idx)] = ' ';
        string_to_edit->char_count -= 1;
      }
    }
    
    // TODO(christian): YIIIIIIIKES FIND A BETTER WAY TO DO THIS
    UI_Key widget_key = ui_hash_string(*string_to_edit);
    ui_set_focus_active(state, widget_key);
  }
  
  return ui_interact_from_widget(state, input);
}

fun UI_Interact
ui_do_progression(UI_State *state,
                  String_U8_Const identifier,
                  UI_ProgressionType type,
                  UI_ProgressionFlags flags,
                  UI_AxisType direction,
                  f32 initial, f32 max, f32 size, f32 thickness) {
  assert_true(type != UIProgression_Null);
  UI_Widget *progression_widget = ui_create_widget(state,
                                                   UIWidget_HotAnimation |
                                                   UIWidget_ActiveAnimation |
                                                   UIWidget_BorderColour |
                                                   UIWidget_BackgroundColour,
                                                   identifier);
  
  UI_Progression *progression = &progression_widget->progression;
  progression->type = type;
	progression->flags = flags;
  progression->direction = direction;
  progression->current = initial;
  progression->max = max;
  
  progression_widget->semantic_size[direction].individual.type = UIIndividualSize_Pixels;
  progression_widget->semantic_size[direction].individual.value = size;
  progression_widget->semantic_size[!direction].individual.type = UIIndividualSize_Pixels;
  progression_widget->semantic_size[!direction].individual.value = thickness;
  
  return ui_interact_from_widget(state, progression_widget);
}

fun UI_Interact
ui_do_drag_and_drop_tex(UI_State *state, String_U8_Const identifier) {
  UI_Widget *d_and_d = ui_create_widget(state,
                                        UIWidget_HotAnimation |
                                        UIWidget_ActiveAnimation |
                                        UIWidget_BorderColour |
                                        UIWidget_BackgroundColour |
                                        UIWidget_AttatchTexture | 
                                        UIWidget_DragAndDrop |
                                        UIWidget_Clickable,
                                        identifier);
  
  return ui_interact_from_widget(state, d_and_d);
}

// NOTE(christian): the size depends on the direction. If direction is X-Axis,
// Then size corresponds to width. If direction is Y-Axis, then size corresponds
// to height. 
// Likewise, the thickness depends on the direction. If direction is X-Axis,
// it refers to height. If direction is Y-Axis, then thickness refers to width.
fun void
ui_begin(UI_State *state, f32 elapsed_time_secs, R_RenderPass_UI *ui_pass) {
  // TODO(christian): fix this
  //u32 view_width, view_height;
  //R2D_GetViewportDims(&view_width, &view_height);
  u32 view_width;
  u32 view_height;
	r_viewport_get_dims(state->buffer, &view_width, &view_height);
  
  state->pass = ui_pass;
  state->elapsed_time_secs = elapsed_time_secs;
  
  for (u32 bucket_index = 0; bucket_index < array_count(state->cache); bucket_index += 1) {
    UI_Widget **start = state->cache + bucket_index;
    while (*start) {
      if ((*start)->last_frame_touched_index != state->current_frame_index) {
        UI_Widget *temp = *start;
        (*start) = (*start)->next_in_hash;
        temp->next_in_free = state->free_widgets;
        state->free_widgets = temp;
      } else {
        start = &((*start)->next_in_hash);
      }
    }
  }
  
  ++state->current_frame_index;
  
  state->root = state->last = null;
  state->widget_count = 0;
  
  state->parent_count = 0;
  state->corner_roundness_count = 0;
  state->edge_thickness_count = 0;
  state->tl_border_colour_count = 0;
  state->tr_border_colour_count = 0;
  state->bl_border_colour_count = 0;
  state->br_border_colour_count = 0;
  state->tl_background_colour_count = 0;
  state->tr_background_colour_count = 0;
  state->bl_background_colour_count = 0;
  state->br_background_colour_count = 0;
  state->text_colour_count = 0;
  state->anchor_x_count = 0;
  state->anchor_y_count = 0;
  state->offset_x_count = 0;
  state->offset_y_count = 0;
  state->text_padding_x_count = 0;
  state->text_padding_y_count = 0;
  state->padding_x_count = 0;
  state->padding_y_count = 0;
  state->font_size_count = 0;
  state->texture_count = 0;
  state->individual_size_x_count = 0;
  state->individual_size_y_count = 0;
  state->drag_and_drop_handle_count = 0;
  state->drag_and_drop_item_count = 0;
  
  ui_push_corner_roundness(state, 6.0f);
  ui_push_edge_thickness(state, 1.0f);
  ui_push_border_colour(state, v4f_make(0.8f, 0.8f, 0.8f, 1.0f));
  ui_push_background_colour(state, v4f_make(0.0f, 0.0f, 0.0f, 1.0f));
  ui_push_text_colour(state, v4f_make(1.0f, 1.0f, 1.0f, 1.0f));
  ui_push_anchor_x(state, ui_make_anchor(UIMetricType_Pixels, 0.0f));
  ui_push_anchor_y(state, ui_make_anchor(UIMetricType_Pixels, 0.0f));
  ui_push_offset_x(state, ui_make_offset(UIOffsetType_Relative, UIMetricType_Pixels, 0.0f));
  ui_push_offset_y(state, ui_make_offset(UIOffsetType_Relative, UIMetricType_Pixels, 0.0f));
  ui_push_text_padding_x(state, 16.0f);
  ui_push_text_padding_y(state, 8.0f);
  ui_push_padding_x(state, 0.0f);
  ui_push_padding_y(state, 0.0f);
  ui_push_font_size(state, R_FontSize_Small);
  ui_push_texture(state, ui_make_clipped_texture_null());
  ui_push_individual_size_x(state, ui_make_individual_size(UIIndividualSize_Null, 0));
  ui_push_individual_size_y(state, ui_make_individual_size(UIIndividualSize_Null, 0));
  ui_push_drag_and_drop_handle(state, ui_make_null_drag_and_drop_handle());
  ui_push_drag_and_drop_item(state, ui_make_null_drag_and_drop_item());
  
  ui_set_hot(state, ui_zero_key());
  ui_set_active(state, ui_zero_key());
  
  UI_Widget *root = ui_create_widget(state, 0, str8_format(state->util_arena, str8("main_container_root_of_all_widgets")));
  root->semantic_size[UIAxis_X].individual.type = UIIndividualSize_Pixels;
  root->semantic_size[UIAxis_X].individual.value = (f32)(view_width);
  root->semantic_size[UIAxis_Y].individual.type = UIIndividualSize_Pixels;
  root->semantic_size[UIAxis_Y].individual.value = (f32)(view_height);
  root->children_layout_direction = UIAxis_Y;
  state->root = root;
	// TODO(CJ): Somehow, this changes somewhere in the code, and I DO NOT KNOW WHERE.
	// SO I HAVE TO MANUALLy SET THIS TO ZERO VECTOR
	state->root->rect.p = v2f_make(0, 0);
  
  // NOTE(christian): defaults
  ui_push_parent(state, root); // exception
}

typedef struct UI_Widget_Stack UI_Widget_Stack;
struct UI_Widget_Stack {
  UI_Widget *widget;
  UI_Widget_Stack *next;
};

fun void
ui_calculate_standalone_sizes(UI_Widget *root, UI_AxisType axis) {
  switch (root->semantic_size[axis].individual.type) {
    case UIIndividualSize_Pixels: {
      root->final_size.v[axis] = root->semantic_size[axis].individual.value;
    } break;
    
    case UIIndividualSize_PercentOfParent: {
    } break;
  }
  
  switch (root->semantic_size[axis].container.type) {
    // NOTE(christian): the semantic size of text is padding
    case UIContainerSize_TextContent: {
      root->final_size.v[axis] += root->text_dims.v[axis] + root->semantic_size[axis].container.value * 2.0f;
    } break;
  }
  
  for (UI_Widget *child = root->leftmost_child;
       child;
       child = child->right_sibling) {
    ui_calculate_standalone_sizes(child, axis);
  }
}

fun void
ui_calculate_downwards_dependent_sizes(UI_Widget *root, UI_AxisType axis) {
  // NOTE(christian): this variable depends on the axis
  f32 accumulated_size = 0;
  if (root->semantic_size[axis].container.type == UIContainerSize_ChildrenSum) {
    if (root->children_layout_direction == axis) {
      for (UI_Widget *child = root->leftmost_child;
           child;
           child = child->right_sibling) {
        ui_calculate_downwards_dependent_sizes(child, axis);
        if (child->right_sibling) {
          accumulated_size += child->final_size.v[axis] + root->children_gap.v[axis] + child->edge_thickness * 0.5f;
        } else {
          accumulated_size += child->final_size.v[axis] + child->edge_thickness * 2.0f;
        }
      }
    } else {
      for (UI_Widget *child = root->leftmost_child;
           child;
           child = child->right_sibling) {
        ui_calculate_downwards_dependent_sizes(child, axis);
        f32 child_size = child->final_size.v[axis] + child->edge_thickness * 2.0f;
        
        if (child_size > accumulated_size) {
          accumulated_size = child_size;
        }
      }
    }
    
    root->final_size.v[axis] = accumulated_size + root->children_padding.v[axis] * 2.0f; // pad
  } else {
    for (UI_Widget *child = root->leftmost_child;
         child;
         child = child->right_sibling) {
      ui_calculate_downwards_dependent_sizes(child, axis);
    }
  }
}

fun void
ui_calculate_final_positions(UI_Widget *root, UI_AxisType axis) {
  f32 p_accumulate = 0;
  if (root->children_layout_direction == axis) {
    f32 gap = root->children_gap.v[axis];
    
    for (UI_Widget *child = root->leftmost_child;
         child;
         child = child->right_sibling) {
      UI_Offset ui_offset = child->offset[axis];
      
      switch (ui_offset.type) {
        case UIOffsetType_Relative: {
          child->rel_p_to_parent.v[axis] += p_accumulate + root->children_padding.v[axis] + child->edge_thickness;
          
          if (child->right_sibling) {
            p_accumulate += child->final_size.v[axis] + gap + root->edge_thickness * 0.5f;
          }
        } break;
        
				case UIOffsetType_Absolute: {
					// absolute: do nothing.
				} break;
        
        default: {
          invalid_code_path();
        } break;
      }
      
      // NOTE(christian): we can see a future problem: Combinatoric explosion.
      f32 anchor_offset_in_pixels = 0;
      switch (child->anchor[axis].metric) {
        case UIMetricType_Pixels: {
          anchor_offset_in_pixels = -child->anchor[axis].value;
        } break;
        
        case UIMetricType_Percentage: {
          anchor_offset_in_pixels = -child->anchor[axis].value * child->final_size.v[axis];
        } break;
        
        default: {
          wc_assert(!"no impl");
        } break;
      }
      
      f32 offset_in_pixels = 0;
      switch (ui_offset.metric) {
        case UIMetricType_Pixels: {
          offset_in_pixels = ui_offset.value;
        } break;
        
        case UIMetricType_Percentage: {
          offset_in_pixels = ui_offset.value * root->final_size.v[axis];
        } break;
        
        default: {
          wc_assert(!"no impl");
        } break;
      }
      
      child->rel_p_to_parent.v[axis] += anchor_offset_in_pixels + offset_in_pixels;
      
      // NOTE(christian): final p's and sizes
      child->rect.p.v[axis] = root->rect.p.v[axis] + child->rel_p_to_parent.v[axis];
      child->rect.dims.v[axis] = child->final_size.v[axis];
      
      ui_calculate_final_positions(child, axis);
    }
  } else {
    for (UI_Widget *child = root->leftmost_child;
         child;
         child = child->right_sibling) {
      UI_Offset ui_offset = child->offset[axis];
      
      switch (ui_offset.type) {
        case UIOffsetType_Relative: {
          child->rel_p_to_parent.v[axis] += root->children_padding.v[axis];
        } break;
        
        case UIOffsetType_Absolute: {
        } break;
        
        default: {
          invalid_code_path();
        } break;
      }
      
      f32 anchor_offset_in_pixels = 0;
      switch (child->anchor[axis].metric) {
        case UIMetricType_Pixels: {
          anchor_offset_in_pixels = -child->anchor[axis].value;
        } break;
        
        case UIMetricType_Percentage: {
          anchor_offset_in_pixels = -child->anchor[axis].value * child->final_size.v[axis];
        } break;
      }
      
      f32 offset_in_pixels = 0;
      switch (ui_offset.metric) {
        case UIMetricType_Pixels: {
          offset_in_pixels = ui_offset.value;
        } break;
        
        case UIMetricType_Percentage: {
          offset_in_pixels = ui_offset.value * root->final_size.v[axis];
        } break;
        
        default: {
          wc_assert(!"no impl");
        } break;
      }
      
      child->rel_p_to_parent.v[axis] += anchor_offset_in_pixels + offset_in_pixels;
      child->rel_p_to_parent.v[axis] += child->edge_thickness;
      
      child->rect.p.v[axis] = root->rect.p.v[axis] + child->rel_p_to_parent.v[axis];
      child->rect.dims.v[axis] = child->final_size.v[axis];
      
      ui_calculate_final_positions(child, axis);
    }
  }
}

fun void
ui_scale_colours(UI_RectColours *colours, f32 scale) {
  for (u32 col = 0; col < array_count(colours->c); ++col) {
    colours->c[col].r *= scale;
    colours->c[col].g *= scale;
    colours->c[col].b *= scale;
  }
}

fun void
ui_simulate_delayed_input(UI_State *state, UI_Widget *input) {
  if (str8_is_null(state->current_dragging_identifier)) {
    return;
  }
  
  // TODO(christian): a linked-list of drag-and-droppables to avoid traversing the
  // entire tree
  if (input && str8_equal_strings(state->current_dragging_identifier, input->identifier_string)) {
    assert_true((input->flags & UIWidget_DragAndDrop) != 0);
    
    v2f mouse_p = os_get_mouse_v2f(state->input);
    u8 mouse_released = (u8)(os_mouse_released(state->input, OSInputMouse_Left));
    u8 mouse_held = (u8)(os_mouse_held(state->input, OSInputMouse_Left));
    
    if (mouse_held) {
      input->bound_texture_p = v2f_add(mouse_p, state->mouse_p_to_tl_d_and_d);
      state->current_dragging_widget = input;
    } else if (mouse_released) {
      assert_false(ui_clipped_texture_is_null(input->bound_texture));
      
      UI_Widget *drop_dest = null;
      input->bound_texture_p = input->rect.p;
      state->current_dragging_identifier = str8_make_null();
      
#if 1
      UI_DragAndDropGroupNode *associated_group_node = ui_find_associated_drag_and_drop_group_node(state, input);
      assert_true(associated_group_node != null);
      UI_DragAndDropItem *item = &(associated_group_node->item);
      
      for (u32 table_index = 0;
           (table_index < array_count(state->drag_and_drop_associations)) && !drop_dest;
           ++table_index) {
        for (UI_DragAndDropBoxNode *table = state->drag_and_drop_associations[table_index];
             table;
             table = table->next_in_hash) {
          if (item->item_props & table->handle) {
            for (UI_DragAndDropGroupNode  *node = table->first; node; node = node->next) {
              UI_Widget *d_widget = ui_find_widget_from_string(state, node->widget_identifier);
              assert_true((d_widget->flags & UIWidget_DragAndDrop) != 0);
              assert_true(d_widget != null);
              
              if ((d_widget != input) && point_in_rect(&mouse_p, &(d_widget->rect))) {
                drop_dest = d_widget;
                break;
              }
            }
          }
        }
      }
#else
      
      // NOTE(christian): search the entire tree for a droppable 
      // destination using BFS.
      Memory_Arena *bfs_memory = arena_get_scratch(null, 0);
      Temporary_Memory bfs_temp_memory = temp_mem_begin(bfs_memory);
      
      for (UI_Widget *widget = state->first; widget; widget = widget->next_in_list) {
        widget->graph_algo_state = UIWidgetState_Undiscovered;
      }
      
      UI_Widget_Node *queue_start = arena_push_struct(bfs_memory, UI_Widget_Node);
      UI_Widget_Node *queue_end = queue_start;
      queue_start->widget = state->root;
      queue_start->next = null;
      state->root->graph_algo_state = UIWidgetState_Discovered;
      
      while (queue_start) {
        UI_Widget_Node *dequeued = queue_start;
        queue_start = queue_start->next;
        if (queue_start == null) {
          queue_end = null;
        }
        
        UI_Widget *d_widget = dequeued->widget;
        assert_true(d_widget != null);
        
        if (d_widget->flags & UIWidget_DragAndDrop) {
          if ((d_widget != input) && point_in_rect(&mouse_p, &(d_widget->rect))) {
            drop_dest = d_widget;
            break;
          }
        }
        
        // for each nodes in adjacency of dequeued, do...
        push_widget_in_queue(bfs_memory, &queue_start, &queue_end, d_widget->left_sibling);
        push_widget_in_queue(bfs_memory, &queue_start, &queue_end, d_widget->right_sibling);
        push_widget_in_queue(bfs_memory, &queue_start, &queue_end, d_widget->leftmost_child);
        push_widget_in_queue(bfs_memory, &queue_start, &queue_end, d_widget->rightmost_child);
        
        d_widget->graph_algo_state = UIWidgetState_Processed;
      }
      temp_mem_end(bfs_temp_memory);
#endif
      
      if (!drop_dest) {
        // NOTE(christian): if our mouse has not collided with any drag-and-droppables,
        // then, return the drag-and-drop texture to its original position.
        input->bound_texture_p = input->rect.p;
      } else {
        // NOTE(christian): else, we have found a target drag and drop
        ui_exchange_drag_and_drop_data(state, input, drop_dest);
        state->has_dropped_this_frame = true;
      }
    }
  }
}

fun void
ui_draw_widgets(UI_State *state, UI_Widget *root) {
  f32 colour_multiplier = 1.0f;
  
  // TODO(christian): LIL Cleanup alot of repetitive code
  if (root->progression.type != UIProgression_Null) {
    UI_Progression *progression = &(root->progression);
    switch (progression->type) {
      case UIProgression_Gradient: {
        UI_RectColours bg_colour = root->bg_colour;
        ui_scale_colours(&bg_colour, colour_multiplier);
        
        f32 percentage = progression->current / progression->max;
        v2f dims = { 0 };
        
        if (root->flags & UIWidget_BackgroundColour) {
          switch (progression->direction) {
            case UIAxis_X: {
							dims.x = percentage * root->rect.dims.x;
							dims.y = root->rect.dims.y;
              
							if (progression->flags & UIProgressionFlag_ClipBackgroundColour) {
								bg_colour.tr.rgb = v3f_scale(bg_colour.tr.rgb, percentage);
								bg_colour.br.rgb = v3f_scale(bg_colour.br.rgb, percentage);
							}
						} break;
            
						case UIAxis_Y: {
							dims.x = root->rect.dims.x;
							dims.y = percentage * root->rect.dims.y;
							
							if (progression->flags & UIProgressionFlag_ClipBackgroundColour) {
								bg_colour.bl.rgb = v3f_scale(bg_colour.bl.rgb, percentage);
								bg_colour.br.rgb = v3f_scale(bg_colour.br.rgb, percentage);
							}
						} break;
            
						default: {
							invalid_code_path();
						} break;
					}
          
          ui_rect_no_shadow(state, root->rect.p, dims, bg_colour, root->corner_roundness, 0.0f);
				}
        
				if ((root->flags & UIWidget_BorderColour) && (root->edge_thickness >= 1.0f)) {
					UI_RectColours border_colour = root->border_colour;
					ui_scale_colours(&border_colour, colour_multiplier);
          
					ui_rect_no_shadow(state, v2f_sub(root->rect.p, v2f_scale(v2f_make(root->edge_thickness, root->edge_thickness), 0.5f)),
                            v2f_add(root->rect.dims, v2f_make(root->edge_thickness, root->edge_thickness)), border_colour,
                            root->corner_roundness, root->edge_thickness);
				}
			} break;
		}
	} else if (!r_handle_match(root->bound_texture.handle, r_handle_make_bad()) &&
             (root->flags & UIWidget_AttatchTexture)) {
		if (root->flags & UIWidget_BackgroundColour) {
			UI_RectColours bg_colour = root->bg_colour;
			ui_scale_colours(&bg_colour, colour_multiplier);
			ui_rect_no_shadow(state, root->rect.p, root->rect.dims, bg_colour, root->corner_roundness, 0.0f);
		}
    
    // TODO(christian): UGLY! make root->bound_texture_p somewhat synchornized with root->rect.p 
    // to avoid flickering
    // NOTE(christian): the texture being dragged
    if (!str8_equal_strings(state->current_dragging_identifier, root->identifier_string)) {
      ui_texture(state, root->bound_texture, root->rect.p, root->rect.dims, 0);
    }
    
		if ((root->flags & UIWidget_BorderColour) && (root->edge_thickness >= 1.0f)) {
			UI_RectColours border_colour = root->border_colour;
			ui_scale_colours(&border_colour, colour_multiplier);
      
			ui_rect_no_shadow(state, v2f_sub(root->rect.p, v2f_scale(v2f_make(root->edge_thickness, root->edge_thickness), 0.5f)),
                        v2f_add(root->rect.dims, v2f_make(root->edge_thickness, root->edge_thickness)), border_colour,
                        root->corner_roundness, root->edge_thickness);
		}
	} else {
		if (root->flags & UIWidget_BackgroundColour) {
			UI_RectColours bg_colour = root->bg_colour;
			ui_scale_colours(&bg_colour, colour_multiplier);
			
			ui_rect_no_shadow(state, root->rect.p, root->rect.dims, bg_colour, root->corner_roundness, 0.0f);
		}
		
		if ((root->flags & UIWidget_BorderColour) &&
        (root->edge_thickness >= 1.0f)) {
			UI_RectColours border_colour = root->border_colour;
			ui_scale_colours(&border_colour, colour_multiplier);
			
			ui_rect_no_shadow(state, v2f_sub(root->rect.p, v2f_scale(v2f_make(root->edge_thickness, root->edge_thickness), 0.5f)),
                        v2f_add(root->rect.dims, v2f_make(root->edge_thickness, root->edge_thickness)), border_colour,
                        root->corner_roundness, root->edge_thickness);
		}
		
		if (root->flags & UIWidget_TextContent) {
			v2f final_p = v2f_add(root->rect.p, v2f_scale(v2f_sub(root->rect.dims, root->text_dims), 0.5f));
			ui_text(state, root->font_size, final_p, root->text_colour, root->content_string);
		}
	}
  
  for (UI_Widget *child = root->leftmost_child;
       child;
       child = child->right_sibling) {
    ui_draw_widgets(state, child);
  }
}

fun void
ui_end(UI_State *state) {
	assert_true(state->root->rect.p.x == 0);
	assert_true(state->root->rect.p.y == 0);
  
  for (UI_AxisType axis = 0; axis < UIAxis_Count; axis += 1) {
    /*  1. Calculate the "standalone sizes".
   //     - these are sizes that do not depend on other widgets and can be calculated
  //       purely from the infomation that comes from the single widget having its size
  //       calculated. (Our friend: UISizeType_Pixels, UISizeType_TextContent)
   */
    ui_calculate_standalone_sizes(state->root, axis);
    
    /*  2. Calculate "upwards-dependent" sizes.
  //     - These are widget sizes that depends on ancestor's size.
  //     - Example: UISizeType_PercentOfParent
  // UI_CalculateStandaloneSizes(state->root, (axis));
  */
    
    /*  3. Calculate "downwards-dependent" sizes.
  //     - These are widget sizes that depends on decendant's size.
  //     - Example: UI_SizeKind_ChildrenSum, for containers
  */
    ui_calculate_downwards_dependent_sizes(state->root, axis);
    
    /*  4. Calculate the final sizes of widgets.
  //     - compute relative positions for each widget.
  //     - compute final positions for each widget
  */
    ui_calculate_final_positions(state->root, axis);
  }
  
  // TODO(christian): use hash table to traverse.
  for (UI_Widget *widget = state->root; widget; widget = widget->next_in_list) {
    ui_simulate_delayed_input(state, widget);
  }
  
  // TODO(christian): texture paddings? Let user set texture dimensions?
  ui_draw_widgets(state, state->root);
  if (state->current_dragging_widget) {
    ui_texture(state, state->current_dragging_widget->bound_texture,
               state->current_dragging_widget->bound_texture_p,
               state->current_dragging_widget->rect.dims, 0);
  }
  
  state->current_dragging_widget = null;
  
  state->parent_count = 0;
  arena_clear(state->util_arena);
}

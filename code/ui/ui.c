enum {
    UIMiscFlag_HotThisPass = 0x1,
    UIMiscFlag_ActiveThisPass = 0x2,
};

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

inl R2D_Quad *
ui_rect(UI_State *state, v2f p, v2f dims, UI_RectColours colours,
        f32 corner_roundness, f32 side_thickness) {
    assert_true(state->buffer != null);
    R2D_Quad *quad = r2d_acquire_quad(&state->buffer->ui_quads);
    quad->origin = p;
    quad->x_axis = v2f_make(dims.x, 0.0f);
    quad->y_axis = v2f_make(0.0f, dims.y);
    quad->corner_colours[0] = colours.tl;
    quad->corner_colours[1] = colours.tr;
    quad->corner_colours[2] = colours.bl;
    quad->corner_colours[3] = colours.br;
    quad->corner_roundness = corner_roundness;
    quad->side_thickness = side_thickness;
    
    return(quad);
}

inl void
ui_text(UI_State *state, v2f p, v4f colour, String_U8_Const string) {
    r2d_text(&state->buffer->ui_quads, &state->buffer->font, p, colour, string);
}

inl R2D_Quad *
ui_texture(UI_State *state, UI_ClippedTexture texture, v2f p, v2f dims, f32 corner_roundness) {
	assert_true(state->buffer != null);
    
	R2D_Quad *quad = r2d_texture_clipped(&state->buffer->ui_quads, texture.handle, p,
										 dims, texture.clip_p, texture.clip_dims, v4f_make(1.0f, 1.0f, 1.0f, 1.0f));
  quad->corner_roundness = corner_roundness;
	return(quad);
}

// TODO(christian): man ..  should i allocate a big block at the beginning instead of doing this?
inl void
ui_initialize(UI_State *state, OS_Input *input, R2D_Buffer *buffer) {
    clear_struct(state);
    state->widget_arena = arena_reserve(align_a_to_b(1024 * sizeof(UI_Widget), 4096));
    state->util_arena = arena_reserve(kb(4));
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
        result->left_sibling = null;
        result->right_sibling = null;
        result->leftmost_child = null;
        result->rightmost_child = null;
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
        
        result->flags = widget_flags;
        result->last_frame_touched_index = state->current_frame_index;
        result->key = widget_key;
        result->identifier_string = str8_copy(state->util_arena, identifier);
        result->content_string = ui_string_as_content(result->identifier_string);
        result->bound_texture.handle = r2d_bad_handle();
        if (result->flags & UIWidget_TextContent) {
            result->semantic_size[UIAxis_X].type = UISizeType_TextContent;
            result->semantic_size[UIAxis_X].value = ui_peek_text_padding_x_top(state); // 0 pixels padding on x
            result->semantic_size[UIAxis_Y].type = UISizeType_TextContent;
            result->semantic_size[UIAxis_Y].value = ui_peek_text_padding_y_top(state); // 0 pixels padding on y
            
            result->text_dims = r2d_get_text_dims(&state->buffer->font, result->content_string);
            //result->text_dims.y = state->buffer->font.max_glyph_height;
            result->text_dims.y = (f32)(state->buffer->font.ascent + state->buffer->font.descent);
        }
    }
    
    result->corner_roundness = ui_peek_corner_roundness_top(state);
    result->edge_thickness = ui_peek_edge_thickness_top(state);
    result->border_colour.tl = ui_peek_tl_border_colour_top(state);
    result->border_colour.tr = ui_peek_tr_border_colour_top(state);
    result->border_colour.bl = ui_peek_bl_border_colour_top(state);
    result->border_colour.br = ui_peek_br_border_colour_top(state);
    result->bg_colour.tl = ui_peek_tl_background_colour_top(state);
    result->bg_colour.tr = ui_peek_tr_background_colour_top(state);
    result->bg_colour.bl = ui_peek_bl_background_colour_top(state);
    result->bg_colour.br = ui_peek_br_background_colour_top(state);
    result->text_colour = ui_peek_text_colour_top(state);
    result->anchor[UIAxis_X] = ui_peek_anchor_x_top(state);
    result->anchor[UIAxis_Y] = ui_peek_anchor_y_top(state);
    result->offset[UIAxis_X] = ui_peek_offset_x_top(state);
    result->offset[UIAxis_Y] = ui_peek_offset_y_top(state);
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

// NOTE(christian): how does this work? Lets define it.
// If the widget (a container) has children, then we check recursively
// for interactions (click) from children. If it does, then return an emtpy
// struct.
// otherwise, we check if we clicked or hover the widget.
fun UI_Interact
ui_interact_from_widget(UI_State *state, UI_Widget *input) {
    UI_Interact result = { 0 };
    
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
                result.released = (u8)(os_mouse_released(state->input, OSInputMouse_Left));
                result.pressed = (u8)(os_mouse_pressed(state->input, OSInputMouse_Left));
                result.held = (u8)(os_mouse_held(state->input, OSInputMouse_Left));
            }
            
            if (input->flags & UIWidget_HotAnimation) {
                ui_set_hot(state, input->key);
                state->misc_flags |= UIMiscFlag_HotThisPass;
            }
            
            if (input->flags & UIWidget_ActiveAnimation) {
                if (result.held) {
                    ui_set_active(state, input->key);
                    state->misc_flags |= UIMiscFlag_ActiveThisPass;
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
    layout->semantic_size[UIAxis_X].type = UISizeType_ChildrenSum;
    //layout->semantic_size[UIAxis_X].value = gap_x_in_px;
    layout->semantic_size[UIAxis_Y].type = UISizeType_ChildrenSum;
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
    
    layout->semantic_size[UIAxis_X].type = UISizeType_ChildrenSum;
    //layout->semantic_size[UIAxis_X].value = gap_x_in_px;
    layout->semantic_size[UIAxis_Y].type = UISizeType_ChildrenSum;
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
ui_do_button_tex(UI_State *state, String_U8_Const identifier,
				 R2D_Handle texture, v2f clip_p, v2f clip_dims,
				 v2f dims) {
  UI_Widget *button = ui_create_widget(state,
                                         UIWidget_TextContent |
                                         UIWidget_Clickable |
                                         UIWidget_HotAnimation |
                                         UIWidget_ActiveAnimation |
                                         UIWidget_BorderColour |
                                         UIWidget_BackgroundColour,
                                         identifier);
    
	button->semantic_size[UIAxis_X].type = UISizeType_Pixels;
	button->semantic_size[UIAxis_X].value = dims.x;
	button->semantic_size[UIAxis_Y].type = UISizeType_Pixels;
	button->semantic_size[UIAxis_Y].value = dims.y;
    
	UI_ClippedTexture *clipped_tex = &(button->bound_texture);
	clipped_tex->handle = texture;
	clipped_tex->clip_p = clip_p;
	clipped_tex->clip_dims = clip_dims;
    
	return ui_interact_from_widget(state, button);
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
ui_do_label_tex(UI_State *state, String_U8_Const identifier, R2D_Handle texture,
                v2f clip_p, v2f clip_dims, v2f dims) {
    UI_Widget *label = ui_create_widget(state,
                                        UIWidget_TextContent |
                                        UIWidget_HotAnimation |
                                        UIWidget_ActiveAnimation |
                                        UIWidget_BorderColour |
                                        UIWidget_BackgroundColour,
                                        identifier);
    
	  label->semantic_size[UIAxis_X].type = UISizeType_Pixels;
	  label->semantic_size[UIAxis_X].value = dims.x;
    label->semantic_size[UIAxis_Y].type = UISizeType_Pixels;
	  label->semantic_size[UIAxis_Y].value = dims.y;
    
  	UI_ClippedTexture *clipped_tex = &(label->bound_texture);
	  clipped_tex->handle = texture;
  	clipped_tex->clip_p = clip_p;
	  clipped_tex->clip_dims = clip_dims;
    
	  return ui_interact_from_widget(state, label);
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

fun UI_Interact
ui_do_titlef(UI_State *state, String_U8_Const string, ...) {
    Memory_Arena *scratch = arena_get_scratch(0, 0);
    Temporary_Memory temp = temp_mem_begin(scratch);
    
    va_list args;
    va_start(args, string);
    String_U8 format = str8_format_va(scratch, string, args);
    va_end(args);
    
    UI_Widget *title = ui_create_widget(state, UIWidget_TextContent, format);
    
    title->text_colour = rgba_make(1.0f, 1.0f, 1.0f, 1.0f);
    title->corner_roundness = 5.0f;
    title->edge_thickness = 1.0f;
    title->flags |= UIWidget_CenterOnParentAxis;
    
    temp_mem_end(temp);
    
    return ui_interact_from_widget(state, title);
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
    
    progression_widget->semantic_size[direction].type = UISizeType_Pixels;
    progression_widget->semantic_size[direction].value = size;
    progression_widget->semantic_size[!direction].type = UISizeType_Pixels;
    progression_widget->semantic_size[!direction].value = thickness;
    
    return ui_interact_from_widget(state, progression_widget);
}

// NOTE(christian): the size depends on the direction. If direction is X-Axis,
// Then size corresponds to width. If direction is Y-Axis, then size corresponds
// to height. 
// Likewise, the thickness depends on the direction. If direction is X-Axis,
// it refers to height. If direction is Y-Axis, then thickness refers to width.

fun void
ui_begin(UI_State *state, f32 elapsed_time_secs) {
    // TODO(christian): fix this
    //u32 view_width, view_height;
    //R2D_GetViewportDims(&view_width, &view_height);
    u32 view_width;
    u32 view_height;
	r2d_get_viewport_dims(state->buffer, &view_width, &view_height);
    
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
    
    //if (!(state->misc_flags & UIMiscFlag_HotThisPass)) {}
    ui_set_hot(state, ui_zero_key());
    
    //if (!(state->misc_flags & UIMiscFlag_ActiveThisPass)) {}
    ui_set_active(state, ui_zero_key());
    
    //state->misc_flags &= ~(UIMiscFlag_HotThisPass | UIMiscFlag_ActiveThisPass);
    
    UI_Widget *root = ui_create_widget(state, 0, str8_format(state->util_arena, str8("main_container_root_of_all_widgets")));
    root->semantic_size[UIAxis_X].type = UISizeType_Pixels;
    root->semantic_size[UIAxis_X].value = (f32)(view_width);
    root->semantic_size[UIAxis_Y].type = UISizeType_Pixels;
    root->semantic_size[UIAxis_Y].value = (f32)(view_height);
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
    switch (root->semantic_size[axis].type) {
        case UISizeType_Pixels: {
            root->final_size.v[axis] = root->semantic_size[axis].value;
        } break;
        
        // NOTE(christian): the semantic size of text is padding
        case UISizeType_TextContent: {
            root->final_size.v[axis] = root->text_dims.v[axis] + root->semantic_size[axis].value * 2.0f;
        } break;
        
        case UISizeType_PercentOfParent: {
            
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
    if (root->semantic_size[axis].type == UISizeType_ChildrenSum) {
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
                } break;
                
				case UIOffsetType_Absolute: {
					// absolute: do nothing.
				} break;
                
                default: {
                    invalid_code_path();
                } break;
            }
            
            if (child->right_sibling) {
                p_accumulate += child->final_size.v[axis] + gap + root->edge_thickness * 0.5f;
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
            
            // TODO(christian): REMOVE THIS
            // NOTE(christian): final p's and sizes
            if (child->flags & UIWidget_CenterOnParentAxis) {
                //child->rel_p_to_parent.v[axis] += root->children_padding.v[axis] * 0.5f;
                child->rel_p_to_parent.v[axis] += (root->final_size.v[axis] - child->final_size.v[axis]) * 0.5f;
            } else {
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
        			ui_rect(state, root->rect.p, dims, bg_colour, root->corner_roundness, 0.0f);
				}
                
				if ((root->flags & UIWidget_BorderColour) && (root->edge_thickness >= 1.0f)) {
					UI_RectColours border_colour = root->border_colour;
					ui_scale_colours(&border_colour, colour_multiplier);
                    
					ui_rect(state, v2f_sub(root->rect.p, v2f_scale(v2f_make(root->edge_thickness, root->edge_thickness), 0.5f)),
							v2f_add(root->rect.dims, v2f_make(root->edge_thickness, root->edge_thickness)), border_colour,
                            root->corner_roundness, root->edge_thickness);
				}
			} break;
		}
	} else if (!r2d_handles_match(root->bound_texture.handle, r2d_bad_handle())) {
		if (root->flags & UIWidget_BackgroundColour) {
			UI_RectColours bg_colour = root->bg_colour;
			ui_scale_colours(&bg_colour, colour_multiplier);
			ui_rect(state, root->rect.p, root->rect.dims, bg_colour, root->corner_roundness, 0.0f);
		}
        
		ui_texture(state, root->bound_texture, root->rect.p, root->rect.dims, 0);
        
		if ((root->flags & UIWidget_BorderColour) && (root->edge_thickness >= 1.0f)) {
			UI_RectColours border_colour = root->border_colour;
			ui_scale_colours(&border_colour, colour_multiplier);
            
			ui_rect(state, v2f_sub(root->rect.p, v2f_scale(v2f_make(root->edge_thickness, root->edge_thickness), 0.5f)),
					v2f_add(root->rect.dims, v2f_make(root->edge_thickness, root->edge_thickness)), border_colour,
                    root->corner_roundness, root->edge_thickness);
		}
	} else {
		if (root->flags & UIWidget_BackgroundColour) {
			UI_RectColours bg_colour = root->bg_colour;
			ui_scale_colours(&bg_colour, colour_multiplier);
			
			ui_rect(state, root->rect.p, root->rect.dims, bg_colour, root->corner_roundness, 0.0f);
		}
		
		if ((root->flags & UIWidget_BorderColour) &&
			(root->edge_thickness >= 1.0f)) {
			UI_RectColours border_colour = root->border_colour;
			ui_scale_colours(&border_colour, colour_multiplier);
			
			ui_rect(state, v2f_sub(root->rect.p, v2f_scale(v2f_make(root->edge_thickness, root->edge_thickness), 0.5f)),
					v2f_add(root->rect.dims, v2f_make(root->edge_thickness, root->edge_thickness)), border_colour,
					root->corner_roundness, root->edge_thickness);
		}
		
		if (root->flags & UIWidget_TextContent) {
			v2f final_p = v2f_add(root->rect.p, v2f_scale(v2f_sub(root->rect.dims, root->text_dims), 0.5f));
			ui_text(state, final_p, root->text_colour, root->content_string);
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
    
    ui_draw_widgets(state, state->root);
    
    state->parent_count = 0;
    arena_clear(state->util_arena);
}

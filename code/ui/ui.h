/* date = February 3rd 2024 7:26 pm */

#ifndef UI_H
#define UI_H

typedef u32 UI_WidgetFlag;
enum {
    UIWidget_TextContent = 1 << 0,
    UIWidget_Clickable = 1 << 1,
    UIWidget_HotAnimation = 1 << 2,
    UIWidget_ActiveAnimation = 1 << 3,
    UIWidget_BorderColour = 1 << 4,
    UIWidget_BackgroundColour = 1 << 5,
    UIWidget_CenterOnParentAxis = 1 << 6,
    UIWidget_FocusOnClick = 1 << 7,
};

typedef u8 UI_SizeType;
enum {
    UISizeType_Null,
    UISizeType_Pixels, // encodes directly the size in pixels
    UISizeType_TextContent, // size depends on the string 
    UISizeType_PercentOfParent, // a certain percentage of the parent's widget size 
    UISizeType_ChildrenSum, // size is sum of all childrem
};

typedef u8 UI_MetricType;
enum {
    UIMetricType_Pixels,
    UIMetricType_Percentage,
    UIMetricType_Count
};

typedef u8 UI_AxisType;
enum {
    UIAxis_X,
    UIAxis_Y,
    UIAxis_Count,
};

typedef struct {
    UI_MetricType metric;
    f32 value;
} UI_Anchor;

// TODO(christian): possible extension.
// Absolute positioning.
// Fixed Positioning.
// Relative Positioning (What we are doing by default)
// Just like CSS
typedef u8 UI_OffsetType;
enum {
    UIOffsetType_Relative,
    UIOffsetType_Absolute,
};

typedef struct {
    UI_OffsetType type;
    UI_MetricType metric;
    f32 value;
} UI_Offset;

typedef struct {
    UI_SizeType type;
    f32 value;
} UI_SemanticSize;

typedef u8 UI_ProgressionType;
enum {
    UIProgression_Null,
    UIProgression_Gradient,
    UIProgression_CurrentAndMaxPair, // example: 55 / 100 (current is 55, max is 100).
    UIProgression_Count,
};

typedef u8 UI_ProgressionFlags;
enum {
	UIProgressionFlag_ClipBackgroundColour = (1 << 0),
	//UIProgression_ClipBorderColour = (1 << 1),
};

typedef struct {
	UI_ProgressionFlags flags;
    UI_ProgressionType type;
    UI_AxisType direction;
    f32 current;
    f32 max;
} UI_Progression;

typedef struct {
    u64 key;
} UI_Key;

typedef struct {
    u8 released;
    u8 pressed;
    u8 held;
    u8 hover;
} UI_Interact;

typedef union {
    struct {
        v4f tl;
        v4f tr;
        v4f bl;
        v4f br;
    };
    v4f c[4];
} UI_RectColours;

typedef struct {
	R2D_Handle handle;
	v2f clip_p;
	v2f clip_dims;
} UI_ClippedTexture;

typedef struct UI_Widget UI_Widget;
struct UI_Widget {
    // NOTE(christian): hierarchy
    UI_Widget *parent; // NOTE(christian): mom and dad
    // NOTE(christian) brothas and sistas
    UI_Widget *left_sibling;
    UI_Widget *right_sibling;
    // NOTE(christian): sons and daughters
    UI_Widget *leftmost_child;
    UI_Widget *rightmost_child;
    
    union {
        // NOTE(christian): hash_for_cache
        UI_Widget *next_in_hash;
        UI_Widget *next_in_free; // or free list
    };
    
    UI_Key key; // key for hash
    String_U8 identifier_string; // the entiriety of the string including triple pounds if any
    String_U8_Const content_string; // the contents of the identifier string without triple pounts
    u64 last_frame_touched_index;
    
    // NOTE(christian): the input to the auto layout algorithm
    UI_SemanticSize semantic_size[UIAxis_Count];
    UI_Anchor anchor[UIAxis_Count];
    UI_Offset offset[UIAxis_Count];
    
    v2f rel_p_to_parent;
    v2f final_size;
    v2f text_dims;
    // NOTE(christian): if we want to make this a container!
    // // NOTE(christian): we could make container semantic size the min dims!
    UI_AxisType children_layout_direction;
    v2f children_padding;
    v2f children_gap;
    
    // NOTE(christian): feature
    UI_Progression progression;
	UI_ClippedTexture bound_texture;
    
    // NOTE(christian): output of the autolayout algorithm.
    // NOTE(christian): used for input consumption. i.e. check if mouse in box / cache
    RectF32 rect;
    Font_Size font_size;
    
    f32 corner_roundness;
    f32 edge_thickness;
    
    UI_RectColours border_colour;
    UI_RectColours bg_colour;
    v4f text_colour;
    
    f32 hot;
    f32 active;
    
    UI_WidgetFlag flags;
};

#define ui_cap_per_stack 32

#define ui_define_property_stack(type,name) \
u64 name##_##count;\
type name##_##stack[ui_cap_per_stack]\

#define ui_define_property_stack_function(type,name) \
inl void ui##_##push##_##name(UI_State *state, type name) {\
assert_true((state->##name##_##count) < ui_cap_per_stack);\
(state->##name##_##stack)[(state->##name##_##count)++] = name;\
}\
inl type ui##_##pop##_##name(UI_State *state) {\
assert_true((state->##name##_##count) > 0);\
type result = (state->##name##_##stack)[--(state->##name##_##count)];\
return(result);\
}\
inl type ui##_##peek##_##name##_##top(UI_State *state) {\
assert_true((state->##name##_##count) > 0);\
type result = (state->##name##_##stack)[(state->##name##_##count) - 1];\
return(result);\
}\

// TODO(christian): problem with current UI system.
// Problem: I want to place a widget exactly where I want them to be
//          relative to its parent's.
// Example: I want to place container widget x-axis centered on parent widget,
//          y-axis completely at the bottom of the parent widget
// Notes: we have not specified how to center our widget. We have different 
// types of widget centering. We call this UI_WidgetAnchor. We also introduce
// types: Pixels, Percentage. If we achor with pixels, we offset the widget with
// that pixel amount. If we anchor with percentage, we take a percentage of 
// the parents size and add it to the widgets position.
typedef struct {
    Memory_Arena *widget_arena;
    // we copy strings / do some temp stuff per frame, place this here. we then reset this after one frame.
    Memory_Arena *util_arena;
    
    UI_Widget *root;
    UI_Widget *cache[128];
    u64 current_frame_index;
    
    UI_Widget *free_widgets;
    
    u32 misc_flags;
    // NOTE(christian): thanks casey
    UI_Key hot_key;
    UI_Key active_key;
    
    UI_Key focus_hot_key;
    UI_Key focus_active_key;
    
    f32 elapsed_time_secs;
    
    OS_Input *input;
    R2D_Buffer *buffer;
    
    // NOTE(christian): stacks. How the heck can I automate this
    ui_define_property_stack(UI_Widget *, parent);
    ui_define_property_stack(f32, corner_roundness);
    ui_define_property_stack(f32, edge_thickness);
    ui_define_property_stack(v4f, tl_border_colour);
    ui_define_property_stack(v4f, tr_border_colour);
    ui_define_property_stack(v4f, bl_border_colour);
    ui_define_property_stack(v4f, br_border_colour);
    ui_define_property_stack(v4f, tl_background_colour);
    ui_define_property_stack(v4f, tr_background_colour);
    ui_define_property_stack(v4f, bl_background_colour);
    ui_define_property_stack(v4f, br_background_colour);
    ui_define_property_stack(v4f, text_colour);
    ui_define_property_stack(UI_Anchor, anchor_x);
    ui_define_property_stack(UI_Anchor, anchor_y);
    ui_define_property_stack(UI_Offset, offset_x);
    ui_define_property_stack(UI_Offset, offset_y);
    ui_define_property_stack(f32, text_padding_x);
    ui_define_property_stack(f32, text_padding_y);
    ui_define_property_stack(Font_Size, font_size);
} UI_State;

inl UI_Key ui_zero_key(void);
inl b32 ui_match_keys(UI_Key a, UI_Key b);

inl UI_RectColours ui_rect_colours(v4f tl, v4f tr, v4f bl, v4f br);
inl f32 ui_metric_to_pixels(UI_MetricType metric);
inl UI_Anchor ui_make_anchor(UI_MetricType metric, f32 value);
inl UI_Offset ui_make_offset(UI_OffsetType type, UI_MetricType metric, f32 value);

inl void ui_initialize(UI_State *state, OS_Input *input, R2D_Buffer *buffer);

#define ui_rect_colour(colour) ui_rect_colours(colour, colour, colour, colour)
#define ui_pop_layout(state) ui_pop_parent(state)

// TODO(christian): implement this!
// first param: the widget itself
// second param: user-bound data to calculate how hot and active works!
typedef void ui_custom_hot_animation(UI_Widget *, void *);
typedef void ui_custom_active_animation(UI_Widget *, void *);

fun UI_Widget *ui_create_widget(UI_State *state, UI_WidgetFlag widget_flags, String_U8_Const identifier);
inl void ui_push_hori_layout(UI_State *state, String_U8_Const identifier, v2f padding, v2f gap);
inl void ui_push_vert_layout(UI_State *state, String_U8_Const identifier, v2f padding, v2f gap);
inl b32 ui_is_hot(UI_State *state, UI_Key key);
inl b32 ui_is_active(UI_State *state, UI_Key key);
inl b32 ui_is_focus_hot(UI_State *state, UI_Key key);
inl b32 ui_is_focus_active(UI_State *state, UI_Key key);

// TODO: how do we implement textured stuff nicely? Its not a good idea
// to just add a *_tex variant for each widget! We might need a notion of a texture stack!
fun UI_Interact ui_do_button(UI_State *state, String_U8_Const string);
fun UI_Interact ui_do_buttonf(UI_State *state, String_U8_Const string, ...);
fun UI_Interact ui_do_button_tex(UI_State *state, String_U8_Const identifier, R2D_Handle texture, v2f clip_p, v2f clip_dims, v2f dims);
fun UI_Interact ui_do_label(UI_State *state, String_U8_Const string);
fun UI_Interact ui_do_labelf(UI_State *state, String_U8_Const string, ...);
fun UI_Interact ui_do_label_tex(UI_State *state, String_U8_Const identifier, R2D_Handle texture, v2f clip_p, v2f clip_dims, v2f dims);
// TODO: Remove this title. We have anchors / offsets anyway.
fun UI_Interact ui_do_titlef(UI_State *state, String_U8_Const string, ...);
fun UI_Interact ui_do_int_input(UI_State *state, String_U8 *string_to_edit, u64 *cursor_idx);

// TODO(christian): this ui_do_progression is quite iffy.
// Well, I could have just put the progression state in the gamestate
// and the game will use the UI API to create the progression.
// The border should be the container, the progression is a UI_Widget
// with a fixed size. Thus, we must add another property stack for UI_Size!
// NOTE(christian): If direction is X-Axis
// size = width
// thickness = height

// NOTE(christian): If direction is Y-Axis
// size = height
// thickness = width
// the background colour is the progression.
fun UI_Interact ui_do_progression(UI_State *state, String_U8_Const identifier,
                                  UI_ProgressionType type, UI_ProgressionFlags flags,
                                  UI_AxisType direction, f32 initial, f32 max,
								  f32 size, f32 thickness);

// NOTE(christian): UI properties
ui_define_property_stack_function(UI_Widget *, parent);
ui_define_property_stack_function(f32, corner_roundness);
ui_define_property_stack_function(f32, edge_thickness);
ui_define_property_stack_function(v4f, tl_border_colour);
ui_define_property_stack_function(v4f, tr_border_colour);
ui_define_property_stack_function(v4f, bl_border_colour);
ui_define_property_stack_function(v4f, br_border_colour);
ui_define_property_stack_function(v4f, tl_background_colour);
ui_define_property_stack_function(v4f, tr_background_colour);
ui_define_property_stack_function(v4f, bl_background_colour);
ui_define_property_stack_function(v4f, br_background_colour);
ui_define_property_stack_function(v4f, text_colour);
ui_define_property_stack_function(UI_Anchor, anchor_x);
ui_define_property_stack_function(UI_Anchor, anchor_y);
ui_define_property_stack_function(UI_Offset, offset_x);
ui_define_property_stack_function(UI_Offset, offset_y);
ui_define_property_stack_function(f32, text_padding_x);
ui_define_property_stack_function(f32, text_padding_y);
ui_define_property_stack_function(Font_Size, font_size);

inl void
ui_push_border_colour(UI_State *state, v4f colour) {
    ui_push_tl_border_colour(state, colour);
    ui_push_tr_border_colour(state, colour);
    ui_push_bl_border_colour(state, colour);
    ui_push_br_border_colour(state, colour);
}

inl v4f
ui_pop_border_colour(UI_State *state) {
    v4f result = ui_pop_tl_border_colour(state);
    ui_pop_tr_border_colour(state);
    ui_pop_bl_border_colour(state);
    ui_pop_br_border_colour(state);
    return(result);
}

inl void
ui_push_background_colour(UI_State *state, v4f colour) {
    ui_push_tl_background_colour(state, colour);
    ui_push_tr_background_colour(state, colour);
    ui_push_bl_background_colour(state, colour);
    ui_push_br_background_colour(state, colour);
}

inl v4f
ui_pop_background_colour(UI_State *state) {
    v4f result = ui_pop_tl_background_colour(state);
    ui_pop_tr_background_colour(state);
    ui_pop_bl_background_colour(state);
    ui_pop_br_background_colour(state);
    return(result);
}

#define ui_push_transparent_background(state) ui_push_background_colour(state, rgba_make(0.0f, 0.0f, 0.0f, 0.0f))
#define ui_pop_transparent_background(state) ui_pop_background_colour(state)

#define ui_push_transparent_border(state) ui_push_border_colour(state, rgba_make(0.0f, 0.0f, 0.0f, 0.0f))
#define ui_pop_transparent_border(state) ui_pop_border_colour(state)

inl void
ui_push_offset(UI_State *state, UI_Offset uniform_offset) {
    ui_push_offset_x(state, uniform_offset);
    ui_push_offset_y(state, uniform_offset);
}

inl UI_Offset
ui_pop_offset(UI_State *state) {
    UI_Offset result = ui_pop_offset_x(state);
    ui_pop_offset_y(state);
    return(result);
}

inl void
ui_push_anchor(UI_State *state, UI_Anchor uniform_anchor) {
    ui_push_anchor_x(state, uniform_anchor);
    ui_push_anchor_y(state, uniform_anchor);
}

inl UI_Anchor
ui_pop_anchor(UI_State *state) {
    UI_Anchor result = ui_pop_anchor_x(state);
    ui_pop_anchor_y(state);
    return(result);
}

inl void
ui_push_text_padding(UI_State *state, f32 padding_pixels) {
    ui_push_text_padding_x(state, padding_pixels);
    ui_push_text_padding_y(state, padding_pixels);
}

inl f32
ui_pop_text_padding(UI_State *state) {
    f32 result = ui_pop_text_padding_x(state);
    ui_pop_text_padding_y(state);
    return(result);
}

fun void ui_begin(UI_State *state, f32 elapsed_time_secs);
fun void ui_end(UI_State *state);

#endif //UI_H

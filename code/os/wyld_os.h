/* date = February 2nd 2024 10:53 pm */

#ifndef WYLD_OS_H
#define WYLD_OS_H

typedef u8 OS_Input_Interact_Flag;
enum {
    OSInputInteract_Released = 1 << 0,
    OSInputInteract_Pressed = 1 << 1,
    OSInputInteract_Held = 1 << 2,
};

typedef u16 OS_Input_Key_Type;
enum {
    OSInputKey_Backspace,
    OSInputKey_Escape,
    
    OSInputKey_0 = '0', OSInputKey_1, OSInputKey_2, OSInputKey_3,
    OSInputKey_4, OSInputKey_5, OSInputKey_6, OSInputKey_7,
    OSInputKey_8, OSInputKey_9,
    
    OSInputKey_A = 'A', OSInputKey_B, OSInputKey_C, OSInputKey_D, 
    OSInputKey_E, OSInputKey_F, OSInputKey_G, OSInputKey_H,
    OSInputKey_I, OSInputKey_J, OSInputKey_K, OSInputKey_L,
    OSInputKey_M, OSInputKey_N, OSInputKey_O, OSInputKey_P,
    OSInputKey_Q, OSInputKey_R, OSInputKey_S, OSInputKey_T,
    OSInputKey_U, OSInputKey_V, OSInputKey_W, OSInputKey_X,
    OSInputKey_Y, OSInputKey_Z,
    OSInputKey_Total,
};

typedef u8 OS_Input_Mouse_Type;
enum {
    OSInputMouse_Left,
    OSInputMouse_Right,
    OSInputMouse_Middle,
    OSInputMouse_Total,
};

typedef u8 OS_Input_Misc_Flag;
enum {
    OSInputMisc_Quit = 1 << 0
};

typedef struct {
    u32 key_states[OSInputKey_Total];
    u32 mouse_states[OSInputMouse_Total];
    u8 misc;
    
    u32 mouse_x, mouse_y;
} OS_Input;

typedef union {
    u64 h64[1];
} OS_Handle;

typedef struct {
	u32 client_width;
    u32 client_height;
	b32 resized_this_frame;
    OS_Input input;
} OS_Window;

typedef s32 ThreadFunc(void *);

inl OS_Handle os_bad_handle(void);
inl b32 os_match_handle(OS_Handle a, OS_Handle b);

fun b32 os_init(void);
fun OS_Window *os_create_window(String_U8_Const window_title, u32 width, u32 height);
fun void os_message_box(String_U8_Const title, String_U8_Const message);
fun u64 os_get_window_refresh_rate(OS_Window *window);

//~ NOTE(christian): time
inl u64 os_get_ticks(void);
inl f64 os_secs_between_ticks(u64 start, u64 end);

//~ NOTE(christian): mem
inl void *os_mem_reserve(u64 size_in_bytes);
inl b32 os_mem_commit(void *memory, u64 size_in_bytes);
inl b32 os_mem_decommit(void *memory, u64 size_in_bytes);
inl b32 os_mem_release(void *block);

//~ NOTE(christian): thread/proc
typedef u16 OS_Wait_Result;
enum {
    OSWaitResult_IsFree, // signaled
    OSWaitResult_IsOccupied, // nonsignaled
    OSWaitResult_Abandoned,
    OSWaitResult_Timeout,
    OSWaitResult_Failed,
};

#define OSWait_Infinite 0xFFFFFFFF

inl void os_sleep(u32 ms);
inl OS_Handle os_thread_create(ThreadFunc thread_func, b32 run_immediately, void *param);
inl b32 os_thread_resume(OS_Handle thread_handle);
inl b32 os_thread_close(OS_Handle thread_handle);
inl b32 os_exit_thread(s32 code);

inl OS_Wait_Result os_wait_for_object(OS_Handle handle, u32 wait_ms);
inl OS_Handle os_semaphore_create(u32 initial_count, u32 max_count, char *name);
inl b32 os_semaphore_release(OS_Handle handle, u32 release_count, u32 *previous_count);

inl void os_exit_process(s32 code);
inl s32 os_get_pid(void);

inl s32 os_interlocked_compare_exchange_s32(s32 volatile *dest, s32 new, s32 comparand);
inl s32 os_interlocked_increment_s32(s32 volatile *addend);
inl s32 os_interlocked_exchange_s32(s32 volatile *dest, s32 source);

// Shared libary run time stuff
inl OS_Handle os_library_load(char *shared_lib_name);
inl b32 os_library_release(OS_Handle handle);
inl void *os_library_get_proc(OS_Handle shared_lib_handle, char *proc_name);

//~NOTE(christian): Input
inl void os_fill_events(OS_Window *window);
inl b32 os_get_misc_input_flag(OS_Input *input, OS_Input_Misc_Flag flag);

inl b32 os_key_released(OS_Input *input, OS_Input_Key_Type type);
inl b32 os_key_pressed(OS_Input *input, OS_Input_Key_Type type);
inl b32 os_key_held(OS_Input *input, OS_Input_Key_Type type);

inl b32 os_mouse_released(OS_Input *input, OS_Input_Mouse_Type type);
inl b32 os_mouse_pressed(OS_Input *input, OS_Input_Mouse_Type type);
inl b32 os_mouse_held(OS_Input *input, OS_Input_Mouse_Type type);

inl v2f os_get_mouse_v2f(OS_Input *input);

#endif //WYLD_OS_H

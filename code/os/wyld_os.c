#include "impl/os_w32.c"
#include "impl/os_w32.h"

inl OS_Handle
os_bad_handle(void) {
    OS_Handle handle;
    handle.h64[0] = 0xFFFFFFFFFFFFFFFF;
    return(handle);
}

inl b32
os_match_handle(OS_Handle a, OS_Handle b) {
    b32 result = a.h64[0] == b.h64[0];
    return(result);
}

inl v2f
os_get_mouse_v2f(OS_Input *input) {
    v2f result;
    result.x = (f32)input->mouse_x;
    result.y = (f32)input->mouse_y;
    
    return(result);
}
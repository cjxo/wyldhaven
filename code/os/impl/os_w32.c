#define WIN32_LEAN_AND_MEAN
#define COBJMACROS
#include <windows.h>
#include <d3d11.h>
#include <d3d11_1.h>
#include <dxgi.h>
#include <dxgi1_3.h>
#include <d3dcompiler.h>
#include <dxgidebug.h>
#include <timeapi.h>

#define w32_window_class_name "windows_windows_windows_windows_HWND"

typedef struct {
	OS_Window base_window;
  HWND handle;
} W32_Window;

glb u32 w32_window_count;
glb Memory_Arena *w32_memory_arena = null;

glb b32 w32_inited = false;
glb LARGE_INTEGER w32_perf_frequency;

inl HANDLE
w32_handle_from_os_handle(OS_Handle handle) {
  HANDLE result;
  if (os_match_handle(handle, os_bad_handle())) {
    result = null;
  } else {
    result = (HANDLE)(handle.h64[0]);
  }
  return(result);
}

inl OS_Handle
os_handle_from_w32_handle(HANDLE handle) {
  OS_Handle result;
  if (handle != null) {
    result.h64[0] = (u64)handle;
  } else {
    result = os_bad_handle();
  }
  
  return(result);
}

inl OS_Handle
os_handle_from_w32_hmodule(HMODULE module) {
  OS_Handle result;
  if (module != null) {
    result.h64[0] = (u64)module;
  } else {
    result = os_bad_handle();
  }
  return(result);
}

inl HMODULE
w32_hmodule_from_os_handle(OS_Handle handle) {
  HMODULE result;
  if (os_match_handle(handle, os_bad_handle())) {
    result = null;
  } else {
    result = (HMODULE)handle.h64[0];
  }
  
  return(result);
}

fun LRESULT __stdcall
w32_window_proc(HWND window_handle, UINT message, WPARAM wparam, LPARAM lparam) {
  LRESULT lresult = 0;
  
  OS_Window *window = (OS_Window *)GetWindowLongPtrA(window_handle, GWLP_USERDATA);
  // TODO(christian): REARCH WHY IS THIS NULL SOMETIMES
  {
    OS_Input *input = &window->input;
    
    switch (message) {
      case WM_CLOSE: {
        DestroyWindow(window_handle);
      } break;
      
      case WM_DESTROY: {
        PostQuitMessage(0);
      } break;
      
      case WM_SIZE: {
        if (window) {
					u32 new_client_width = (u32)LOWORD(lparam);
					u32 new_client_height = (u32)HIWORD(lparam);
          
					if ((new_client_width != window->client_width) ||
              (new_client_height != window->client_height)) {
						window->resized_this_frame = true;
					}
          
          window->client_width = new_client_width;
          window->client_height = new_client_height;
        }
      } break;
      
      case WM_LBUTTONDOWN: {
        input->mouse_states[OSInputMouse_Left] |= OSInputInteract_Pressed | OSInputInteract_Held;
      } break;
      
      case WM_LBUTTONUP: {
        input->mouse_states[OSInputMouse_Left] &= ~OSInputInteract_Held;
        input->mouse_states[OSInputMouse_Left] |= OSInputInteract_Released;
      } break;
      
      case WM_RBUTTONDOWN: {
        input->mouse_states[OSInputMouse_Right] |= OSInputInteract_Pressed | OSInputInteract_Held;
      } break;
      
      case WM_RBUTTONUP: {
        input->mouse_states[OSInputMouse_Right] &= ~OSInputInteract_Held;
        input->mouse_states[OSInputMouse_Right] |= OSInputInteract_Released;
      } break;
      
      default: {
        lresult = DefWindowProcA(window_handle, message, wparam, lparam);
      } break;
    }
  }
  
  return(lresult);
}

fun b32
os_init(void) {
  b32 success = false;
  
  if (!w32_inited) {
    WNDCLASS window_class = {
      .style = 0,
      .lpfnWndProc = &w32_window_proc,
      .cbClsExtra = 0,
      .cbWndExtra = 0,
      .hInstance = GetModuleHandleA(null),
      .hIcon = null,
      .hCursor = LoadCursorA(null, IDC_ARROW),
      .hbrBackground = null,
      .lpszMenuName = null,
      .lpszClassName = w32_window_class_name
    };
    
    if (RegisterClassA(&window_class)) {
      w32_memory_arena = arena_reserve(8llu * kb(4llu), Memory_Arena_Flag_CommitOrDecommitOnPushOrPop);
      
      //timeGetDevCaps();
      timeBeginPeriod(1);
      
      QueryPerformanceFrequency(&w32_perf_frequency);
      
      w32_inited = true;
      success = true;
    }
  }
  
  return(success);
}

fun OS_Window *
os_create_window(String_U8_Const window_title, u32 width, u32 height) {
  OS_Window *result = null;
  if (w32_inited) {
    RECT desired_rect = {
      .left = 0,
      .top = 0,
      .right = (LONG)(width),
      .bottom = (LONG)(height),
    };
    
    AdjustWindowRect(&desired_rect, WS_OVERLAPPEDWINDOW, FALSE);
    
    HWND window_handle = CreateWindowExA(0, w32_window_class_name, (char *)(window_title.string),
                                         WS_OVERLAPPEDWINDOW, 0, 0,
                                         desired_rect.right - desired_rect.left,
                                         desired_rect.bottom - desired_rect.top, null, null,
                                         GetModuleHandleA(null), null);
    
    assert_true(IsWindow(window_handle));
    
    ShowWindow(window_handle, SW_SHOW);
    
		W32_Window *w32_window = arena_push_struct(w32_memory_arena, W32_Window);
    result = (OS_Window *)w32_window;
    result->client_width = width;
    result->client_height = height;
		result->resized_this_frame = false;
    w32_window->handle = window_handle;
  }
  
  return(result);
}

fun void
os_message_box(String_U8_Const title, String_U8_Const message) {
  MessageBoxA(null, (char *)message.string,
              (char *)title.string, MB_OK);
}

fun u64
os_get_window_refresh_rate(OS_Window *window) {
  // TODO(christian): the user could be using more than one monitor.
  // figure out how to get the window from the window handle.
  unused(window);
  DEVMODE dev_mode;
  EnumDisplaySettingsA(null, ENUM_CURRENT_SETTINGS, &dev_mode);
  
  u64 result = dev_mode.dmDisplayFrequency;
  return(result);
}

//~ NOTE(christian): time
inl u64
os_get_ticks(void) {
  LARGE_INTEGER perf_counter;
  QueryPerformanceCounter(&perf_counter);
  
  return (u64)(perf_counter.QuadPart);
}

inl f64
os_secs_between_ticks(u64 start, u64 end) {
  f64 diff = (f64)(end - start);
  f64 result = diff * (1.0 / (f64)(w32_perf_frequency.QuadPart));
  
  return(result);
}

inl void *
os_mem_reserve(u64 size_in_bytes) {
  void *result = VirtualAlloc(null, size_in_bytes, MEM_RESERVE, PAGE_NOACCESS);
  return(result);
}

inl b32
os_mem_commit(void *memory, u64 size_in_bytes) {
  b32 result = VirtualAlloc(memory, size_in_bytes, MEM_COMMIT, PAGE_READWRITE) != null;
  return(result);
}

inl b32
os_mem_decommit(void *memory, u64 size_in_bytes) {
  b32 result = VirtualFree(memory, size_in_bytes, MEM_DECOMMIT) != FALSE;
  return(result);
}

inl b32
os_mem_release(void *block) {
  b32 result = VirtualFree(block, 0, MEM_RELEASE) != FALSE;
  return(result);
}

//~ NOTE(christian): thread/proc
inl void
os_sleep(u32 ms) {
  Sleep(ms);
}

inl OS_Handle
os_thread_create(ThreadFunc thread_func, b32 run_immediately, void *param) {
  OS_Handle result;
  DWORD creation_flags = run_immediately ? 0 : CREATE_SUSPENDED;
  HANDLE handle = CreateThread(null, 0, (LPTHREAD_START_ROUTINE)(thread_func),
                               param, creation_flags, null);
  
  result.h64[0] = (u64)(handle);
  
  return(result);
}

inl b32
os_thread_resume(OS_Handle thread_handle) {
  b32 result = ResumeThread(w32_handle_from_os_handle(thread_handle)) != ((DWORD)-1);
  return(result);
}

inl b32
os_thread_close(OS_Handle thread_handle) {
  b32 result = CloseHandle(w32_handle_from_os_handle(thread_handle)) != 0;
  return(result);
}

inl b32
os_exit_thread(s32 code) {
  ExitThread(code);
}

inl OS_Wait_Result
os_wait_for_object(OS_Handle handle, u32 wait_ms) {
  HANDLE w32_handle = w32_handle_from_os_handle(handle);
  DWORD w32_wait_result = WaitForSingleObject(w32_handle, wait_ms);
  
  OS_Wait_Result result = OSWaitResult_Failed;
  switch (w32_wait_result) {
    case WAIT_ABANDONED: {
      result = OSWaitResult_Abandoned;
    } break;
    
    case WAIT_OBJECT_0: {
      result = OSWaitResult_IsFree;
    } break;
    
    case WAIT_TIMEOUT: {
      result = OSWaitResult_Timeout;
    } break;
    
    case WAIT_FAILED: {
      result = OSWaitResult_Failed;
    } break;
    
    default: {
      invalid_code_path();
    } break;
  }
  
  return(result);
}

inl s32
os_interlocked_compare_exchange_s32(s32 volatile *dest, s32 new, s32 comparand) {
  s32 result = (s32)InterlockedCompareExchange((LONG volatile *)dest, new, comparand);
  return(result);
}

inl s32
os_interlocked_increment_s32(s32 volatile *addend) {
  s32 result = (s32)InterlockedIncrement((LONG volatile *)addend);
  return(result);
}

inl s32
os_interlocked_exchange_s32(s32 volatile *dest, s32 source) {
  s32 result = (s32)InterlockedExchange((LONG volatile *)dest, source);
  return(result);
}

inl OS_Handle
os_library_load(char *shared_lib_name) {
  HMODULE module = LoadLibraryA(shared_lib_name);
  OS_Handle result = os_handle_from_w32_hmodule(module);
  return(result);
}

inl b32
os_library_release(OS_Handle handle) {
  HMODULE module = w32_hmodule_from_os_handle(handle);
  b32 result = FreeLibrary(module) != 0;
  return(result);
}

inl void *
os_library_get_proc(OS_Handle shared_lib_handle, char *proc_name) {
  void *result = (void *)GetProcAddress(w32_hmodule_from_os_handle(shared_lib_handle), proc_name);
  return(result);
}

inl OS_Handle
os_semaphore_create(u32 initial_count, u32 max_count, char *name) {
  assert_false(initial_count > max_count);
  HANDLE handle = CreateSemaphoreExA(null, initial_count, max_count,
                                     name, 0, SEMAPHORE_ALL_ACCESS);
  
  OS_Handle result = os_handle_from_w32_handle(handle);
  
  return(result);
}

inl b32
os_semaphore_release(OS_Handle handle, u32 release_count, u32 *previous_count) {
  assert_true(release_count > 0);
  LONG lprevious_count;
  HANDLE w32_handle = w32_handle_from_os_handle(handle);
  b32 result = ReleaseSemaphore(w32_handle, release_count, &lprevious_count) != FALSE;
  if (previous_count) {
    *previous_count = (u32)(lprevious_count);
  }
  
  return(result);
}

inl void
os_exit_process(s32 code) {
  ExitProcess((u32)code);
}

inl s32
os_get_pid(void) {
  s32 result = (s32)GetCurrentProcessId();
  return(result);
}

fun OS_Input_Key_Type
w32_map_wparam_to_os_input_key_type(WPARAM wparam) {
  OS_Input_Key_Type result;
  
  switch (wparam) {
    case VK_ESCAPE: {
      result = OSInputKey_Escape;
    } break;
    
    case VK_BACK: {
      result = OSInputKey_Backspace;
    } break;
    
    default: {
      if (is_alpha_numeric((u8)(wparam))) {
        result = (OS_Input_Key_Type)(wparam);
      } else {
        result = OSInputKey_Total;
      }
    } break;
  }
  
  return(result);
}

//~NOTE(christian): Input
inl void
os_fill_events(OS_Window *window) {
  OS_Input *input = &window->input;
  for (u32 key_index = 0; key_index < OSInputKey_Total; key_index += 1) {
    input->key_states[key_index] &= ~(OSInputInteract_Pressed | OSInputInteract_Released);
  }
  
  for (u32 mouse_index = 0; mouse_index < OSInputMouse_Total; mouse_index += 1) {
    input->mouse_states[mouse_index] &= ~(OSInputInteract_Pressed | OSInputInteract_Released);
  }
  
	W32_Window *w32_window = (W32_Window *)window;
	window->resized_this_frame = false;
  SetWindowLongPtrA(w32_window->handle, GWLP_USERDATA, (LONG_PTR)window);
  
  MSG message;
  while (PeekMessageA(&message, null, 0, 0, PM_REMOVE) != 0) {
    switch (message.message) {
      case WM_QUIT: {
        input->misc |= OSInputMisc_Quit;
      } break;
      
      case WM_KEYDOWN: {
        OS_Input_Key_Type key = w32_map_wparam_to_os_input_key_type(message.wParam);
        if (key != OSInputKey_Total) {
          input->key_states[key] |= (OSInputInteract_Pressed | OSInputInteract_Held);
        }
      } break;
      
      case WM_KEYUP: {
        OS_Input_Key_Type key = w32_map_wparam_to_os_input_key_type(message.wParam);
        if (key != OSInputKey_Total) {
          input->key_states[key] |= OSInputInteract_Released;
          input->key_states[key] &= ~OSInputInteract_Held;
        }
      } break;
      
      default: {
        TranslateMessage(&message); 
        DispatchMessage(&message); 
      } break;
    }
  }
  
  
  POINT point;
  GetCursorPos(&point);
  ScreenToClient(w32_window->handle, &point);
  input->mouse_x = (s32)point.x;
  input->mouse_y = (s32)point.y;
}

inl b32
os_get_misc_input_flag(OS_Input *input, OS_Input_Misc_Flag flag) {
  b32 result = input->misc & flag;
  return(result);
}

inl b32
os_key_released(OS_Input *input, OS_Input_Key_Type type) {
  b32 result = input->key_states[type] & OSInputInteract_Released;
  return(result);
}

inl b32
os_key_pressed(OS_Input *input, OS_Input_Key_Type type) {
  b32 result = input->key_states[type] & OSInputInteract_Pressed;
  return(result);
}

inl b32
os_key_held(OS_Input *input, OS_Input_Key_Type type) {
  b32 result = input->key_states[type] & OSInputInteract_Held;
  return(result);
}

inl b32
os_mouse_released(OS_Input *input, OS_Input_Mouse_Type type) {
  b32 result = input->mouse_states[type] & OSInputInteract_Released;
  return(result);
}
;
inl b32
os_mouse_pressed(OS_Input *input, OS_Input_Mouse_Type type) {
  b32 result = input->mouse_states[type] & OSInputInteract_Pressed;
  return(result);
}

inl b32
os_mouse_held(OS_Input *input, OS_Input_Mouse_Type type) {
  b32 result = input->mouse_states[type] & OSInputInteract_Held;
  return(result);
}

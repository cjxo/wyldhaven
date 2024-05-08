/* date = February 2nd 2024 10:15 pm */

#ifndef WYLD_BASE_H
#define WYLD_BASE_H

#include <string.h>
#include <stdint.h>
#include <stdio.h>
#include <stdarg.h>
#include <intrin.h>
#include <float.h>

typedef    int8_t s8;
typedef   uint8_t u8;

typedef  int16_t s16;
typedef uint16_t u16;

typedef  int32_t s32;
typedef uint32_t u32;

typedef  int64_t s64;
typedef uint64_t u64;

typedef float f32;
typedef double f64;

typedef s32 b32;

#define null 0
#define true 1
#define false 0

#if defined(_WIN32)
# define OS_WINDOWS
#elif defined(__linux__)
# define OS_LINUX
#else
#error Unsupported OS
#endif

// #if defined(OS_WINDOWS)
#pragma section(".rdata$", read)
#define read_only static __declspec(allocate(".rdata$"))
// #endif

// #if defined(COMPILER_CL)
#define thread_var __declspec(thread)
#define dll_export __declspec(dllexport)
#define dll_import __declspec(dllimport)
// #endif

#define assert_break() (*(volatile int *)0 = 0)
#define stmnt(s) do{s}while(0)

#if defined(WC_DEBUG)
# define wc_assert(c) stmnt( if (!(c)) { assert_break(); } )
#else
# define wc_assert(c)
#endif

#define assert_true(c) wc_assert((c)==true)
#define assert_false(c) wc_assert((c)==false)
#define invalid_code_path() wc_assert(false)

#define fun static
#define glb static
#define loc static
#define inl inline

#define minimum(a,b) ((a)<(b)?(a):(b))
#define maximum(a,b) ((a)>(b)?(a):(b))
#define clamp(val, min, max) ((val)<(min)?(min):((val)>(max)?(max):(val)))
#define array_count(a) (sizeof(a)/sizeof((a)[0]))
#define unused(v) (void)v

#define copy_memory(d,s,sz) memcpy(d,s,sz)
#define copy_struct(d,s) copy_memory(d,s,Minimum(sizeof(*(d)),sizeof(*(s))))
#define clear_memory(d,sz) memset(d,'\0',sz)
#define clear_struct(s) clear_memory(s,sizeof(*(s)))

#define align_a_to_b(a,b) ((a)+((b)-1))&(~((b)-1))
#define kb(v) (1024llu*(v))
#define mb(v) (1024llu*kb(v))
#define gb(v) (1024llu*mb(v))

#define invalid_index_u32 0xFFFFFFFF
#define invalid_index_u64 0xFFFFFFFFFFFFFFFF

#define _stringify(s) #s
#define stringify(s) _stringify(s)
#define concat_(a,b) a##b
#define concat(a,b) concat_(a,b)

#define str8(s) str8_make_from_c_str(s,sizeof(s)-1)
// NOTE(christian): I Kinda like this
#define _str8(s) str8_make_from_c_str(#s,sizeof(#s)-1)

typedef struct {
    u8 *string;
    u64 char_count;
    u64 char_capacity;
} String_U8_Const;

typedef String_U8_Const String_U8;

typedef struct {
    u8 *memory;
    u64 commit_ptr;
    u64 stack_ptr;
    u64 capacity;
} Memory_Arena;

typedef struct {
    Memory_Arena *arena;
    u64 start_stack_ptr;
} Temporary_Memory;

//~ NOTE(christian): Tools
inl b32 is_alpha(u8 c);
inl b32 is_digit(u8 c);
inl b32 is_alpha_numeric(u8 c);

//~ NOTE(christian): str8
inl String_U8_Const str8_make_from_c_str(char *str, u64 char_count);
inl b32 str8_to_s32(String_U8_Const string, s32 *result);
fun String_U8 str8_format_va(Memory_Arena *arena, String_U8_Const string, va_list args0);
fun String_U8 str8_format(Memory_Arena *arena, String_U8_Const string, ...);
inl String_U8 str8_reserve(Memory_Arena *arena, u64 char_count);
fun String_U8_Const str8_substring_view(String_U8_Const source, u64 start, u64 end);
fun String_U8 str8_copy(Memory_Arena *arena, String_U8_Const source);
inl String_U8 str8_substring(Memory_Arena *arena, String_U8_Const source, u64 from, u64 to);
fun b32 str8_equal_strings(String_U8_Const a, String_U8_Const b);
fun u64 str8_compute_hash(u64 base, String_U8_Const string);
fun u64 str8_find_first_string(String_U8_Const string, String_U8_Const to_find, u64 offset_from_beginning_of_source);

//~ NOTE(christian): mem
#define arena_push_array(arena,type,count) (type *)arena_push(arena,sizeof(type)*(count))
#define arena_push_struct(arena,type) arena_push_array(arena,type,1)
fun Memory_Arena *arena_reserve(u64 size_in_bytes);
inl void arena_clear(Memory_Arena *arena);
fun void *arena_push(Memory_Arena *arena, u64 push_size);
fun b32 arena_pop(Memory_Arena *arena, u64 pop_size);
inl Temporary_Memory temp_mem_begin(Memory_Arena *arena);
inl void temp_mem_end(Temporary_Memory temp);
inl Memory_Arena *arena_get_scratch(Memory_Arena **conflict, u64 conflict_count);

//~ NOTE(christian): random
#define pcg_default_multiplier_64 6364136223846793005ULL
#define pcg_default_increment_64 1442695040888963407ULL

typedef struct {
    u64 state;
} PCG32_State;

inl fun u32 pcg32_next_u32(PCG32_State *pcg);
inl fun u32 pcg32_next_range_u32(PCG32_State *pcg, u32 low, u32 high);
inl u64 pcg32_next_u64(PCG32_State *pcg);
inl f32 pcg32_next_normal_f32(PCG32_State *pcg);
inl f32 pcg32_next_binormal_f32(PCG32_State *pcg);
inl void pcg32_seed(PCG32_State *pcg, u64 seed);

inl u32 rnd_next_u32(void);
inl u64 rnd_next_u64(void);
inl u32 rnd_next_range_u32(u32 low, u32 high);
inl f32 rnd_next_normal_f32(void);
inl f32 rnd_next_binormal_f32(void);
inl void rnd_seed(u64 seed);

#endif //WYLD_BASE_H

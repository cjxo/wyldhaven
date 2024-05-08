
//~ NOTE(christian): Tools
inl b32
is_alpha(u8 c) {
    b32 result = ((c >= 'a') && (c <= 'z')) || ((c >= 'A') && (c <= 'Z'));
    return(result);
}

inl b32
is_digit(u8 c) {
    b32 result = (c >= '0') && (c <= '9');
    return(result);
}

inl b32
is_alpha_numeric(u8 c) {
    b32 result = is_alpha(c) || is_digit(c);
    return(result);
}

//~ NOTE(christian): str8
inl String_U8_Const
str8_make_from_c_str(char *str, u64 char_count) {
    String_U8_Const result;
    result.string = (u8 *)str;
    result.char_count = char_count;
    result.char_capacity = char_count;
    return(result);
}

inl b32
str8_to_s32(String_U8_Const string, s32 *result) {
    b32 convert_success = false;
    
    u64 char_index = 0;
    // NOTE(christian): consume whitespace
    while ((char_index < string.char_capacity) && (string.string[char_index] == ' ')) {
        ++char_index;
    }
    
    if (char_index < string.char_capacity) {
        b32 int_is_negative = false;
        switch (string.string[char_index]) {
            case '-': {
                int_is_negative = true;
            };
            
            case '+': {
                ++char_index;
            } break;
        }
        
        if ((char_index < string.char_capacity) && is_digit(string.string[char_index])) {
            *result = 0;
            while ((char_index < string.char_capacity) && is_digit(string.string[char_index])) {
                *result *= 10;
                *result += string.string[char_index] - '0';
                ++char_index;
            }
            convert_success = true;
        }
    }
    
    return(convert_success);
}

fun String_U8
str8_format_va(Memory_Arena *arena, String_U8_Const string, va_list args0) {
    va_list args1;
    va_copy(args1, args0);
    
    s64 total_chars = vsnprintf(null, 0, (char *)(string.string), args1);
    String_U8 result = {0};
    if (total_chars)
    {
        result.char_count = (u64)(total_chars);
        result.char_capacity = result.char_count;
        result.string = arena_push_array(arena, u8, result.char_count + 1);
        
        wc_assert((result.string != null) && "out of memory");
        
        vsnprintf((char *)(result.string), result.char_count + 1,
                  (char *)(string.string), args1);
        
        result.string[result.char_count] = '\0';
    }
    
    va_end(args1);
    
    return(result);
}

fun String_U8
str8_format(Memory_Arena *arena, String_U8_Const string, ...) {
    va_list args;
    va_start(args, string);
    
    String_U8 result = str8_format_va(arena, string, args);
    
    va_end(args);
    return(result);
}

inl String_U8
str8_reserve(Memory_Arena *arena, u64 char_count) {
    String_U8 result;
    result.string = arena_push_array(arena, u8, char_count);
    result.char_count = 0;
    result.char_capacity = char_count;
    return(result);
}

inl String_U8
str8_reserve_zero(Memory_Arena *arena, u64 char_count) {
    String_U8 result = str8_reserve(arena, char_count);
    clear_memory(result.string, char_count);
    return(result);
}

fun String_U8_Const
str8_substring_view(String_U8_Const source, u64 start, u64 end) {
    u64 end_clamped = minimum(end, source.char_count);
    u64 start_clamped = minimum(start, end_clamped);
    
    String_U8_Const result;
    result.string = source.string + start_clamped;
    result.char_count = end_clamped - start_clamped;
    result.char_capacity = result.char_count;
    
    return(result);
}

fun String_U8
str8_copy(Memory_Arena *arena, String_U8_Const source) {
    String_U8 result;
    result.string = arena_push_array(arena, u8, source.char_count);
    result.char_count = source.char_count;
    result.char_capacity = result.char_count;
    
    for (u64 char_index = 0; char_index < result.char_count; char_index += 1) {
        result.string[char_index] = source.string[char_index];
    }
    
    return(result);
}

inl String_U8
str8_substring(Memory_Arena *arena, String_U8_Const source, u64 from, u64 to) {
    String_U8 result = str8_copy(arena, str8_substring_view(source, from, to));
    return(result);
}

fun b32
str8_equal_strings(String_U8_Const a, String_U8_Const b) {
    b32 result = false;
    
    if (a.char_count == b.char_count) {
        u64 N = a.char_count;
        u64 char_index = 0;
        while ((char_index < N) && (a.string[char_index] == b.string[char_index])) {
            ++char_index;
        }
        
        result = (N == char_index);
    }
    
    return(result);
}

fun u64
str8_compute_hash(u64 base, String_U8_Const string) {
    u64 hash_value = base;
    for (u64 char_index = 0; char_index < string.char_count; char_index += 1) {
        hash_value = (hash_value << 5) + string.string[char_index];
    }
    
    return(hash_value);
}

fun u64
str8_find_first_string(String_U8_Const string, String_U8_Const to_find, u64 offset_from_beginning_of_source) {
    u64 string_index = invalid_index_u64;
    
    if (to_find.char_count && (string.char_count >= to_find.char_count)) {
        string_index = offset_from_beginning_of_source;
        u8 first_char_of_to_find = to_find.string[0];
        u64 one_past_last = string.char_count - to_find.char_count + 1;
        
        for (; string_index < one_past_last; ++string_index) {
            if (string.string[string_index] == first_char_of_to_find) {
                if ((string.char_count - string_index) >= to_find.char_count) {
                    String_U8_Const substring;
                    substring.string = string.string + string_index;
                    substring.char_count = to_find.char_count;
                    if (str8_equal_strings(substring, to_find)) {
                        break;
                    }
                }
            }
        }
        
        if (string_index == one_past_last) {
            string_index = invalid_index_u64;
        }
    }
    
    return(string_index);
}

//~ NOTE(christian): mem
fun Memory_Arena *
arena_reserve(u64 size_in_bytes) {
    Memory_Arena *result = null;
    
    void *block = os_mem_reserve(size_in_bytes);
    if (block) {
        u64 initial_commit = align_a_to_b(sizeof(Memory_Arena), 8);
        if (os_mem_commit(block, initial_commit)) {
            result = (Memory_Arena *)(block);
            result->memory = (u8 *)(block);
            result->commit_ptr = initial_commit;
            result->stack_ptr = initial_commit;
            result->capacity = size_in_bytes;
        }
    }
    
    return(result);
}

inl void
arena_clear(Memory_Arena *arena) {
    u64 arena_size = align_a_to_b(sizeof(Memory_Arena), 8);
    if (arena->stack_ptr > arena_size) {
        arena_pop(arena, arena->stack_ptr - arena_size);
    }
}

fun void *
arena_push(Memory_Arena *arena, u64 push_size) {
    void *result = null;
    
    push_size = align_a_to_b(push_size, 8);
    u64 desired_stack_ptr = arena->stack_ptr + push_size;
    if (desired_stack_ptr <= arena->capacity) {
        void *result_block_on_success = arena->memory + arena->stack_ptr;
        
        u64 desired_commit_ptr = arena->commit_ptr;
        if (desired_stack_ptr >= arena->commit_ptr) {
            u64 new_commit_ptr = align_a_to_b(desired_stack_ptr, kb(128));
            u64 clamped_commit_ptr = minimum(new_commit_ptr, arena->capacity);
            
            if (clamped_commit_ptr > arena->commit_ptr) {
                if (os_mem_commit(arena->memory + arena->commit_ptr,
                                  clamped_commit_ptr - arena->commit_ptr)) {
                    desired_commit_ptr = clamped_commit_ptr;
                }
            }
        }
        
        if (desired_commit_ptr > arena->stack_ptr) {
            arena->stack_ptr = desired_stack_ptr;
            arena->commit_ptr = desired_commit_ptr;
            result = result_block_on_success;
        }
    }
    
    return(result);
}

fun b32
arena_pop(Memory_Arena *arena, u64 pop_size) {
    b32 result = false;
    pop_size = align_a_to_b(pop_size, 8);
    if (pop_size <= (arena->stack_ptr + align_a_to_b(sizeof(Memory_Arena), 8))) {
        arena->stack_ptr -= pop_size;
        u64 desired_commit_ptr = align_a_to_b(arena->stack_ptr, kb(128));
        
        if (desired_commit_ptr < arena->commit_ptr) {
            if (os_mem_decommit(arena->memory + desired_commit_ptr,
                                arena->commit_ptr - desired_commit_ptr)) {
                arena->commit_ptr = desired_commit_ptr;
                result = true;
            }
        }
    }
    
    return(result);
}

inl Temporary_Memory
temp_mem_begin(Memory_Arena *arena) {
    Temporary_Memory result;
    result.arena = arena;
    result.start_stack_ptr = arena->stack_ptr;
    
    return(result);
}

inl void
temp_mem_end(Temporary_Memory temp) {
    if (temp.arena->stack_ptr > temp.start_stack_ptr) {
        arena_pop(temp.arena, temp.arena->stack_ptr - temp.start_stack_ptr);
    }
}

#define scratch_per_thread 4
thread_var glb Memory_Arena *scratch_array[scratch_per_thread] = { null };

inl Memory_Arena *
arena_get_scratch(Memory_Arena **conflict, u64 conflict_count) {
    if (scratch_array[0] == null) {
        for (u32 arena_index = 0;
             arena_index < scratch_per_thread;
             arena_index += 1) {
            scratch_array[arena_index] = arena_reserve(mb(2));
        }
    }
    
    Memory_Arena *result = null;
    
    for (u32 arena_index = 0;
         arena_index < scratch_per_thread;
         arena_index += 1) {
        b32 no_conflict = true;
        Memory_Arena *possible_result = scratch_array[arena_index];
        
        for (u32 conflict_index = 0;
             conflict_index < conflict_count;
             conflict_index += 1) {
            if (conflict[conflict_index] == possible_result) {
                no_conflict = false;
                break;
            }
        }
        
        if (no_conflict) {
            result = possible_result;
            break;
        }
    }
    
    return(result);
}

//~ NOTE(christian): random
glb PCG32_State pcg_state;

inl fun u32
pcg32_next_u32(PCG32_State *pcg) {
    u64 state = pcg->state;
    pcg->state = state * pcg_default_multiplier_64 + pcg_default_increment_64;
    
    u32 value = (u32)((state ^ (state >> 18)) >> 27);
    s32 rot = (s32)(state >> 59);
    return (rot
            ? ((value >> rot) | (value << (32 - rot)))
            : value);
}

inl fun u32
pcg32_next_range_u32(PCG32_State *pcg, u32 low, u32 high) {
    u32 bound = high - low;
    u32 threshold = (u32)(-(s32)(bound) % (s32)(bound));
    
    for (;;) {
        u32 r = pcg32_next_u32(pcg);
        if (r >= threshold) {
            return low + (r % bound);
        }
    }
}

inl u64
pcg32_next_u64(PCG32_State *pcg) {
    u64 result = (((u64)(pcg32_next_u32(pcg)) << 32) | (pcg32_next_u32(pcg)));
    return(result);
}

inl f32
pcg32_next_normal_f32(PCG32_State *pcg) {
    u32 x = pcg32_next_u32(pcg);
    f32 result = (f32)((s32)(x >> 8)) * 0x1.0p-24f;
    return(result);
}

inl f32
pcg32_next_binormal_f32(PCG32_State *pcg) {
    f32 result = pcg32_next_normal_f32(pcg) * 2.0f - 1.0f;
    return(result);
}

inl void
pcg32_seed(PCG32_State *pcg, u64 seed) {
    pcg->state = 0;
    pcg32_next_u32(pcg);
    pcg->state += seed;
    pcg32_next_u32(pcg);
}

inl u32
rnd_next_u32(void) {
    return pcg32_next_u32(&pcg_state);
}

inl u64
rnd_next_u64(void) {
    return pcg32_next_u64(&pcg_state);
}

inl u32
rnd_next_range_u32(u32 low, u32 high) {
    return pcg32_next_range_u32(&pcg_state, low, high);
}

inl f32
rnd_next_normal_f32(void) {
    return pcg32_next_normal_f32(&pcg_state);
}

inl f32
rnd_next_binormal_f32(void) {
    return pcg32_next_binormal_f32(&pcg_state);
}

inl void
rnd_seed(u64 seed) {
    pcg32_seed(&pcg_state, seed);
}
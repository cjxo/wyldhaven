/* date = April 6th 2024 8:22 pm */

#ifndef WYLD_UTIL_H
#define WYLD_UTIL_H

typedef struct Work_Queue Work_Queue;
typedef void Work_Queue_Callback(struct Work_Queue *, void *);

typedef struct {
  void *data;
  Work_Queue_Callback *callback;
} Work_Queue_Entry;

struct Work_Queue {
  Work_Queue_Entry entries[256];
  
  OS_Handle semaphore;
  
  s32 volatile completion_goal;
  s32 volatile completion_count;
  s32 volatile next_entry_to_read;
  s32 volatile next_entry_to_write;
};

fun void wq_create(Work_Queue *queue, u32 thread_count);
fun void wq_complete_all_work(Work_Queue *queue);
fun void wq_add_entry(Work_Queue *queue, void *data, Work_Queue_Callback *callback);

// NOTE(christian): debug stuff
// NOTE(christian): wow: https://lwn.net/Articles/255364/
// https://lwn.net/Articles/250967/
typedef struct {
  u64 time_value;
  char *file_name;
  char *function_name;
  u32 counter_index;
  u32 line_number;
} Prof_TimerRecord;

#define prof_begin_timed_block() \
u32 __counter_value=__COUNTER__;\
prof_start_profile_function(__counter_value,__FUNCTION__,__FILE__,__LINE__)\

#define prof_end_timed_block() prof_end_profile_function(__counter_value)

fun void prof_start_profile_function(u32 counter_init, char *function_name, char *file_name, u32 line_number);
fun void prof_end_profile_function(u32 counter_init);

#endif //WYLD_UTIL_H

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

#endif //WYLD_UTIL_H

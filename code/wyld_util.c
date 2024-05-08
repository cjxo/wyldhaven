fun b32
wq_do_next_entry(Work_Queue *queue) {
    b32 should_sleep = false;
    
    s32 original_next_entry_to_read = queue->next_entry_to_read;
    s32 new_next_entry_to_read = (original_next_entry_to_read + 1) % array_count(queue->entries);
    
    if (original_next_entry_to_read != queue->next_entry_to_write) {
        s32 index = os_interlocked_compare_exchange_s32(&queue->next_entry_to_read,
                                                        new_next_entry_to_read,
                                                        original_next_entry_to_read);
        
        if (index == original_next_entry_to_read) {
            Work_Queue_Entry *entry = queue->entries + index;
            entry->callback(queue, entry->data);
            os_interlocked_increment_s32(&queue->completion_count);
        }
    } else {
        should_sleep = true;
    }
    
    return(should_sleep);
}

fun void
wq_add_entry(Work_Queue *queue, void *data, Work_Queue_Callback *callback) {
    s32 new_next_entry_to_write = (queue->next_entry_to_write + 1) % array_count(queue->entries);
    assert_true(new_next_entry_to_write != queue->next_entry_to_read);
    
    Work_Queue_Entry *entry = queue->entries + queue->next_entry_to_write;
    entry->data = data;
    entry->callback = callback;
    
    os_interlocked_increment_s32(&queue->completion_goal);
    
    _WriteBarrier();
    
    os_interlocked_exchange_s32(&queue->next_entry_to_write, new_next_entry_to_write);
    
    os_semaphore_release(queue->semaphore, 1, null);
}

fun void
wq_complete_all_work(Work_Queue *queue) {
    while (queue->completion_count != queue->completion_goal) {
        wq_do_next_entry(queue);
    }
}

fun s32
__wq_thread_func(void *param) {
    Work_Queue *work_queue = (Work_Queue *)param;
    for (;;) {
        if (wq_do_next_entry(work_queue)) {
            os_wait_for_object(work_queue->semaphore, OSWait_Infinite);
        }
    }
    
    //return(0);
}

fun void
wq_create(Work_Queue *queue, u32 thread_count) {
    assert_true(thread_count > 1);
    
    queue->completion_goal = 0;
    queue->completion_count = 0;
    queue->next_entry_to_read = 0;
    queue->next_entry_to_write = 0;
    
    u32 initial_count = 0;
    queue->semaphore = os_semaphore_create(initial_count, thread_count, null);
    
    for (u32 thread_index = 0;
         thread_index < thread_count;
         ++thread_index) {
        OS_Handle thread = os_thread_create(&__wq_thread_func, true, queue);
        assert_false(os_match_handle(thread, os_bad_handle()));
        os_thread_close(thread);
    }
}
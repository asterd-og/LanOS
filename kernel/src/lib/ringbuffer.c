#include <ringbuffer.h>
#include <heap.h>
#include <string.h>
#include <serial.h>
#include <sched.h>

ringbuffer_t *ringbuffer_create(size_t item_count, size_t item_size) {
    ringbuffer_t *ringbuffer = (ringbuffer_t*)kmalloc(sizeof(ringbuffer_t));
    ringbuffer->data = kmalloc(item_count * item_size);
    ringbuffer->read_waiters = queue_create();
    ringbuffer->write_idx = 0;
    ringbuffer->read_idx = 0;
    ringbuffer->data_count = 0;
    ringbuffer->item_count = item_count;
    ringbuffer->item_size = item_size;
    memset(ringbuffer->data, 0, item_size * item_size);
    return ringbuffer;
}

int ringbuffer_write(ringbuffer_t *ringbuffer, void *data) {
    memcpy((char*)ringbuffer->data + ringbuffer->write_idx * ringbuffer->item_size, data, ringbuffer->item_size);
    ringbuffer->write_idx = (ringbuffer->write_idx + 1) % ringbuffer->item_count;
    ringbuffer->data_count++;
    // Wake up readers
    thread_t *thread = queue_dequeue(ringbuffer->read_waiters);
    while (thread) {
        thread->state = THREAD_RUNNING;
        thread = queue_dequeue(ringbuffer->read_waiters);
    }
    return 0;
}

int ringbuffer_read(ringbuffer_t *ringbuffer, void *out) {
    ringbuffer_wait_data(ringbuffer);
    memcpy(out, (char*)ringbuffer->data + ringbuffer->read_idx * ringbuffer->item_size, ringbuffer->item_size);
    ringbuffer->read_idx = (ringbuffer->read_idx + 1) % ringbuffer->item_count;
    ringbuffer->data_count--;
    return 0;
}

void ringbuffer_get(ringbuffer_t *ringbuffer, void *out, size_t *idx) {
    size_t new_idx = *idx % ringbuffer->item_count;
    while (new_idx >= ringbuffer->data_count) {
        // Nothing to read yet — block
        thread_t *thread = this_thread();
        thread->state = THREAD_BLOCKED;
        queue_append(ringbuffer->read_waiters, thread);
        sched_yield();
    }
    memcpy(out, (char*)ringbuffer->data + new_idx * ringbuffer->item_size, ringbuffer->item_size);
    *idx = (new_idx + 1) % ringbuffer->item_count;
}

void ringbuffer_wait_data(ringbuffer_t *ringbuffer) {
    if (ringbuffer->data_count > 0) return;
    thread_t *thread = this_thread();
    thread->state = THREAD_BLOCKED;
    queue_append(ringbuffer->read_waiters, thread);
    sched_yield();
}

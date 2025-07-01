#include <mutex.h>
#include <log.h>
#include <assert.h>
#include <sched.h>
#include <heap.h>

mutex_t *mutex_create() {
    mutex_t *mutex = (mutex_t*)kmalloc(sizeof(mutex_t));
    mutex->owner = NULL;
    mutex->queue = queue_create();
    mutex->lock = 0;
    return mutex;
}

void mutex_acquire(mutex_t *mutex) {
    thread_t *thread = this_thread();
    if (__sync_lock_test_and_set(&mutex->lock, 1)) {
        // Enqueue this thread to wait to be the owner.
        thread->state = THREAD_BLOCKED;
        queue_append(mutex->queue, thread);
        sched_yield();
    }
    mutex->owner = this_thread();
}

void mutex_release(mutex_t *mutex) {
    ASSERT(mutex->owner == this_thread());
    mutex->owner = NULL;
    __sync_lock_release(&mutex->lock);
    // Wake up next thread on the owner queue.
    thread_t *thread = queue_dequeue(mutex->queue);
    if (!thread)
        return;
    thread->state = THREAD_RUNNING;
}

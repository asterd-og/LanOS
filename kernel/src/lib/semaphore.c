#include <semaphore.h>
#include <heap.h>
#include <sched.h>
#include <serial.h>

semaphore_t *semaphore_create() {
    semaphore_t *sem = (semaphore_t*)kmalloc(sizeof(semaphore_t));
    sem->queue = queue_create();
    sem->count = 0;
    sem->lock = 0;
    return sem;
}

void semaphore_wait(semaphore_t *sem) {
    spinlock_lock(&sem->lock);
    sem->count--;
    if (sem->count < 0) {
        // Block current thread.
        sched_pause();
        thread_t *thread = this_thread();
        thread->state = THREAD_BLOCKED;
        queue_append(sem->queue, thread);
        spinlock_free(&sem->lock);
        sched_resume();
    }
    spinlock_free(&sem->lock);
}

void semaphore_signal(semaphore_t *sem) {
    spinlock_lock(&sem->lock);
    sem->count++;
    if (sem->count <= 0) {
        // Wake up next task from queue
        thread_t *thread = queue_dequeue(sem->queue);
        if (thread) {
            thread->state = THREAD_RUNNING;
            spinlock_free(&sem->lock);
            return;
        }
    }
    spinlock_free(&sem->lock);
}

#pragma once

#include <stdint.h>
#include <queue.h>
#include <spinlock.h>

typedef struct {
    queue_t *queue;
    int count;
    int lock;
} semaphore_t;

semaphore_t *semaphore_create();

void semaphore_wait(semaphore_t *sem);
void semaphore_signal(semaphore_t *sem);

#pragma once

#include <stdint.h>
#include <queue.h>

typedef struct {
    void *owner;
    queue_t *queue;
    int lock;
} mutex_t;

mutex_t *mutex_create();

void mutex_acquire(mutex_t *mutex);
void mutex_release(mutex_t *mutex);

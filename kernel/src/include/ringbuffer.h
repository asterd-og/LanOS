#pragma once

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <semaphore.h>

typedef struct {
    void *data;
    queue_t *read_waiters;
    uint64_t read_idx;
    uint64_t write_idx;
    size_t item_size;
    size_t item_count;
    size_t data_count;
} ringbuffer_t;

ringbuffer_t *ringbuffer_create(size_t item_count, size_t item_size);
int ringbuffer_write(ringbuffer_t *ringbuffer, void *data);
int ringbuffer_read(ringbuffer_t *ringbuffer, void *out);
void ringbuffer_get(ringbuffer_t *ringbuffer, void *out, size_t *idx);
void ringbuffer_wait_data(ringbuffer_t *ringbuffer);

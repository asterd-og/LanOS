#pragma once

#include <stdint.h>
#include <stddef.h>

typedef struct queue_item_t {
    void *data;
    struct queue_item_t *next;
} queue_item_t;

typedef struct {
    queue_item_t *head;
    queue_item_t *tail;
} queue_t;

queue_t *queue_create();

queue_item_t *queue_append(queue_t *queue, void *data);
void *queue_dequeue(queue_t *queue);

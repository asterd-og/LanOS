#pragma once

#include <stdint.h>
#include <stddef.h>

typedef struct slab_bin_t {
    char *area;
    size_t obj_size;
    size_t free_objs;
    size_t max_objs;
    size_t area_size;
    uint64_t free;
    struct slab_bin_t *empty; // Points to the next empty bin
} slab_bin_t;

typedef struct {
    size_t size;
    slab_bin_t *bin;
} __attribute__((packed)) slab_obj_t;

void slab_init();
void *kmalloc(size_t size);
void kfree(void *ptr);

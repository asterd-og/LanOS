#pragma once

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#define SLAB_MAGIC (uint32_t)0xdeadbeef

typedef struct slab_cache_t {
    void *slabs;
    uint64_t obj_size;
    uint64_t free_idx;
    bool used;
    struct slab_cache_t *empty_cache;
} slab_cache_t;

typedef struct {
    bool used;
    slab_cache_t *cache;
    uint32_t magic;
} __attribute__((packed)) slab_obj_t;

typedef struct {
    uint32_t magic;
    uint64_t page_count;
} __attribute__((packed)) slab_page_t;

void slab_init();
void *slab_alloc(size_t size);
void *slab_realloc(void *ptr, size_t size);
void slab_free(void *ptr);

void *kmalloc(size_t size);
void *krealloc(void *ptr, size_t size);
void kfree(void *ptr);

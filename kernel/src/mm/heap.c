#include <heap.h>
#include <vmm.h>
#include <pmm.h>
#include <log.h>
#include <spinlock.h>
#include <string.h>
#include <assert.h>

NEW_LOCK(heap_lock);

#define SLAB_PAGES 4
#define SLAB_SIZE (SLAB_PAGES * PAGE_SIZE)

#define SLAB_IDX(cache, i) (slab_obj_t*)(cache->slabs + (i * (sizeof(slab_obj_t) + cache->obj_size)))
#define OBJS_IN_SLAB(cache) (SLAB_SIZE / (cache->obj_size + sizeof(slab_obj_t)))

// Only 8 because once we hit 4096, the VMA is going to take over.
slab_cache_t *caches[8] = { 0 };

slab_cache_t *slab_create_cache(uint64_t obj_size) {
    slab_cache_t *cache = (slab_cache_t*)vmm_alloc(kernel_pagemap, 1, false);
    cache->obj_size = obj_size;
    uint64_t obj_count = SLAB_SIZE / (cache->obj_size + sizeof(slab_obj_t));
    uint64_t total_size = obj_count * (cache->obj_size + sizeof(slab_obj_t));
    cache->slabs = vmm_alloc(kernel_pagemap, DIV_ROUND_UP(total_size, PAGE_SIZE), false);
    cache->free_idx = 0;
    cache->used = false;
    cache->empty_cache = NULL;
    return cache;
}

void slab_init() {
    uint64_t last_size = 8;
    uint64_t size = 0;
    for (int i = 0; i < 8; i++) {
        size = last_size * 2;
        last_size = size;
        caches[i] = slab_create_cache(size);
    }
}

slab_cache_t *slab_get_cache(size_t size) {
    uint64_t cache = 64 - __builtin_clzll(size - 1);
    cache = (cache >= 4 ? cache - 4 : 0);
    if (cache > 7)
        return NULL;
    return caches[cache];
}

int64_t slab_find_free(slab_cache_t *cache) {
    if (cache->free_idx < OBJS_IN_SLAB(cache))
        return cache->free_idx++;
    for (uint64_t i = 0; i < OBJS_IN_SLAB(cache); i++) {
        slab_obj_t *obj = SLAB_IDX(cache, i);
        if (obj->magic != SLAB_MAGIC)
            return i;
    }
    return -1;
}

slab_cache_t *cache_get_empty(slab_cache_t *cache) {
    slab_cache_t *empty = cache;
    while (empty->used) {
        if (!empty->empty_cache) {
            empty->empty_cache = slab_create_cache(cache->obj_size);
            empty = empty->empty_cache;
            break;
        }
        empty = empty->empty_cache;
    }
    return empty;
}

void *slab_alloc(size_t size) {
    spinlock_lock(&heap_lock);
    slab_cache_t *cache = slab_get_cache(size);
    if (!cache) {
        uint64_t total_pages = DIV_ROUND_UP(size + sizeof(slab_page_t), PAGE_SIZE);
        slab_page_t *page = vmm_alloc(kernel_pagemap, total_pages, false);
        page->magic = SLAB_MAGIC;
        page->page_count = total_pages;
        spinlock_free(&heap_lock);
        return (void*)(page + 1);
    }
    bool found = false;
    int64_t free_obj;
    slab_obj_t *obj = NULL;
    while (!found) {
        if (cache->used) {
            cache = cache_get_empty(cache);
        }
        free_obj = slab_find_free(cache);
        if (free_obj == -1)
            cache->used = true;
        else {
            obj = SLAB_IDX(cache, free_obj);
            found = true;
        }
    }
    obj->cache = cache;
    obj->magic = SLAB_MAGIC;
    spinlock_free(&heap_lock);
    return (void*)(obj + 1);
}

void *slab_realloc(void *ptr, size_t size) {
    if (!ptr) return slab_alloc(size);
    uint64_t old_size = 0;
    slab_page_t *page = (slab_page_t*)((uint64_t)ptr - sizeof(slab_page_t));
    uint64_t dbg = (uint64_t)page;
    if (page->magic != SLAB_MAGIC) {
        slab_obj_t *obj = (slab_obj_t*)((uint64_t)ptr - sizeof(slab_obj_t));
        if (obj->magic != SLAB_MAGIC) {
            LOG_ERROR("Critical SLAB error: Trying to reallocate invalid ptr.\n");
            ASSERT(0);
            return NULL;
        }
        old_size = obj->cache->obj_size;
        dbg = (uint64_t)obj;
    } else {
        old_size = page->page_count * PAGE_SIZE - sizeof(slab_page_t);
    }
    void *new_ptr = slab_alloc(size);
    memcpy(new_ptr, ptr, (old_size > size ? size : old_size));
    slab_free(ptr);
    return new_ptr;
}

void slab_free(void *ptr) {
    spinlock_lock(&heap_lock);
    slab_page_t *page = (slab_page_t*)((uint64_t)ptr - sizeof(slab_page_t));
    if (page->magic != SLAB_MAGIC) {
        slab_obj_t *obj = (slab_obj_t*)((uint64_t)ptr - sizeof(slab_obj_t));
        if (obj->magic != SLAB_MAGIC) {
            LOG_ERROR("Critical SLAB error: Trying to free invalid ptr.\n");
            spinlock_free(&heap_lock);
            return;
        }
        slab_cache_t *cache = obj->cache;
        if (cache->used)
            cache->used = false;
        obj->magic = 0;
        spinlock_free(&heap_lock);
        return;
    }
    vmm_free(kernel_pagemap, page);
    spinlock_free(&heap_lock);
}

void *kmalloc(size_t size) {
    return slab_alloc(size);
}

void *krealloc(void *ptr, size_t size) {
    return slab_realloc(ptr, size);
}

void kfree(void *ptr) {
    slab_free(ptr);
}

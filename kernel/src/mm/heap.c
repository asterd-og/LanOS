#include <heap.h>
#include <vmm.h>
#include <pmm.h>
#include <log.h>
#include <spinlock.h>

#define BIN_PAGES 8

slab_bin_t *slab_bins[10]; // Up to 16 KB, starting at 32 B

NEW_LOCK(heap_lock);

slab_bin_t *slab_create_bin(uint64_t obj_size) {
    slab_bin_t *bin = vmm_alloc(kernel_pagemap, 1);
    bin->obj_size = obj_size;
    bin->max_objs = (PAGE_SIZE * BIN_PAGES) / obj_size;
    bin->free_objs = bin->max_objs;
    bin->free = 0;
    uint64_t pages = bin->max_objs * obj_size;
    pages += bin->max_objs * sizeof(size_t) + sizeof(uint64_t);
    pages = DIV_ROUND_UP(pages, PAGE_SIZE);
    bin->area = vmm_alloc(kernel_pagemap, pages);
    bin->area_size = pages * PAGE_SIZE;
    bin->empty = NULL;
    return bin;
}

void slab_init() {
    uint64_t last_size = 16;
    for (int i = 0; i < 10; i++) {
        uint64_t size = last_size * 2;
        last_size = size;
        slab_bins[i] = slab_create_bin(size);
    }
}

slab_bin_t *slab_get_bin(size_t size) {
    uint64_t bin = 64 - __builtin_clzll(size - 1);
    bin = (bin >= 5 ? bin - 5 : 0);
    if (bin > 9)
        return NULL;
    return slab_bins[bin];
}

void *kmalloc(size_t size) {
    if (!size) return NULL;
    spinlock_lock(&heap_lock);
    slab_bin_t *bin = slab_get_bin(size);
    if (!bin) {
        uint64_t page_count = DIV_ROUND_UP(size, PAGE_SIZE);
        size_t *start = vmm_alloc(kernel_pagemap, page_count);
        *start = page_count;
        spinlock_free(&heap_lock);
        return start + 1;
    }
    while (bin->free_objs == 0) {
        if (!bin->empty)
            bin->empty = slab_create_bin(bin->obj_size);
        bin = bin->empty;
    }
    if (bin->free >= bin->area_size) {
        uint64_t start = (uint64_t)bin->area;
        slab_obj_t *obj = (slab_obj_t*)start;
        while (obj->size == bin->obj_size) {
            start += sizeof(slab_obj_t) + bin->obj_size;
            obj = (slab_obj_t*)start;
        }
        bin->free = start - (uint64_t)bin->area;
    }
    uint64_t start = (uint64_t)bin->area + bin->free;
    slab_obj_t *obj = (slab_obj_t*)start;
    obj->size = bin->obj_size;
    obj->bin = bin;
    bin->free += bin->obj_size + sizeof(slab_obj_t);
    bin->free_objs -= 1;
    spinlock_free(&heap_lock);
    return (void*)(start + sizeof(slab_obj_t));
}

void kfree(void *ptr) {
    uint64_t llptr = (uint64_t)ptr;
    if (((llptr - 8) & 0xfff) == 0) {
        // TODO: Test if a bin allocation can ever be in a page boundary.
        uint64_t page_count = *(uint64_t*)(ptr - 8);
        vmm_free(kernel_pagemap, (void*)(llptr - 8), page_count);
        return;
    }
    slab_obj_t *obj = (slab_obj_t*)(llptr - sizeof(slab_obj_t));
    slab_bin_t *bin = obj->bin;
    bin->free_objs += 1;
    if (bin->free_objs == 1)
        bin->free = (uint64_t)obj - (uint64_t)bin->area;
}

#pragma once

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#define MM_READ 1
#define MM_WRITE 2
#define MM_USER 4
#define MM_NX (1ull << 63)

#define PML4E(x) ((x >> 39) & 0x1ff)
#define PDPTE(x) ((x >> 30) & 0x1ff)
#define PDE(x) ((x >> 21) & 0x1ff)
#define PTE(x) ((x >> 12) & 0x1ff)

#define PTE_MASK(x) (typeof(x))((uint64_t)x & 0x000ffffffffff000)

#define PAGE_EXISTS(x) ((uint64_t)x & MM_READ)

typedef struct vma_region_t {
    uint64_t start;
    uint64_t page_count;
    uint64_t flags;
    struct vma_region_t *next;
    struct vma_region_t *prev;
} vma_region_t;

typedef struct {
    uint64_t *pml4;
    vma_region_t *vma_head;
} pagemap_t;

extern pagemap_t *kernel_pagemap;

void vmm_init();
void vmm_map(pagemap_t *pagemap, uint64_t vaddr, uint64_t paddr, uint64_t flags);
uint64_t vmm_get_phys(pagemap_t *pagemap, uint64_t vaddr);
void vmm_map_range(pagemap_t *pagemap, uint64_t vaddr, uint64_t paddr, uint64_t flags, uint64_t count);
pagemap_t *vmm_switch_pagemap(pagemap_t *pagemap);
pagemap_t *vmm_new_pagemap();

vma_region_t *vma_add_region(pagemap_t *pagemap, uint64_t start, uint64_t page_count, uint64_t flags);
vma_region_t *vma_insert_region(vma_region_t *after, uint64_t start, uint64_t page_count, uint64_t flags);

void *vmm_alloc(pagemap_t *pagemap, uint64_t page_count, bool user);
void *vmm_alloc_to(pagemap_t *pagemap, uint64_t paddr, uint64_t page_count, bool user);
void vmm_free(pagemap_t *pagemap, void *ptr, uint64_t page_count);

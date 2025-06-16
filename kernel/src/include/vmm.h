#pragma once

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <idt.h>

#define MM_READ 1
#define MM_WRITE 2
#define MM_USER 4
#define MM_NX (1ull << 63)

#define PML4E(x) ((x >> 39) & 0x1ff)
#define PDPTE(x) ((x >> 30) & 0x1ff)
#define PDE(x) ((x >> 21) & 0x1ff)
#define PTE(x) ((x >> 12) & 0x1ff)

#define PTE_MASK(x) (typeof(x))((uint64_t)x & 0x000ffffffffff000)
#define PTE_FLAGS(x) (typeof(x))((uint64_t)x & 0xfff0000000000fff)

#define PAGE_EXISTS(x) ((uint64_t)x & MM_READ)

typedef struct vma_region_t {
    uint64_t start;
    uint64_t page_count;
    uint64_t flags;
    struct vma_region_t *next;
    struct vma_region_t *prev;
} vma_region_t;

typedef struct vm_mapping_t {
    uint64_t start;
    uint64_t page_count;
    uint64_t flags;
    struct vm_mapping_t *next;
    struct vm_mapping_t *prev;
} vm_mapping_t;

typedef struct {
    uint64_t *pml4;
    vm_mapping_t *vm_mappings;
    int vma_lock;
    vma_region_t *vma_head;
} pagemap_t;

extern pagemap_t *kernel_pagemap;

void vmm_init();
void vmm_map(pagemap_t *pagemap, uint64_t vaddr, uint64_t paddr, uint64_t flags);
void vmm_unmap(pagemap_t *pagemap, uint64_t vaddr);
uint64_t vmm_get_phys(pagemap_t *pagemap, uint64_t vaddr);
void vmm_map_range(pagemap_t *pagemap, uint64_t vaddr, uint64_t paddr, uint64_t flags, uint64_t count);
pagemap_t *vmm_switch_pagemap(pagemap_t *pagemap);
pagemap_t *vmm_new_pagemap();

void vma_set_start(pagemap_t *pagemap, uint64_t start, uint64_t page_count);
vma_region_t *vma_add_region(pagemap_t *pagemap, uint64_t start, uint64_t page_count, uint64_t flags);
vma_region_t *vma_insert_region(vma_region_t *after, uint64_t start, uint64_t page_count, uint64_t flags);

vm_mapping_t *vmm_new_mapping(pagemap_t *pagemap, uint64_t start, uint64_t page_count, uint64_t flags);
void vmm_remove_mapping(vm_mapping_t *mapping);

void *vmm_alloc(pagemap_t *pagemap, uint64_t page_count, bool user);
void vmm_free(pagemap_t *pagemap, void *ptr);

pagemap_t *vmm_fork(pagemap_t *parent);
void vmm_clean_pagemap(pagemap_t *pagemap);
void vmm_destroy_pagemap(pagemap_t *pagemap);

int vmm_handle_fault(context_t *ctx);

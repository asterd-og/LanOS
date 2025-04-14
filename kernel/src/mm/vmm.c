#include <vmm.h>
#include <log.h>
#include <pmm.h>
#include <string.h>
#include <limine.h>
#include <smp.h>

#define PHYS_BASE(x) (x - executable_vaddr + executable_paddr)

__attribute__((used, section(".limine_requests")))
static volatile struct limine_executable_address_request limine_executable_address = {
    .id = LIMINE_EXECUTABLE_ADDRESS_REQUEST,
    .revision = 0
};

pagemap_t *kernel_pagemap = NULL;

extern char ld_limine_start[];
extern char ld_limine_end[];
extern char ld_text_start[];
extern char ld_text_end[];
extern char ld_rodata_start[];
extern char ld_rodata_end[];
extern char ld_data_start[];
extern char ld_data_end[];

void vmm_init() {
    struct limine_executable_address_response *executable_address = limine_executable_address.response;
    kernel_pagemap = HIGHER_HALF((pagemap_t*)pmm_request());
    kernel_pagemap->pml4 = HIGHER_HALF((uint64_t*)pmm_request());
    memset(kernel_pagemap->pml4, 0, PAGE_SIZE);
    
    uint64_t executable_vaddr = executable_address->virtual_base;
    uint64_t executable_paddr = executable_address->physical_base;

    uint64_t limine_start = ALIGN_DOWN((uint64_t)ld_limine_start, PAGE_SIZE);
    uint64_t limine_end   = ALIGN_UP((uint64_t)ld_limine_end, PAGE_SIZE);
    uint64_t limine_pages = DIV_ROUND_UP(limine_end - limine_start, PAGE_SIZE);

    uint64_t text_start = ALIGN_DOWN((uint64_t)ld_text_start, PAGE_SIZE);
    uint64_t text_end   = ALIGN_UP((uint64_t)ld_text_end, PAGE_SIZE);
    uint64_t text_pages = DIV_ROUND_UP(text_end - text_start, PAGE_SIZE);

    uint64_t rodata_start = ALIGN_DOWN((uint64_t)ld_rodata_start, PAGE_SIZE);
    uint64_t rodata_end   = ALIGN_UP((uint64_t)ld_rodata_end, PAGE_SIZE);
    uint64_t rodata_pages = DIV_ROUND_UP(rodata_end - rodata_start, PAGE_SIZE);

    uint64_t data_start = ALIGN_DOWN((uint64_t)ld_data_start, PAGE_SIZE);
    uint64_t data_end   = ALIGN_UP((uint64_t)ld_data_end, PAGE_SIZE);
    uint64_t data_pages = DIV_ROUND_UP(data_end - data_start, PAGE_SIZE);

    vmm_map_range(kernel_pagemap, limine_start, PHYS_BASE(limine_start), MM_READ, limine_pages);
    vmm_map_range(kernel_pagemap, text_start, PHYS_BASE(text_start), MM_READ, text_pages);
    vmm_map_range(kernel_pagemap, rodata_start, PHYS_BASE(rodata_start), MM_READ | MM_NX,
            rodata_pages);
    vmm_map_range(kernel_pagemap, data_start, PHYS_BASE(data_start), MM_READ | MM_WRITE | MM_NX,
            data_pages);

    for (uint64_t gb4 = 0; gb4 < 0x100000000; gb4 += PAGE_SIZE) {
        vmm_map(kernel_pagemap, gb4, gb4, MM_READ | MM_WRITE);
        vmm_map(kernel_pagemap, HIGHER_HALF(gb4), gb4, MM_READ | MM_WRITE);
    }

    vmm_switch_pagemap(kernel_pagemap);

    avl_t *avl_root = NULL;
    uint64_t first_free_addr = memmap->entries[0]->base + pmm_bitmap_pages * PAGE_SIZE;
    vma_region_t region = {
        .start = HIGHER_HALF(first_free_addr),
        .page_count = 0x100000,
        .flags = MM_READ | MM_WRITE
    };
    avl_root = avl_insert_node(avl_root, 0x100000, &region);
    kernel_pagemap->avl_root = avl_root;

    LOG_OK("VMM Initialised.\n");
}

uint64_t *vmm_new_level(uint64_t *level, uint64_t entry) {
    uint64_t *new_level = pmm_request();
    memset(HIGHER_HALF(new_level), 0, PAGE_SIZE);
    level[entry] = (uint64_t)new_level | 0b111; // Directories should have
                                                // these default flags.
    return new_level;
}

void vmm_map(pagemap_t *pagemap, uint64_t vaddr, uint64_t paddr, uint64_t flags) {
    uint64_t *pdpt = (uint64_t*)pagemap->pml4[PML4E(vaddr)];
    if (!PAGE_EXISTS(pdpt))
        pdpt = vmm_new_level(pagemap->pml4, PML4E(vaddr));
    pdpt = HIGHER_HALF(PTE_MASK(pdpt));

    uint64_t *pd = (uint64_t*)pdpt[PDPTE(vaddr)];
    if (!PAGE_EXISTS(pd))
        pd = vmm_new_level(pdpt, PDPTE(vaddr));
    pd = HIGHER_HALF(PTE_MASK(pd));

    uint64_t *pt = (uint64_t*)pd[PDE(vaddr)];
    if (!PAGE_EXISTS(pt))
        pt = vmm_new_level(pd, PDE(vaddr));
    pt = HIGHER_HALF(PTE_MASK(pt));

    pt[PTE(vaddr)] = paddr | flags;
}

void vmm_map_range(pagemap_t *pagemap, uint64_t vaddr, uint64_t paddr, uint64_t flags, uint64_t count) {
    for (uint64_t i = 0; i < count; i++)
        vmm_map(pagemap, vaddr + (i * PAGE_SIZE), paddr + (i * PAGE_SIZE), flags);
}

void vmm_switch_pagemap(pagemap_t *pagemap) {
    __asm__ volatile ("mov %0, %%cr3" : : "r"(PHYSICAL((uint64_t)pagemap->pml4)) : "memory");
    if (smp_started)
        this_cpu()->pagemap = pagemap;
}

pagemap_t *vmm_new_pagemap() {
    pagemap_t *pagemap = HIGHER_HALF((pagemap_t*)pmm_request());
    pagemap->pml4 = HIGHER_HALF((uint64_t*)pmm_request());
    pagemap->avl_root = NULL;
    memset(pagemap->pml4, 0, PAGE_SIZE);
    for (uint64_t i = 256; i < 512; i++)
        pagemap->pml4[i] = kernel_pagemap->pml4[i];
    // The avl root is defined after the pagemap is created
    // I wont put it here because the starting address
    // might vary from page map to page map (for example, in an ELF it might be
    // defined after the last section in the linker.)
    return pagemap;
}

void *vmm_alloc(pagemap_t *pagemap, uint64_t page_count) {
    // Look for the best fit on the tree
    avl_t *best_node = pagemap->avl_root;
    avl_t *node = pagemap->avl_root;
    while (node != NULL) {
        if (node->key >= page_count) {
            best_node = node;
            node = node->left;
        } else node = node->right;
    }
    vma_region_t *region = (vma_region_t*)best_node->data;
    uint64_t id = 0;
    if (best_node->dup) {
        avl_dup_t *dup = best_node->dup;
        id++;
        do {
            if (dup->next == best_node->dup)
                break;
            dup = dup->next;
            id++;
        } while (1);
        region = (vma_region_t*)dup->data;
    }
    uint64_t start = region->start;
    uint64_t new_start = region->start + (page_count * PAGE_SIZE);
    uint64_t new_page_count = region->page_count - page_count;
    pagemap->avl_root = avl_delete_node(pagemap->avl_root, best_node->key, id);
    if (new_page_count != 0) {
        vma_region_t new_region = {
            .start = new_start,
            .page_count = new_page_count,
            .flags = MM_READ | MM_WRITE
        };
        pagemap->avl_root = avl_insert_node(pagemap->avl_root,
                new_page_count, &new_region);
    }
    // Map the pages
    for (uint64_t i = 0; i < page_count; i++) {
        vmm_map(kernel_pagemap, start + (i * PAGE_SIZE),
                (uint64_t)pmm_request(), MM_READ | MM_WRITE);
    }
    return (void*)start;
}

void vmm_free(pagemap_t *pagemap, void *ptr, uint64_t page_count) {
    // TODO: Find the node containing the region
    // in which this page belonged to (to maybe merge nodes together)
    // TODO: Free the physical pages.
    vma_region_t region = {
        .start = (uint64_t)ptr,
        .page_count = page_count,
        .flags = MM_READ | MM_WRITE
    };
    pagemap->avl_root = avl_insert_node(pagemap->avl_root, page_count, &region);
}

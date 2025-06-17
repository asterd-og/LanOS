#include <vmm.h>
#include <log.h>
#include <pmm.h>
#include <string.h>
#include <limine.h>
#include <smp.h>
#include <spinlock.h>

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

    uint64_t first_free_addr = memmap->entries[0]->base + pmm_bitmap_pages * PAGE_SIZE;
    vma_set_start(kernel_pagemap, HIGHER_HALF(0x100000000000), 1);

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

    LOG_OK("VMM Initialised.\n");
}

void vma_set_start(pagemap_t *pagemap, uint64_t start, uint64_t page_count) {
    vma_region_t *region = HIGHER_HALF((vma_region_t*)pmm_request());
    region->start = start;
    region->page_count = page_count;
    region->flags = MM_READ | MM_WRITE;
    region->next = region;
    region->prev = region;
    pagemap->vma_head = region;
}

vma_region_t *vma_add_region(pagemap_t *pagemap, uint64_t start, uint64_t page_count, uint64_t flags) {
    vma_region_t *region = HIGHER_HALF((vma_region_t*)pmm_request());
    region->start = start;
    region->page_count = page_count;
    region->flags = flags;
    region->prev = pagemap->vma_head->prev;
    region->next = pagemap->vma_head;
    pagemap->vma_head->prev->next = region;
    pagemap->vma_head->prev = region;
    return region;
}

vma_region_t *vma_insert_region(vma_region_t *after, uint64_t start, uint64_t page_count, uint64_t flags) {
    vma_region_t *region = HIGHER_HALF((vma_region_t*)pmm_request());
    region->start = start;
    region->page_count = page_count;
    region->flags = flags;
    region->prev = after;
    region->next = after->next;
    after->next->prev = region;
    after->next = region;
    return region;
}

vm_mapping_t *vmm_new_mapping(pagemap_t *pagemap, uint64_t start, uint64_t page_count, uint64_t flags) {
    vm_mapping_t *mapping = HIGHER_HALF((vm_mapping_t*)pmm_request());
    mapping->start = start;
    mapping->page_count = page_count;
    mapping->flags = flags;
    if (pagemap->vm_mappings) {
        mapping->prev = pagemap->vm_mappings->prev;
        mapping->next = pagemap->vm_mappings;
        pagemap->vm_mappings->prev->next = mapping;
        pagemap->vm_mappings->prev = mapping;
        return mapping;
    }
    mapping->prev = mapping->next = mapping;
    pagemap->vm_mappings = mapping;
    return mapping;
}

void vma_remove_region(vma_region_t *region) {
    region->next->prev = region->prev;
    region->prev->next = region->next;
    pmm_free(PHYSICAL((void*)region));
}

void vmm_remove_mapping(vm_mapping_t *mapping) {
    mapping->next->prev = mapping->prev;
    mapping->prev->next = mapping->next;
    pmm_free(PHYSICAL((void*)mapping));
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

extern void mmu_invlpg(uint64_t vaddr);

void vmm_unmap(pagemap_t *pagemap, uint64_t vaddr) {
    uint64_t *pdpt = (uint64_t*)pagemap->pml4[PML4E(vaddr)];
    if (!PAGE_EXISTS(pdpt)) return;
    pdpt = HIGHER_HALF(PTE_MASK(pdpt));

    uint64_t *pd = (uint64_t*)pdpt[PDPTE(vaddr)];
    if (!PAGE_EXISTS(pd)) return;
    pd = HIGHER_HALF(PTE_MASK(pd));

    uint64_t *pt = (uint64_t*)pd[PDE(vaddr)];
    if (!PAGE_EXISTS(pt)) return;
    pt = HIGHER_HALF(PTE_MASK(pt));

    pt[PTE(vaddr)] = 0;
}

uint64_t vmm_get_phys_flags(pagemap_t *pagemap, uint64_t vaddr) {
    uint64_t *pdpt = (uint64_t*)pagemap->pml4[PML4E(vaddr)];
    if (!PAGE_EXISTS(pdpt)) return 0;
    pdpt = HIGHER_HALF(PTE_MASK(pdpt));

    uint64_t *pd = (uint64_t*)pdpt[PDPTE(vaddr)];
    if (!PAGE_EXISTS(pd)) return 0;
    pd = HIGHER_HALF(PTE_MASK(pd));

    uint64_t *pt = (uint64_t*)pd[PDE(vaddr)];
    if (!PAGE_EXISTS(pt)) return 0;
    pt = HIGHER_HALF(PTE_MASK(pt));

    return pt[PTE(vaddr)];
}

uint64_t vmm_get_phys(pagemap_t *pagemap, uint64_t vaddr) {
    uint64_t page = vmm_get_phys_flags(pagemap, vaddr);
    if (!page) return 0;

    return PTE_MASK(page);
}

void vmm_map_range(pagemap_t *pagemap, uint64_t vaddr, uint64_t paddr, uint64_t flags, uint64_t count) {
    for (uint64_t i = 0; i < count; i++)
        vmm_map(pagemap, vaddr + (i * PAGE_SIZE), paddr + (i * PAGE_SIZE), flags);
}

pagemap_t *vmm_switch_pagemap(pagemap_t *pagemap) {
    pagemap_t *old_pagemap = NULL;
    if (smp_started) {
        old_pagemap = this_cpu()->pagemap;
        this_cpu()->pagemap = pagemap;
    }
    __asm__ volatile ("mov %0, %%cr3" : : "r"(PHYSICAL((uint64_t)pagemap->pml4)) : "memory");
    return old_pagemap;
}

pagemap_t *vmm_new_pagemap() {
    pagemap_t *pagemap = HIGHER_HALF((pagemap_t*)pmm_request());
    pagemap->pml4 = HIGHER_HALF((uint64_t*)pmm_request());
    memset(pagemap->pml4, 0, PAGE_SIZE);
    pagemap->vm_mappings = NULL;
    pagemap->vma_lock = 0;
    pagemap->vma_head = NULL;
    for (uint64_t i = 256; i < 512; i++)
        pagemap->pml4[i] = kernel_pagemap->pml4[i];
    // The vma root is defined after the pagemap is created
    // I wont put it here because the starting address
    // might vary from page map to page map (for example, in an ELF it might be
    // defined after the last section in the linker.)
    return pagemap;
}

uint64_t vmm_internal_alloc(pagemap_t *pagemap, uint64_t page_count, uint64_t flags) {
    // Look for the first fit on the list.
    vma_region_t *region = pagemap->vma_head->next;
    uint64_t addr = 0;
    if (region == pagemap->vma_head) {
        addr = region->start + (region->page_count * PAGE_SIZE);
        vma_add_region(pagemap, addr, page_count, flags);
        return addr;
    }
    for (; region != pagemap->vma_head; region = region->next) {
        if (region->next == pagemap->vma_head) {
            addr = region->start + (region->page_count * PAGE_SIZE);
            vma_add_region(pagemap, addr, page_count, flags);
            return addr;
        }
        uint64_t region_end = region->start + (region->page_count * PAGE_SIZE);
        if (region->next->start >= region_end + (page_count * PAGE_SIZE)) {
            addr = region_end;
            region = vma_insert_region(region, addr, page_count, flags);
            return addr;
        }
    }
    return 0;
}

void *vmm_alloc(pagemap_t *pagemap, uint64_t page_count, bool user) {
    // Look for the first fit on the list.
    if (!page_count) return NULL;
    spinlock_lock(&pagemap->vma_lock);
    uint64_t flags = MM_READ | MM_WRITE | (user ? MM_USER : 0);
    uint64_t addr = vmm_internal_alloc(pagemap, page_count, flags);
    for (int i = 0; i < page_count; i++)
        vmm_map(pagemap, addr + (i * PAGE_SIZE), (uint64_t)pmm_request(), flags);
    vmm_new_mapping(pagemap, addr, page_count, flags);
    spinlock_free(&pagemap->vma_lock);
    return (void*)addr;
}

void vmm_free(pagemap_t *pagemap, void *ptr) {
    if (((uint64_t)ptr & 0xfff) != 0)
        return;
    spinlock_lock(&pagemap->vma_lock);
    vma_region_t *region = pagemap->vma_head->next;
    for (; region != pagemap->vma_head; region = region->next) {
        if (region->start == (uint64_t)ptr) {
            for (uint64_t i = 0; i < region->page_count; i++) {
                uint64_t phys_addr = vmm_get_phys(pagemap, (uint64_t)region->start + (i * PAGE_SIZE));
                pmm_free((void*)phys_addr);
                vmm_unmap(pagemap, region->start + (i * PAGE_SIZE));
                mmu_invlpg(region->start + i * PAGE_SIZE);
            }
            vma_remove_region(region);
            spinlock_free(&pagemap->vma_lock);
            return;
        }
    }
    spinlock_free(&pagemap->vma_lock);
}

pagemap_t *vmm_fork(pagemap_t *parent) {
    pagemap_t *restore = vmm_switch_pagemap(kernel_pagemap);
    pagemap_t *pagemap = HIGHER_HALF((pagemap_t*)pmm_request());
    pagemap->pml4 = HIGHER_HALF((uint64_t*)pmm_request());
    memset(pagemap->pml4, 0, PAGE_SIZE);
    for (uint64_t i = 256; i < 512; i++)
        pagemap->pml4[i] = kernel_pagemap->pml4[i];
    // Copy mappings
    vm_mapping_t *mapping = parent->vm_mappings;
    while (true) {
        uint64_t page_flags = mapping->flags;
        uint64_t new_flags = page_flags & ~MM_WRITE;
        new_flags |= (1ULL << 55);
        for (uint64_t i = 0; i < mapping->page_count; i++) {
            uint64_t virt_addr = mapping->start + (i * PAGE_SIZE);
            uint64_t phys_addr = vmm_get_phys(parent, virt_addr);
            vmm_map(pagemap, virt_addr, phys_addr, new_flags);
            vmm_map(parent, virt_addr, phys_addr, new_flags);
        }
        vmm_new_mapping(pagemap, mapping->start, mapping->page_count, page_flags);
        mapping = mapping->next;
        if (mapping == parent->vm_mappings)
            break;
    }
    // Copy vma
    vma_set_start(pagemap, parent->vma_head->start, parent->vma_head->page_count);
    spinlock_lock(&parent->vma_lock);
    vma_region_t *region = parent->vma_head->next;
    for (; region != parent->vma_head; region = region->next)
        vma_add_region(pagemap, region->start, region->page_count, region->flags);
    spinlock_free(&parent->vma_lock);
    vmm_switch_pagemap(restore);
    return pagemap;
}

void vmm_clean_pagemap(pagemap_t *pagemap) {
    // Free up vm allocations and unmap them
    vma_region_t *region = pagemap->vma_head->next;
    vma_region_t *next_region = region->next;
    for (; region != pagemap->vma_head; region = next_region) {
        next_region = region->next;
        for (uint64_t i = 0; i < region->page_count; i++) {
            uint64_t phys_addr = vmm_get_phys(pagemap, (uint64_t)region->start + (i * PAGE_SIZE));
            pmm_free((void*)phys_addr);
            vmm_unmap(pagemap, region->start + (i * PAGE_SIZE));
        }
        vma_remove_region(region);
    }
    // Remove vma head
    pmm_free(PHYSICAL(pagemap->vma_head));
    pagemap->vma_head = NULL;
    // Clean mappings
    vm_mapping_t *mapping = pagemap->vm_mappings->next;
    vm_mapping_t *start_mapping = pagemap->vm_mappings;
    bool cont = true;
    do {
        vm_mapping_t *next = mapping->next;
        if (next == start_mapping)
            cont = false;
        for (uint64_t i = 0; i < mapping->page_count; i++)
            vmm_unmap(pagemap, mapping->start + (i * PAGE_SIZE));
        vmm_remove_mapping(mapping);
        mapping = next;
    } while (cont);
    for (uint64_t i = 0; i < start_mapping->page_count; i++)
        vmm_unmap(pagemap, start_mapping->start + (i * PAGE_SIZE));
    vmm_remove_mapping(start_mapping);
    pagemap->vm_mappings = NULL;
}

void vmm_destroy_pagemap(pagemap_t *pagemap) {
    vmm_clean_pagemap(pagemap);
    pmm_free(PHYSICAL(pagemap->pml4));
    pmm_free(PHYSICAL(pagemap));
}

int vmm_handle_fault(context_t *ctx) {
    // Check if the fault was a CoW fault, or if it was truly just a page fault.
    uint64_t cr2 = 0;
    __asm__ volatile ("movq %%cr2, %0" : "=r"(cr2));
    if (!smp_started || !this_cpu() || !this_thread()) {
        printf("Page fault on 0x%p, Should NOT continue.\n", cr2);
        return 1; // Should NOT continue execution.
    }
    pagemap_t *restore = vmm_switch_pagemap(kernel_pagemap);
    uint64_t fault_addr = ALIGN_DOWN(cr2, PAGE_SIZE);
    pagemap_t *pagemap = this_thread()->pagemap;
    uint64_t page = vmm_get_phys_flags(pagemap, fault_addr);
    uint64_t old_phys = PTE_MASK(page);
    if (!old_phys) {
        printf("Page fault on thread (0x%p).\n", cr2);
        return 1;
    }
    uint64_t new_flags = PTE_FLAGS(page);
    new_flags &= ~(1ULL << 5);
    new_flags |= MM_WRITE;
    uint64_t new_phys = (uint64_t)pmm_request();
    memcpy(HIGHER_HALF((void*)new_phys), HIGHER_HALF((void*)old_phys), PAGE_SIZE);
    vmm_map(pagemap, fault_addr, new_phys, new_flags);
    __asm__ volatile ("invlpg (%0)" : : "r"(fault_addr));
    return 0;
}

#include "vmm.h"
#include <elf.h>
#include <pmm.h>
#include <stdint.h>
#include <string.h>

uint64_t elf_load(uint8_t *data, pagemap_t *pagemap) {
    elf_hdr_t *hdr = (elf_hdr_t*)data;

    if (hdr->ident[0] != 0x7f || hdr->ident[1] != 'E' || hdr->ident[2] != 'L' || hdr->ident[3] != 'F')
        return 0;

    if (hdr->type != 2)
        return 0;

    elf_phdr_t *phdr;
    uint64_t max_vaddr = 0;

    vmm_switch_pagemap(pagemap);

    for (int i = 0; i < hdr->entry_ph_count; i++) {
        phdr = (elf_phdr_t*)(data + hdr->phoff + (i * sizeof(elf_phdr_t)));
        if (phdr->type == 1) {
            // Load this segment into memory at the correct virtual address
            uint64_t start = ALIGN_DOWN(phdr->vaddr, PAGE_SIZE);
            uint64_t end = ALIGN_UP(phdr->vaddr + phdr->mem_size, PAGE_SIZE);
            for (uint64_t i = start; i < end; i += PAGE_SIZE) {
                uint64_t page = (uint64_t)pmm_request();
                vmm_map(pagemap, i, page, MM_READ | MM_WRITE | MM_USER);
            }
            memcpy((void*)start, (void*)(data + phdr->offset), phdr->file_size);
            if (end > max_vaddr)
                max_vaddr = end;
        }
    }
    vmm_switch_pagemap(kernel_pagemap);
    max_vaddr += PAGE_SIZE;

    vma_add_region(pagemap, max_vaddr, 1, MM_READ | MM_WRITE | MM_USER);

    return hdr->entry;
}

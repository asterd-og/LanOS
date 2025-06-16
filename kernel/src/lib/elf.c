#include <elf.h>
#include <pmm.h>
#include <stdint.h>
#include <string.h>
#include <stdio.h>

uint64_t elf_load(uint8_t *data, pagemap_t *pagemap) {
    elf_hdr_t *hdr = (elf_hdr_t*)data;

    if (hdr->ident[0] != 0x7f || hdr->ident[1] != 'E' || hdr->ident[2] != 'L' || hdr->ident[3] != 'F')
        return 0;

    if (hdr->type != 2)
        return 0;

    elf_phdr_t *phdrs = (elf_phdr_t*)(data + hdr->phoff);
    uint64_t max_vaddr = 0;

    vmm_switch_pagemap(pagemap);

    for (int i = 0; i < hdr->phnum; i++) {
        elf_phdr_t *phdr = &phdrs[i];
        if (phdr->type == 1) {
            // Load this segment into memory at the correct virtual address
            uint64_t start = ALIGN_DOWN(phdr->vaddr, PAGE_SIZE);
            uint64_t end = ALIGN_UP(phdr->vaddr + phdr->memsz, PAGE_SIZE);
            uint64_t flags = MM_READ | MM_WRITE | MM_USER;
            for (uint64_t i = start; i < end; i += PAGE_SIZE) {
                uint64_t page = (uint64_t)pmm_request();
                vmm_map(pagemap, i, page, flags);
            }
            vmm_new_mapping(pagemap, start, (end - start) / PAGE_SIZE, flags);
            memcpy((void*)phdr->vaddr, (void*)(data + phdr->offset), phdr->filesz);
            if (phdr->memsz > phdr->filesz)
                memset((void*)(phdr->vaddr + phdr->filesz), 0, phdr->memsz - phdr->filesz); // Zero out BSS
            if (end > max_vaddr)
                max_vaddr = end;
        }
    }

    vmm_switch_pagemap(kernel_pagemap);
    max_vaddr += PAGE_SIZE;

    // Sets the start of the VMA, the first free region to start allocating.
    vma_set_start(pagemap, max_vaddr, 1);

    return hdr->entry;
}

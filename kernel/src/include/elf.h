#pragma once

#include <stdint.h>
#include <stddef.h>
#include <vmm.h>

typedef struct {
    unsigned char ident[16];
    uint16_t type;
    uint16_t isa;
    uint32_t elf_version;
    uint64_t entry;
    uint64_t phoff;
    uint64_t shoff;
    uint32_t flags;
    uint16_t hdr_size;
    uint16_t entry_ph_size;
    uint16_t entry_ph_count;
    uint16_t entry_sh_size;
    uint16_t entry_sh_count;
    uint16_t sh_names;
} elf_hdr_t;

typedef struct {
    uint32_t type;
    uint32_t flags;
    uint64_t offset;
    uint64_t vaddr;
    uint64_t paddr;
    uint64_t file_size;
    uint64_t mem_size;
    uint64_t align;
} elf_phdr_t;

typedef struct {
    uint32_t name;
    uint32_t type;
    uint64_t flags;
    uint64_t addr;
    uint64_t offset;
    uint64_t size;
    uint32_t link;
    uint32_t info;
    uint64_t align;
    uint64_t ent_size;
} elf_shdr_t;

uint64_t elf_load(uint8_t *data, pagemap_t *pagemap);

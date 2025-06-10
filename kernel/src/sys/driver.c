#include <driver.h>
#include <elf.h>
#include <pmm.h>
#include <stdio.h>
#include <string.h>
#include <kernel.h>
#include <heap.h>

void driver_load(uint8_t *elf) {
    elf_hdr_t *hdr = (elf_hdr_t*)elf;
    if (hdr->ident[0] != 0x7f || hdr->ident[1] != 'E' || hdr->ident[2] != 'L' || hdr->ident[3] != 'F')
        return;
    if (hdr->type != 1)
        return;
    elf_shdr_t *shdrs = (elf_shdr_t*)(elf + hdr->shoff);
    elf_shdr_t *shstrtab = &shdrs[hdr->shstrndx];
    char *strtab = (char*)(elf + shstrtab->offset);
    elf_shdr_t *shsymtab = NULL;
    for (int i = 1; i < hdr->shnum; i++) {
        elf_shdr_t *shdr = &shdrs[i];
        if (shdr->type == 8 && (shdr->flags & 0x2)) {
            // Alloc and zero out BSS
            void *addr = vmm_alloc(kernel_pagemap, DIV_ROUND_UP(shdr->size, PAGE_SIZE), false);
            memset(addr, 0, shdr->size);
            shdr->addr = (uint64_t)addr;
        }
        if (shdr->type == 1) {
            // Alloc and copy progbits
            if (!shdr->size) continue;
            void *addr = vmm_alloc(kernel_pagemap, DIV_ROUND_UP(shdr->size, PAGE_SIZE), false);
            memcpy(addr, elf + shdr->offset, shdr->size);
            shdr->addr = (uint64_t)addr;
            continue;
        }
        char *name = strtab + shdr->name;
        if (!strcmp(name, ".symtab"))
            shsymtab = &shdrs[i];
    }
    // Resolve references
    elf_sym_t *symtab = (elf_sym_t*)(elf + shsymtab->offset);
    char *symstrtab = (char*)(elf + shdrs[shsymtab->link].offset);

    elf_sym_t *entry_sym = NULL;

    for (int i = 1; i < shsymtab->size / sizeof(elf_sym_t); i++) {
        elf_sym_t *sym = &symtab[i];
        char *name = symstrtab + sym->name;
        if (sym->shndx == 0) {
            // Unresolved reference, solve it!
            uint64_t addr = kernel_find_sym(name);
            if (!addr) {
                printf("Failed to resolve reference %s\n", name);
                return;
            }
            sym->value = addr;
        } else if (sym->shndx != 0xfff1) {
            uint64_t base = shdrs[sym->shndx].addr;
            sym->value += base;
            if (!strcmp("drv_main", name)) {
                if (ELF64_ST_TYPE(sym->info) != 2) {
                    printf("Symbol drv_main is not a function.\n");
                    return;
                }
                entry_sym = sym;
            }
        }
    }

    // Now relocate the code
    for (int i = 1; i < hdr->shnum; i++) {
        elf_shdr_t *shdr = &shdrs[i];
        if (shdr->type != 4) continue;
        if ((shdrs[shdr->info].flags & 2) == 0) continue;
        elf_rela_t *relatab = (elf_rela_t*)(elf + shdr->offset);
        elf_shdr_t *shtarget = &shdrs[shdr->info];
        for (int j = 0; j < shdr->size / shdr->entsize; j++) {
            elf_sym_t *sym = &symtab[ELF64_R_SYM(relatab[j].info)];
            uint64_t target = relatab[j].offset + shtarget->addr;
            uint64_t s = symtab[ELF64_R_SYM(relatab[j].info)].value;
            uint64_t a = relatab[j].addend;
            uint64_t p = target;
            switch (ELF64_R_TYPE(relatab[j].info)) {
                case 1:
                    *(uint64_t*)target = s + a;
                    break;
                case 2:
                case 4:
                    *(uint32_t*)target = s + a - p;
                    break;
                case 10:
                    *(uint32_t*)target = s + a;
                    break;
                default:
                    printf("Unhandled relocation type %d.\n", ELF64_R_TYPE(relatab[j].info));
                    break;
            }
        }
    }
    void (*entry)(void) = (void(*)(void))entry_sym->value;
    entry();
}

void driver_load_node(vnode_t *node) {
    uint8_t *buffer = (uint8_t*)kmalloc(node->size);
    vfs_read(node, buffer, 0, node->size);
    driver_load(buffer);
    kfree(buffer);
}

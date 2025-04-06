#include <gdt.h>
#include <log.h>

gdt_table_t gdt_table = {
    {
        0x0000000000000000,
        0x00af9b000000ffff, // 0x08 64 bit cs (code)
        0x00af93000000ffff, // 0x10 64 bit ss (data)
        0x00affb000000ffff, // 0x18 user mode cs (code)
        0x00aff3000000ffff, // 0x20 user mode ss (data)
    }
};

gdt_desc_t gdt_desc;

void gdt_reload_seg();

void gdt_init() {
    gdt_desc.size = sizeof(gdt_table_t) - 1;
    gdt_desc.address = (uint64_t)&gdt_table;
    __asm__ volatile ("lgdt %0" : : "m"(gdt_desc) : "memory");
    gdt_reload_seg();
    LOG_OK("GDT Loaded.\n");
}

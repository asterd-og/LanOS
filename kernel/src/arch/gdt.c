#include <gdt.h>
#include <log.h>
#include <smp.h>

gdt_table_t gdt_tables[MAX_CPU];
gdt_desc_t gdt_desc[MAX_CPU];
tss_desc_t tss_desc[MAX_CPU];

void gdt_reload_seg();

void gdt_init(uint32_t cpu_num) {
    tss_desc[cpu_num].iopb = sizeof(tss_desc_t);
    gdt_tables[cpu_num].entries[0] = 0x0000000000000000;
    gdt_tables[cpu_num].entries[1] = 0x00af9b000000ffff; // 0x08 64 bit cs (code)
    gdt_tables[cpu_num].entries[2] = 0x00af93000000ffff; // 0x10 64 bit ss (data)
    gdt_tables[cpu_num].entries[3] = 0x00affb000000ffff; // 0x18 user mode cs (code)
    gdt_tables[cpu_num].entries[4] = 0x00aff3000000ffff; // 0x20 user mode ss (data)

    uint64_t tss = (uint64_t)&tss_desc[cpu_num];

    gdt_tables[cpu_num].tss_entry.len = sizeof(tss_entry_t);
    gdt_tables[cpu_num].tss_entry.base = (uint16_t)(tss & 0xffff);
    gdt_tables[cpu_num].tss_entry.base1 = (uint8_t)((tss >> 16) & 0xff);
    gdt_tables[cpu_num].tss_entry.flags = 0x89;
    gdt_tables[cpu_num].tss_entry.flags1 = 0;
    gdt_tables[cpu_num].tss_entry.base2 = (uint8_t)((tss >> 24) & 0xff);
    gdt_tables[cpu_num].tss_entry.base3 = (uint32_t)(tss >> 32);
    gdt_tables[cpu_num].tss_entry.resv = 0;

    gdt_desc[cpu_num].size = sizeof(gdt_table_t) - 1;
    gdt_desc[cpu_num].address = (uint64_t)&gdt_tables[cpu_num];
    __asm__ volatile ("lgdt %0" : : "m"(gdt_desc[cpu_num]) : "memory");
    __asm__ volatile ("ltr %0" : : "r"((uint16_t)0x28));
    gdt_reload_seg();
    if (cpu_num == 0)
        LOG_OK("GDT Loaded.\n");
}

void tss_set_rsp(uint32_t cpu_num, int rsp, void *stack) {
    tss_desc[cpu_num].rsp[rsp] = (uint64_t)stack;
}

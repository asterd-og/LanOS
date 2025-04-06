#include <idt.h>
#include <log.h>

const char *exception_messages[32] = {
    "Division by zero",
    "Debug",
    "Non-maskable interrupt",
    "Breakpoint",
    "Detected overflow",
    "Out-of-bounds",
    "Invalid opcode",
    "No coprocessor",
    "Double fault",
    "Coprocessor segment overrun",
    "Bad TSS",
    "Segment not present",
    "Stack fault",
    "General protection fault",
    "Page fault",
    "Unknown interrupt",
    "Coprocessor fault",
    "Alignment check",
    "Machine check",
    "Reserved",
    "Reserved",
    "Reserved",
    "Reserved",
    "Reserved",
    "Reserved",
    "Reserved",
    "Reserved",
    "Reserved",
    "Reserved",
    "Reserved",
    "Reserved",
    "Reserved"
};

__attribute__((aligned(16))) static idt_entry_t idt_entries[256];
static idt_desc_t idt_desc;
extern void *idt_int_table[];

void idt_set_entry(uint16_t vector, void *isr, uint8_t flags);

void idt_init() {
    for (uint16_t vector = 0; vector < 256; vector++)
        idt_set_entry(vector, idt_int_table[vector], 0x8E);

    idt_desc.size = sizeof(idt_entries) - 1;
    idt_desc.address = (uint64_t)&idt_entries;

    __asm__ volatile ("lidt %0" : : "m"(idt_desc) : "memory");
    __asm__ volatile ("sti");
    LOG_OK("IDT Initialised.\n");
}

void idt_set_entry(uint16_t vector, void *isr, uint8_t flags) {
    idt_entry_t *entry = &idt_entries[vector];
    entry->offset_low = (uint16_t)((uint64_t)isr & 0xFFFF);
    entry->selector = 0x08;
    entry->ist = 0;
    entry->flags = flags;
    entry->offset_mid = (uint16_t)(((uint64_t)isr >> 16) & 0xFFFF);
    entry->offset_high = (uint32_t)(((uint64_t)isr >> 32) & 0xFFFFFFFF);
    entry->resv = 0;
}

void idt_exception_handler(context_t *ctx) {
    LOG_ERROR("Kernel exception caught: %s.\n", exception_messages[ctx->int_no]);
    // TODO: Dump registers.
    __asm__ volatile ("cli\n\t.1:\n\thlt\n\tjmp .1\n");
}

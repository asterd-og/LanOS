#include <idt.h>
#include <log.h>
#include <apic.h>
#include <assert.h>
#include <sched.h>
#include <serial.h>

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
void *handlers[224];

void idt_set_entry(uint16_t vector, void *isr, uint8_t flags);

void idt_init() {
    for (uint16_t vector = 0; vector < 256; vector++)
        idt_set_entry(vector, idt_int_table[vector], 0x8e);

    idt_desc.size = sizeof(idt_entries) - 1;
    idt_desc.address = (uint64_t)&idt_entries;

    __asm__ volatile ("lidt %0" : : "m"(idt_desc) : "memory");
    __asm__ volatile ("sti");
    LOG_OK("IDT Initialised.\n");
}

void idt_reinit() {
    __asm__ volatile ("lidt %0" : : "m"(idt_desc) : "memory");
    __asm__ volatile ("sti");
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

void idt_set_ist(uint16_t vector, uint8_t ist) {
    idt_entries[vector].ist = ist;
}

void idt_install_irq(uint8_t irq, void *handler) {
    handlers[irq] = handler;
    if (irq < 16)
        ioapic_remap_irq(0, irq, irq + 32, false); // Right now we just map
                                                   // every interrupt to MP.
}

void idt_irq_handler(context_t *ctx) {
    void (*handler)(context_t*) = handlers[ctx->int_no - 32];
    if (!handler) {
        LOG_ERROR("Uncaught IRQ #%d.\n", ctx->int_no - 32);
        ASSERT(0);
    }
    handler(ctx);
}

void idt_exception_handler(context_t *ctx) {
    if (ctx->int_no >= 32)
        return idt_irq_handler(ctx);
    LOG_ERROR("Kernel exception caught: %s.\n", exception_messages[ctx->int_no]);
    serial_printf("Kernel crash on core %d at 0x%p. RSP: 0x%p\n", this_cpu()->id, ctx->rip, ctx->rsp);
    serial_printf("CS: 0x%x SS: 0x%x\n", ctx->cs, ctx->ss);
    // TODO: Dump registers.
    __asm__ volatile ("cli\n\t.1:\n\thlt\n\tjmp .1\n");
}

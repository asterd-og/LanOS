#include <pit.h>
#include <idt.h>
#include <apic.h>
#include <ports.h>
#include <stdio.h>

uint64_t counter = 0;

void pit_handler(void) {
    counter++;
    lapic_eoi();
}

void pit_init() {
    uint16_t div = 1193180 / 1000;
    outb(0x43, 0b110110);
    outb(0x40, div);
    outb(0x40, div >> 8);
    idt_install_irq(0, pit_handler);
}

void pit_sleep(uint64_t ms) {
    uint64_t sleep_until = counter + ms;
    while (counter < sleep_until)
        __asm__ volatile ("pause");
}

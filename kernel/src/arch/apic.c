#include <apic.h>
#include <smp.h>
#include <cpu.h>
#include <pmm.h>
#include <pit.h>
#include <madt.h>
#include <assert.h>

uint64_t lapic_address = 0;
bool x2apic = false;

void lapic_init() {
    uint64_t apic_msr = read_msr(0x1B);
    lapic_address = HIGHER_HALF(madt_apic_address);
    apic_msr |= 0x800; // Enable flag
    uint32_t a = 1, b = 0, c = 0, d = 0;
    __asm__ volatile ("cpuid" : "=a"(a), "=b"(b), "=c"(c), "=d"(d) : "a"(a));
    if (c & (1 << 21)) {
        apic_msr |= 0x400; // Enable x2apic
        x2apic = true;
    }
    write_msr(0x1B, apic_msr);
    lapic_write(0xF0, lapic_read(0xF0) | 0x100); // Spurious interrupt vector
}

uint64_t lapic_read(uint32_t reg) {
    if (x2apic) {
        reg = (reg >> 4) + 0x800;
        return read_msr(reg);
    }
    uint64_t addr = lapic_address + reg;
    return *((volatile uint32_t*)addr);
}

void lapic_write(uint32_t reg, uint64_t value) {
    if (x2apic) {
        reg = (reg >> 4) + 0x800;
        return write_msr(reg, value);
    }
    uint64_t addr = lapic_address + reg;
    *((volatile uint32_t*)addr) = value;
    return;
}

void lapic_eoi() {
    lapic_write(0xB0, 0);
}

void lapic_ipi(uint32_t lapic_id, uint32_t data) {
    if (x2apic) {
        lapic_write(0x300, ((uint64_t)lapic_id << 32) | data);
        return;
    }
    lapic_write(0x310, lapic_id << 24);
    lapic_write(0x300, data);
}

void lapic_ipi_all(uint32_t lapic_id, uint32_t vector) {
    lapic_ipi(lapic_id, vector | 0x80000);
}

void lapic_oneshot(uint32_t vector, uint64_t ms) {
    lapic_write(0x320, 0x10000);
    lapic_write(0x380, 0);
    lapic_write(0x3E0, 0x3);
    lapic_write(0x380, ms * this_cpu()->lapic_ticks);
    lapic_write(0x320, vector);
}

void lapic_stop_timer() {
    lapic_write(0x380, 0);
    lapic_write(0x320, 0x10000);
}

uint64_t lapic_init_timer() {
    lapic_write(0x3E0, 0x3);
    lapic_write(0x380, 0xffffffff);
    pit_sleep(1);
    lapic_write(0x320, 0x10000);
    uint64_t init_count = 0xffffffff - lapic_read(0x390);
    return init_count;
}

uint32_t lapic_get_id() {
    uint32_t id = lapic_read(0x20);
    if (!x2apic) id >>= 24;
    return id;
}

// I/O APIC

uint64_t ioapic_address = 0;
#define REDTBL(n) (0x10 + 2 * n)

uint32_t ioapic_read(uint8_t reg) {
    *(volatile uint32_t*)ioapic_address = reg;
    return *(volatile uint32_t*)(ioapic_address + 0x10);
}

void ioapic_write(uint8_t reg, uint32_t data) {
    *(volatile uint32_t*)ioapic_address = reg;
    *(volatile uint32_t*)(ioapic_address + 0x10) = data;
}

void ioapic_remap_gsi(uint32_t lapic_id, uint32_t gsi, uint8_t vec, uint32_t flags) {
    uint64_t value = vec;
    value |= flags;
    value |= (uint64_t)lapic_id << 56;
    ioapic_write(REDTBL(gsi), (uint32_t)value);
    ioapic_write(REDTBL(gsi)+1, (uint32_t)(value >> 32));
}

void ioapic_remap_irq(uint32_t lapic_id, uint8_t irq, uint8_t vec, bool masked) {
    madt_iso_t *iso = madt_iso_list[irq];
    if (!iso)
        return ioapic_remap_gsi(lapic_id, irq, vec, (masked ? 1 << 16 : 0));
    uint32_t flags = 0;
    if (iso->flags & (1 << 1)) flags |= 1 << 13;
    if (iso->flags & (1 << 3)) flags |= 1 << 15;
    if (masked) flags |= (1 << 16);
    return ioapic_remap_gsi(lapic_id, iso->gsi, vec, flags);
}

void ioapic_init() {
    ioapic_address = HIGHER_HALF((uint64_t)madt_ioapic->ioapic_addr);
    ASSERT(PHYSICAL(ioapic_address));
    uint32_t value = ioapic_read(0x01);
    uint32_t count = ((value >> 16) & 0xFF) + 1;
    for (uint32_t i = 0; i < count; i++) {
        uint32_t low = ioapic_read(REDTBL(i));
        ioapic_write(REDTBL(i), low | (1 << 16)); // Mask entry
    }
}

#pragma once

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <smp.h>

void lapic_init();
uint64_t lapic_read(uint32_t reg);
void lapic_write(uint32_t reg, uint64_t val);
void lapic_eoi();
void lapic_ipi(uint32_t lapic_id, uint32_t data);
void lapic_ipi_all(uint32_t lapic_id, uint32_t vector);
void lapic_ipi_others(uint32_t lapic_id, uint32_t vector);
void lapic_oneshot(uint32_t vector, uint64_t ms);
uint64_t lapic_init_timer();
void lapic_stop_timer();
uint32_t lapic_get_id();

void ioapic_init();
void ioapic_remap_irq(uint32_t lapic_id, uint8_t irq, uint8_t vec, bool masked);

#pragma once

#include <stdint.h>
#include <stddef.h>
#include <acpi.h>

typedef struct {
    sdt_header_t header;
    uint32_t lapic_address;
    uint32_t flags;
    char table[];
} __attribute__((packed)) madt_t;

typedef struct {
    uint8_t type;
    uint8_t len;
    char data[];
} __attribute__((packed)) madt_entry_t;

typedef struct {
    uint8_t acpi_cpu_id;
    uint8_t apic_id;
    uint32_t flags;
} __attribute__((packed)) madt_lapic_t;

typedef struct {
    uint8_t ioapic_id;
    uint8_t resv;
    uint32_t ioapic_addr;
    uint32_t gsi_base;
} __attribute__((packed)) madt_ioapic_t;

typedef struct {
    uint8_t bus;
    uint8_t irq;
    uint32_t gsi;
    uint16_t flags;
} __attribute__((packed)) madt_iso_t;

extern madt_ioapic_t *madt_ioapic;
extern madt_iso_t *madt_iso_list[16];
extern uint64_t madt_apic_address;

void madt_init();

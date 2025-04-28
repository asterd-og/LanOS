#pragma once

#include <stdint.h>
#include <stddef.h>
#include <acpi.h>

typedef struct {
    uint64_t base_addr;
    uint16_t pci_seg;
    uint8_t start_bus;
    uint8_t end_bus;
    uint32_t rsv;
} __attribute__((packed)) mcfg_entry_t;

typedef struct {
    sdt_header_t header;
    uint64_t rsv;
    mcfg_entry_t entries[];
} __attribute__((packed)) mcfg_t;

typedef struct {
    uint16_t vendor_id;
    uint16_t device_id;
    uint16_t cmd;
    uint16_t status;
    uint8_t rev;
    uint8_t prog_if;
    uint8_t subclass;
    uint8_t _class;
    uint8_t cache_line_size;
    uint8_t lat_timer;
    uint8_t type;
    uint8_t bist;
} __attribute__((packed)) pci_header_t;

typedef struct {
    pci_header_t header;
    uint32_t bars[6];
    uint32_t cardbus;
    uint16_t subsys_vendor_id;
    uint16_t subsys_id;
    uint32_t expansion_rom;
    uint8_t cap;
    uint32_t resv : 24;
    uint32_t resv1;
    uint8_t int_line;
    uint8_t int_pin;
    uint8_t min_grant;
    uint8_t max_lat;
} __attribute__((packed)) pci_header0_t;

void pci_init();
pci_header_t *pci_get_dev(uint8_t _class, uint8_t subclass);

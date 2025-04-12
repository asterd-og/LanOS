#pragma once

#include <stdint.h>
#include <stddef.h>

typedef struct {
    char signature[8];
    uint8_t checksum;
    char id[6];
    uint8_t rev;
    uint32_t rsdt;
} __attribute__((packed)) rsdp_t;

typedef struct {
    char signature[8];
    uint8_t checksum;
    char id[6];
    uint8_t rev;
    uint32_t rsdt;
    uint32_t len;
    uint64_t xsdt;
    uint8_t ext_checksum;
    uint8_t resv[3];
} __attribute__((packed)) xsdp_t;

typedef struct {
    char signature[4];
    uint32_t len;
    uint8_t rev;
    uint8_t checksum;
    char id[6];
    char table_id[8];
    uint32_t oem_rev;
    uint32_t creator_id;
    uint32_t creator_rev;
} __attribute__((packed)) sdt_header_t;

typedef struct {
    sdt_header_t header;
    char table[];
} __attribute__((packed)) sdt_t;

void acpi_init();
void *acpi_find(const char *signature);

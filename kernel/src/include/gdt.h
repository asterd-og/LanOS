#pragma once

#include <stdint.h>
#include <stddef.h>

typedef struct {
    uint64_t entries[5];
} __attribute__((packed)) gdt_table_t;

typedef struct {
    uint16_t size;
    uint64_t address;
} __attribute__((packed)) gdt_desc_t;

void gdt_init();

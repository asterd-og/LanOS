#pragma once

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <vmm.h>

typedef struct {
    uint32_t id;
    uint64_t lapic_ticks;
    pagemap_t *pagemap;
} cpu_t;

extern cpu_t *smp_cpu_list[256];

void smp_init();
cpu_t *this_cpu();

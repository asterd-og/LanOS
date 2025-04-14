#pragma once

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <sched.h>
#include <vmm.h>

typedef struct {
    uint32_t id;
    uint64_t lapic_ticks;
    pagemap_t *pagemap;
    task_t *task_idle;
    task_t *current_task;
} cpu_t;

extern cpu_t *smp_cpu_list[256];
extern bool smp_started;

void smp_init();
cpu_t *this_cpu();
cpu_t *get_cpu(uint32_t id);

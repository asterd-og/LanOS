#pragma once

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <sched.h>
#include <vmm.h>

#define MAX_CPU 128

typedef struct {
    uint32_t id;
    uint64_t lapic_ticks;
    pagemap_t *pagemap;
    task_t *task_idle;
    task_t *current_task;
    uint64_t task_count;
} cpu_t;

extern int smp_last_cpu;
extern cpu_t *smp_cpu_list[MAX_CPU];
extern bool smp_started;

void smp_init();
cpu_t *this_cpu();
cpu_t *get_cpu(uint32_t id);

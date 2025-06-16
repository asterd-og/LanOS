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
    thread_t *thread_head;
    thread_t *current_thread;
    uint64_t thread_count;
    int sched_lock;
    bool has_runnable_thread;
} cpu_t;

extern uint32_t smp_bsp_cpu;

extern int smp_last_cpu;
extern cpu_t *smp_cpu_list[MAX_CPU];
extern bool smp_started;

void smp_init();
cpu_t *this_cpu();
cpu_t *get_cpu(uint32_t id);

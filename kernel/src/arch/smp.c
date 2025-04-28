#include "vmm.h"
#include <smp.h>
#include <heap.h>
#include <log.h>
#include <limine.h>
#include <spinlock.h>
#include <madt.h>
#include <cpu.h>
#include <gdt.h>
#include <idt.h>
#include <apic.h>

__attribute__((used, section(".limine_requests")))
static volatile struct limine_mp_request limine_mp = {
    .id = LIMINE_MP_REQUEST,
    .revision = 0
};

cpu_t *smp_cpu_list[256];
NEW_LOCK(smp_lock);
uint64_t started_count = 0;
bool smp_started = false;

void smp_cpu_init(struct limine_mp_info *mp_info) {
    vmm_switch_pagemap(kernel_pagemap);
    spinlock_lock(&smp_lock);
    gdt_reinit();
    idt_reinit();
    smp_cpu_list[mp_info->lapic_id]->id = mp_info->lapic_id;
    lapic_init();
    smp_cpu_list[mp_info->lapic_id]->lapic_ticks = lapic_init_timer();
    smp_cpu_list[mp_info->lapic_id]->task_idle = NULL;
    smp_cpu_list[mp_info->lapic_id]->pagemap = kernel_pagemap;
    sched_install();
    LOG_OK("Initialized CPU %d.\n", mp_info->lapic_id);
    started_count++;
    spinlock_free(&smp_lock);
    while (1)
        __asm__ volatile ("hlt");
}

void smp_init() {
    struct limine_mp_response *mp_response = limine_mp.response;
    cpu_t *bsp_cpu = (cpu_t*)kmalloc(sizeof(cpu_t));
    bsp_cpu->id = mp_response->bsp_lapic_id;
    bsp_cpu->pagemap = kernel_pagemap;
    bsp_cpu->lapic_ticks = lapic_init_timer();
    bsp_cpu->task_idle = NULL;
    smp_cpu_list[bsp_cpu->id] = bsp_cpu;
    LOG_INFO("Detected %zu CPUs.\n", mp_response->cpu_count);
    for (uint64_t i = 0; i < mp_response->cpu_count; i++) {
        struct limine_mp_info *mp_info = mp_response->cpus[i];
        if (i == bsp_cpu->id)
            continue;
        smp_cpu_list[mp_info->lapic_id] = (cpu_t*)kmalloc(sizeof(cpu_t));
        mp_info->goto_address = smp_cpu_init;
        mp_info->extra_argument = (uint64_t)mp_info;
    }
    while (started_count < mp_response->cpu_count - 1)
        __asm__ volatile ("pause");
    smp_started = true;
    LOG_OK("SMP Initialised.\n");
}

cpu_t *this_cpu() {
    return smp_cpu_list[lapic_get_id()];
}

cpu_t *get_cpu(uint32_t id) {
    return smp_cpu_list[id];
}

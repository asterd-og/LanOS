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
#include <assert.h>
#include <pmm.h>
#include <syscall.h>
#include <string.h>

__attribute__((used, section(".limine_requests")))
static volatile struct limine_mp_request limine_mp = {
    .id = LIMINE_MP_REQUEST,
    .revision = 0
};

cpu_t *smp_cpu_list[MAX_CPU];
NEW_LOCK(smp_lock);
uint64_t started_count = 0;
bool smp_started = false;
int smp_last_cpu = 0;
uint32_t smp_bsp_cpu;

void smp_setup_kstack(cpu_t *cpu) {
    void *stack = (void*)vmm_alloc(kernel_pagemap, 8, true);
    tss_set_ist(cpu->id, 0, (void*)((uint64_t)stack + 8 * PAGE_SIZE));
    tss_set_rsp(cpu->id, 0, (void*)((uint64_t)stack + 8 * PAGE_SIZE));
}

void smp_setup_thread_queue(cpu_t *cpu) {
    memset(cpu->thread_queues, 0, THREAD_QUEUE_CNT * sizeof(thread_queue_t));
    uint64_t quantum = 5;
    for (int i = 0; i < THREAD_QUEUE_CNT; i++) {
        cpu->thread_queues[i].quantum = quantum;
        quantum += 5;
    }
}

void smp_cpu_init(struct limine_mp_info *mp_info) {
    vmm_switch_pagemap(kernel_pagemap);
    spinlock_lock(&smp_lock);
    gdt_init(mp_info->lapic_id);
    idt_reinit();
    cpu_t *cpu = smp_cpu_list[mp_info->lapic_id];
    cpu->id = mp_info->lapic_id;
    lapic_init();
    cpu->lapic_ticks = lapic_init_timer();
    cpu->pagemap = kernel_pagemap;
    cpu->thread_count = 0;
    cpu->sched_lock = 0;
    cpu->has_runnable_thread = false;
    syscall_init();
    smp_setup_thread_queue(cpu);
    smp_setup_kstack(cpu);
    cpu_enable_sse();
    LOG_OK("Initialized CPU %d.\n", mp_info->lapic_id);
    started_count++;
    if (mp_info->lapic_id > smp_last_cpu) smp_last_cpu = mp_info->lapic_id;
    spinlock_free(&smp_lock);
    while (1)
        __asm__ volatile ("hlt");
}

void smp_init() {
    struct limine_mp_response *mp_response = limine_mp.response;
    if (mp_response->cpu_count > MAX_CPU) {
        LOG_ERROR("The system has more CPUs (%d) than allowed. Change MAX_CPU on smp.h\n", mp_response->cpu_count);
        ASSERT(0);
    }
    cpu_t *bsp_cpu = (cpu_t*)kmalloc(sizeof(cpu_t));
    bsp_cpu->id = mp_response->bsp_lapic_id;
    bsp_cpu->pagemap = kernel_pagemap;
    bsp_cpu->lapic_ticks = lapic_init_timer();
    bsp_cpu->thread_count = 0;
    bsp_cpu->sched_lock = 0;
    bsp_cpu->has_runnable_thread = false;
    smp_cpu_list[bsp_cpu->id] = bsp_cpu;
    smp_bsp_cpu = bsp_cpu->id;
    smp_setup_thread_queue(bsp_cpu);
    smp_setup_kstack(bsp_cpu);
    cpu_enable_sse();
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
    if (!smp_started) return NULL;
    return smp_cpu_list[lapic_get_id()];
}

cpu_t *get_cpu(uint32_t id) {
    return smp_cpu_list[id];
}

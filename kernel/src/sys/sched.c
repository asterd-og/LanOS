#include <sched.h>
#include <string.h>
#include <apic.h>
#include <heap.h>
#include <smp.h>
#include <pmm.h>
#include <log.h>

void sched_init() {
    idt_install_irq(16, sched_switch);
}

void sched_idle() {
    while (1) {
    }
}

void sched_install() {
    task_t *task_idle = sched_new_task(this_cpu()->id, sched_idle);
    this_cpu()->current_task = NULL;
}

task_t *sched_new_task(uint32_t cpu_num, void *entry) {
    task_t *task = (task_t*)kmalloc(sizeof(task_t));

    uint64_t stack = HIGHER_HALF((uint64_t)pmm_request());
    memset((void*)stack, 0, 4096);

    task->cpu_num = cpu_num;
    task->stack_top = stack;
    task->ctx.rip = (uint64_t)entry;
    task->ctx.rsp = stack + 4096;
    task->ctx.cs = 0x08;
    task->ctx.ss = 0x10;
    task->ctx.rflags = 0x202;
    task->pagemap = vmm_new_pagemap();
    avl_t *avl_root = NULL;
    vma_region_t region = {
        .start = 0x0,
        .page_count = 0x100000,
        .flags = MM_READ | MM_WRITE
    };
    avl_root = avl_insert_node(avl_root, 0x100000, &region);
    task->pagemap->avl_root = avl_root;

    cpu_t *cpu = get_cpu(cpu_num);
    if (cpu->task_idle == NULL) {
        task->next = task;
        task->prev = task;
        cpu->task_idle = task;
        return task;
    }

    task->next = cpu->task_idle;
    task->prev = cpu->task_idle->prev;
    cpu->task_idle->prev->next = task;
    cpu->task_idle->prev = task;

    return task;
}

void sched_switch(context_t *ctx) {
    cpu_t *cpu = this_cpu();
    task_t *switch_to = NULL;
    if (cpu->current_task) {
        cpu->current_task->ctx = *ctx;
        switch_to = cpu->current_task->next;
    } else
        switch_to = cpu->task_idle;
    cpu->current_task = switch_to;
    *ctx = switch_to->ctx;
    vmm_switch_pagemap(switch_to->pagemap);
    lapic_eoi();
    lapic_oneshot(SCHED_VEC, TASK_QUANTUM);
}

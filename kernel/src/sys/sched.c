#include "vmm.h"
#include <sched.h>
#include <string.h>
#include <apic.h>
#include <heap.h>
#include <smp.h>
#include <pmm.h>
#include <log.h>
#include <elf.h>
#include <gdt.h>
#include <spinlock.h>

NEW_LOCK(sched_lock);

uint64_t current_id = 0;

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

    task->id = current_id++;
    task->cpu_num = cpu_num;
    task->stack = stack;
    task->ctx.rip = (uint64_t)entry;
    task->ctx.rsp = stack + PAGE_SIZE;
    task->ctx.cs = 0x08;
    task->ctx.ss = 0x10;
    task->ctx.rflags = 0x202;
    task->pagemap = vmm_new_pagemap();
    task->user = false;

    vma_set_start(task->pagemap, 0, 1);

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

task_t *sched_load_elf(uint32_t cpu_num, vnode_t *node, int argc, char *argv[]) {
    pagemap_t *old_pagemap = vmm_switch_pagemap(kernel_pagemap);
    task_t *task = (task_t*)kmalloc(sizeof(task_t));
    uint8_t *buffer = (uint8_t*)kmalloc(node->size);
    vfs_read(node, buffer, node->size);

    task->id = current_id++;
    task->cpu_num = cpu_num;
    task->pagemap = vmm_new_pagemap();

    task->ctx.rip = (uint64_t)elf_load(buffer, task->pagemap);

    uint64_t stack = (uint64_t)vmm_alloc(task->pagemap, 8, true);
    vmm_switch_pagemap(task->pagemap);
    memset((void*)stack, 0, PAGE_SIZE);
    vmm_switch_pagemap(old_pagemap);

    char **kernel_argv = (char**)kmalloc(argc * 8);
    for (int i = 0; i < argc; i++) {
        kernel_argv[i] = (char*)kmalloc(strlen(argv[i]) + 1);
        memcpy(kernel_argv[i], argv[i], strlen(argv[i]) + 1);
    }

    char **user_argv = (char**)vmm_alloc(task->pagemap, DIV_ROUND_UP(argc * 8, PAGE_SIZE), true);
    vmm_switch_pagemap(task->pagemap);
    for (int i = 0; i < argc; i++) {
        user_argv[i] = (char*)vmm_alloc(task->pagemap, DIV_ROUND_UP(strlen(kernel_argv[i]) + 1, PAGE_SIZE), true);
        memcpy(user_argv[i], kernel_argv[i], strlen(kernel_argv[i]) + 1);
    }
    vmm_switch_pagemap(old_pagemap);

    for (int i = 0; i < argc; i++)
        vmm_free(kernel_pagemap, kernel_argv[i]);
    vmm_free(kernel_pagemap, kernel_argv);

    uint64_t kernel_stack = (uint64_t)vmm_alloc(kernel_pagemap, 2, false);
    memset((void*)kernel_stack, 0, PAGE_SIZE);

    task->kernel_stack = kernel_stack;
    task->kernel_rsp = kernel_stack + (PAGE_SIZE * 2);

    kfree(buffer);
    task->ctx.rsp = stack + (PAGE_SIZE * 8);
    task->ctx.rdi = argc;
    task->ctx.rsi = (uint64_t)user_argv;
    task->ctx.cs = 0x1b; // 0x18 | 3
    task->ctx.ss = 0x23; // 0x20 | 3
    task->ctx.rflags = 0x202;
    task->stack = stack;
    task->handle_count = 0;
    task->user = true;

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
    if (switch_to->user)
        tss_set_rsp(cpu->id, 0, (void*)switch_to->kernel_rsp);
    vmm_switch_pagemap(switch_to->pagemap);
    lapic_eoi();
    lapic_oneshot(SCHED_VEC, TASK_QUANTUM);
}

task_t *this_task() {
    return this_cpu()->current_task;
}

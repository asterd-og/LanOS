#pragma once

#include <stdint.h>
#include <stddef.h>
#include <idt.h>
#include <vmm.h>
#include <vfs.h>

#define TASK_QUANTUM 25
#define SCHED_VEC 48

typedef struct task_t {
    uint64_t id;
    uint32_t cpu_num;
    uint64_t stack_top;
    context_t ctx;
    pagemap_t *pagemap;
    struct task_t *next;
    struct task_t *prev;
    vnode_t *handles[128];
    int handle_count;
} task_t;

void sched_init();
void sched_install();
task_t *sched_new_task(uint32_t cpu, void *entry);
task_t *sched_load_elf(uint32_t cpu, vnode_t *node);
void sched_switch(context_t *ctx);
task_t *this_task();

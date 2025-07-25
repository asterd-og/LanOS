#include <sched.h>
#include <string.h>
#include <apic.h>
#include <heap.h>
#include <smp.h>
#include <pmm.h>
#include <elf.h>
#include <gdt.h>
#include <spinlock.h>
#include <log.h>
#include <cpu.h>
#include <serial.h>

uint64_t sched_tid = 0;
uint64_t sched_pid = 0;

proc_t *sched_proclist[256] = { 0 };

void sched_preempt(context_t *ctx);
void sched_switch(context_t *ctx);

void sched_init() {
    idt_install_irq(16, sched_preempt);
    idt_install_irq(17, sched_switch);
    idt_set_ist(SCHED_VEC, 1);
    idt_set_ist(SCHED_VEC+1, 1);
}

void sched_idle() {
    while (1) {
        sched_yield();
    }
}

void sched_install() {
    for (uint32_t i = 0; i <= smp_last_cpu; i++) {
        cpu_t *cpu = smp_cpu_list[i];
        if (!cpu || cpu->id == smp_bsp_cpu)
            continue;
        proc_t *proc = sched_new_proc(false);
        thread_t *thread = sched_new_kthread(proc, cpu->id, THREAD_QUEUE_CNT - 1, sched_idle);
    }
}

proc_t *sched_new_proc(bool user) {
    proc_t *proc = (proc_t*)kmalloc(sizeof(proc_t));
    proc->id = sched_pid++;
    proc->cwd = root_node;
    proc->threads = NULL;
    proc->parent = NULL;
    proc->children = proc->sibling = NULL;
    proc->pagemap = (user ? vmm_new_pagemap() : kernel_pagemap);
    memset(proc->sig_handlers, 0, 64 * sizeof(sigaction_t));
    memset(proc->fd_table, 0, 256 * 8);
    proc->fd_table[0] = fd_open("/dev/pts0", O_RDONLY);
    proc->fd_table[1] = fd_open("/dev/pts0", O_WRONLY);
    proc->fd_table[2] = fd_open("/dev/pts0", O_WRONLY);
    proc->fd_table[3] = fd_open("/dev/serial", O_WRONLY);
    proc->fd_count = 4;
    sched_proclist[proc->id] = proc;
    return proc;
}

void sched_add_thread(cpu_t *cpu, thread_t *thread) {
    cpu->thread_count++;
    thread_queue_t *queue = &cpu->thread_queues[thread->priority];
    if (!queue->head) {
        thread->list_next = thread;
        thread->list_prev = thread;
        queue->head = thread;
        queue->current = thread;
        return;
    }

    thread->list_next = queue->head;
    thread->list_prev = queue->head->list_prev;
    queue->head->list_prev->list_next = thread;
    queue->head->list_prev = thread;
}

void sched_proc_add_thread(proc_t *parent, thread_t *thread) {
    if (!parent->threads) {
        parent->threads = thread;
        thread->next = thread;
        thread->prev = thread;
        return;
    }
    thread->next = parent->threads;
    thread->prev = parent->threads->prev;
    parent->threads->prev->next = thread;
    parent->threads->prev = thread;
}

void sched_prepare_user_stack(thread_t *thread, int argc, char *argv[], char *envp[]) {
    // Copy the arguments and envp into kernel memory
    char **kernel_argv = (char**)kmalloc(argc * 8);
    for (int i = 0; i < argc; i++) {
        int size = strlen(argv[i]) + 1;
        kernel_argv[i] = (char*)kmalloc(size);
        memcpy(kernel_argv[i], argv[i], size);
    }

    int envc = 0;
    while (envp[envc++]);
    envc -= 1;

    char **kernel_envp = (char**)kmalloc(envc * 8);
    for (int i = 0; i < envc; i++) {
        int size = strlen(envp[i]) + 1;
        kernel_envp[i] = (char*)kmalloc(size);
        memcpy(kernel_envp[i], envp[i], size);
    }

    // Copy the arguments to the thread stack.
    uint64_t thread_argv[argc];
    uint64_t stack_top = thread->ctx.rsp;
    uint64_t offset = 0;
    if ((argc + envc) % 2 == 0) offset = 8;
    pagemap_t *restore = vmm_switch_pagemap(thread->pagemap);
    for (int i = 0; i < argc; i++) {
        int size = strlen(kernel_argv[i]) + 1;
        offset += ALIGN_UP(size, 16); // Keep aligned to 16 bytes (ABI requirement)
        thread_argv[i] = stack_top - offset;
        memcpy((void*)(stack_top - offset), kernel_argv[i], size);
    }

    // Copy the environment variables to the thread stack.
    uint64_t thread_envp[envc];
    for (int i = 0; i < envc; i++) {
        int size = strlen(kernel_envp[i]) + 1;
        offset += ALIGN_UP(size, 16);
        thread_envp[i] = stack_top - offset;
        memcpy((void*)(stack_top - offset), kernel_envp[i], size);
    }

    // Set up argv and argc
    offset += 8;
    *(uint64_t*)(stack_top - offset) = 0; // envp[envc] = NULL

    for (int i = envc - 1; i >= 0; i--) {
        offset += 8;
        *(uint64_t*)(stack_top - offset) = thread_envp[i];
    }

    offset += 8;
    *(uint64_t*)(stack_top - offset) = 0; // argv[argc] = NULL

    for (int i = argc - 1; i >= 0; i--) {
        offset += 8;
        *(uint64_t*)(stack_top - offset) = thread_argv[i];
    }

    offset += 8;
    *(uint64_t*)(stack_top - offset) = argc;

    vmm_switch_pagemap(restore);

    thread->ctx.rsp = stack_top - offset;

    for (int i = 0; i < argc; i++)
        kfree(kernel_argv[i]);
    kfree(kernel_argv);
    for (int i = 0; i < envc; i++)
        kfree(kernel_envp[i]);
    kfree(kernel_envp);
}

thread_t *sched_new_kthread(proc_t *parent, uint32_t cpu_num, int priority, void *entry) {
    thread_t *thread = (thread_t*)kmalloc(sizeof(thread_t));
    thread->id = sched_tid++;
    thread->cpu_num = cpu_num;
    thread->parent = parent;
    thread->pagemap = parent->pagemap;
    thread->flags = 0;
    thread->priority = (priority > (THREAD_QUEUE_CNT - 1) ? (THREAD_QUEUE_CNT - 1) : priority);
    sched_proc_add_thread(parent, thread);
    thread->sig_deliver = 0;
    thread->sig_mask = 0;

    // Fx area
    thread->fx_area = vmm_alloc(kernel_pagemap, 1, true);
    memset(thread->fx_area, 0, 512);
    *(uint16_t *)(thread->fx_area + 0x00) = 0x037F;
    *(uint32_t *)(thread->fx_area + 0x18) = 0x1F80;

    // Stack (4 KB)
    uint64_t kernel_stack = (uint64_t)vmm_alloc(kernel_pagemap, 4, false);
    memset((void*)kernel_stack, 0, 4 * PAGE_SIZE);

    thread->kernel_stack = kernel_stack;
    thread->kernel_rsp = kernel_stack + (PAGE_SIZE * 4);
    thread->stack = kernel_stack;

    thread->ctx.rip = (uint64_t)entry;
    thread->ctx.cs = 0x08;
    thread->ctx.ss = 0x10;
    thread->ctx.rflags = 0x202;
    thread->ctx.rsp = thread->kernel_rsp;
    thread->thread_stack = thread->ctx.rsp;
    thread->fs = 0;

    thread->state = THREAD_RUNNING;
    get_cpu(cpu_num)->has_runnable_thread = true;

    sched_add_thread(get_cpu(cpu_num), thread);

    return thread;
}

thread_t *sched_new_thread(proc_t *parent, uint32_t cpu_num, int priority, vnode_t *node, int argc, char *argv[], char *envp[]) {
    thread_t *thread = (thread_t*)kmalloc(sizeof(thread_t));
    thread->id = sched_tid++;
    thread->cpu_num = cpu_num;
    thread->parent = parent;
    thread->pagemap = parent->pagemap;
    thread->flags = 0;
    thread->priority = (priority > (THREAD_QUEUE_CNT - 1) ? (THREAD_QUEUE_CNT - 1) : priority);

    sched_proc_add_thread(parent, thread);

    thread->sig_deliver = 0;
    thread->sig_mask = 0;

    // Load ELF
    uint8_t *buffer = (uint8_t*)kmalloc(node->size);
    vfs_read(node, buffer, 0, node->size);
    thread->ctx.rip = elf_load(buffer, thread->pagemap);

    // Fx area
    thread->fx_area = vmm_alloc(kernel_pagemap, 1, true);
    memset(thread->fx_area, 0, 512);
    *(uint16_t *)(thread->fx_area + 0x00) = 0x037F;
    *(uint32_t *)(thread->fx_area + 0x18) = 0x1F80;

    // Kernel stack (16 KB)
    uint64_t kernel_stack = (uint64_t)vmm_alloc(kernel_pagemap, 4, false);
    memset((void*)kernel_stack, 0, 4 * PAGE_SIZE);

    thread->kernel_stack = kernel_stack;
    thread->kernel_rsp = kernel_stack + (PAGE_SIZE * 4);

    // Thread stack (32 KB)
    uint64_t thread_stack = (uint64_t)vmm_alloc(thread->pagemap, 8, true);
    uint64_t thread_stack_top = thread_stack + 8 * PAGE_SIZE;
    thread->stack = thread_stack;

    // Sig stack (4 KB)
    uint64_t sig_stack = (uint64_t)vmm_alloc(thread->pagemap, 1, true);
    thread->sig_stack = sig_stack;

    // Set up the rest of the registers
    thread->ctx.cs = 0x23;
    thread->ctx.ss = 0x1b;
    thread->ctx.rflags = 0x202;

    // Set up stack (argc, argv, env)
    thread->ctx.rsp = thread_stack_top;
    sched_prepare_user_stack(thread, argc, argv, envp);
    thread->thread_stack = thread->ctx.rsp;

    thread->fs = 0;

    thread->state = THREAD_RUNNING;
    get_cpu(cpu_num)->has_runnable_thread = true;

    sched_add_thread(get_cpu(cpu_num), thread);

    return thread;
}

cpu_t *get_lw_cpu() {
    cpu_t *cpu = NULL;
    for (int i = 0; i < smp_last_cpu; i++) {
        if (smp_cpu_list[i] == NULL || i == smp_bsp_cpu) continue;
        if (!cpu) {
            cpu = smp_cpu_list[i];
            continue;
        }
        if (smp_cpu_list[i]->thread_count < cpu->thread_count)
            cpu = smp_cpu_list[i];
    }
    return cpu;
}

uint64_t sched_get_rip();

thread_t *sched_fork_thread(proc_t *proc, thread_t *parent, syscall_frame_t *frame) {
    thread_t *thread = (thread_t*)kmalloc(sizeof(thread_t));
    cpu_t *cpu = get_lw_cpu();
    spinlock_lock(&cpu->sched_lock);
    thread->id = sched_tid++;
    thread->cpu_num = cpu->id;
    thread->parent = proc;
    thread->pagemap = proc->pagemap;
    thread->flags = 0;
    thread->fx_area = vmm_alloc(kernel_pagemap, 1, true);
    memcpy(thread->fx_area, parent->fx_area, 512);

    sched_proc_add_thread(proc, thread);

    thread->sig_deliver = 0;
    thread->sig_mask = parent->sig_mask;

    // Set up RIP
    uint64_t kernel_stack = (uint64_t)vmm_alloc(kernel_pagemap, 4, false);
    memcpy((void*)kernel_stack, (void*)parent->kernel_stack, 4 * PAGE_SIZE);

    thread->kernel_stack = kernel_stack;
    thread->kernel_rsp = kernel_stack + 4 * PAGE_SIZE;

    thread->stack = parent->stack;

    // Sig stack (4 KB)
    thread->sig_stack = parent->sig_stack;

    // Set up the rest of the registers
    memcpy(&thread->ctx, frame, sizeof(context_t));
    thread->ctx.rsp = parent->thread_stack;
    thread->ctx.cs = 0x23;
    thread->ctx.ss = 0x1b;
    thread->ctx.rflags = frame->r11;
    thread->ctx.rax = 0;
    thread->ctx.rip = frame->rcx;

    // Set up stack (argc, argv, env)
    thread->thread_stack = parent->thread_stack;

    thread->fs = read_msr(FS_BASE);

    thread->state = THREAD_RUNNING;
    cpu->has_runnable_thread = true;
    
    sched_add_thread(cpu, thread);
    spinlock_free(&cpu->sched_lock);

    return thread;
}

proc_t *sched_fork_proc() {
    proc_t *parent = this_proc();
    proc_t *proc = (proc_t*)kmalloc(sizeof(proc_t));
    proc->id = sched_pid++;
    proc->cwd = parent->cwd;
    proc->threads = NULL;
    proc->parent = parent;
    proc->sibling = NULL;
    if (!parent->children) parent->children = proc;
    else {
        proc_t *last_sibling = parent->children;
        while (last_sibling->sibling != NULL)
            last_sibling = last_sibling->sibling;
        last_sibling->sibling = proc;
        proc->sibling = NULL;
    }
    proc->pagemap = vmm_fork(parent->pagemap);
    memcpy(proc->sig_handlers, parent->sig_handlers, 64 * sizeof(sigaction_t));
    memcpy(proc->fd_table, parent->fd_table, 256 * 8);
    proc->fd_count = parent->fd_count;
    sched_proclist[proc->id] = proc;
    return proc;
}

int sched_demote(cpu_t *cpu, thread_t *thread) {
    if (thread->priority == THREAD_QUEUE_CNT - 1)
        return 1;
    serial_printf("Demoted thread %d to queue %d.\n", thread->id, thread->priority);
    thread_queue_t *old_queue = &cpu->thread_queues[thread->priority];
    thread->priority++;
    thread_queue_t *new_queue = &cpu->thread_queues[thread->priority];
    if (thread->list_next == thread) {
        old_queue->head = NULL;
        old_queue->current = NULL;
    } else {
        if (old_queue->head == thread)
            old_queue->head = thread->list_next;
        if (old_queue->current == thread)
            old_queue->current = thread->list_next;
        thread->list_next->list_prev = thread->list_prev;
        thread->list_prev->list_next = thread->list_next;
    }
    sched_add_thread(cpu, thread);
    return 0;
}

thread_t *sched_pick(cpu_t *cpu) {
    for (uint32_t i = 0; i < THREAD_QUEUE_CNT; i++) {
        thread_queue_t *queue = &cpu->thread_queues[i];
        if (!queue->head)
            continue;
        thread_t *start = queue->head;
        thread_t *thread = queue->current;
        thread_t *next;
        bool found = false;
        do {
            next = thread->list_next;
            if (!(thread->state == THREAD_RUNNING)) {
                thread = next;
                continue;
            }
            if (!(thread->flags & TFLAGS_PREEMPTED)) {
                thread->preempt_count = 0;
                found = true;
                break;
            }
            thread->preempt_count++;
            if (thread->preempt_count == SCHED_PREEMPTION_MAX) {
                int ret = sched_demote(cpu, thread);
                thread->preempt_count = 0;
                thread->flags &= ~TFLAGS_PREEMPTED;
                if (ret == 1) {
                    i = 0;
                    break;
                }
            } else {
                found = true;
                break;
            }
            thread = next;
        } while (thread != queue->head);
        if (found) {
            queue->current = thread->next;
            thread->flags &= ~TFLAGS_PREEMPTED;
            return thread;
        }
    }
    return NULL;
}

void sched_switch(context_t *ctx) {
    lapic_stop_timer();
    cpu_t *cpu = this_cpu();
    spinlock_lock(&cpu->sched_lock);
    if (cpu->current_thread) {
        thread_t *thread = cpu->current_thread;
        thread->fs = read_msr(FS_BASE);
        thread->ctx = *ctx;
        __asm__ volatile ("fxsave (%0)" : : "r"(thread->fx_area));
    }
    thread_t *next_thread = sched_pick(cpu);
    cpu->current_thread = next_thread;
    *ctx = next_thread->ctx;
    vmm_switch_pagemap(next_thread->pagemap);
    write_msr(FS_BASE, next_thread->fs);
    write_msr(KERNEL_GS_BASE, (uint64_t)next_thread);
    __asm__ volatile ("fxrstor (%0)" : : "r"(next_thread->fx_area));
    spinlock_free(&cpu->sched_lock);
    // An ideal thread wouldn't need the timer to preempt.
    lapic_oneshot(SCHED_VEC, cpu->thread_queues[next_thread->priority].quantum);
    lapic_eoi();
}

void sched_preempt(context_t *ctx) {
    this_thread()->flags |= TFLAGS_PREEMPTED;
    this_thread()->preempt_count++;
    sched_switch(ctx);
}

thread_t *this_thread() {
    if (!this_cpu()) return NULL;
    return this_cpu()->current_thread;
}

proc_t *this_proc() {
    return this_cpu()->current_thread->parent;
}

void sched_exit(int code) {
    lapic_stop_timer();
    thread_t *thread = this_thread();
    thread->state = THREAD_ZOMBIE;
    thread->exit_code = code;
    // Wake up any threads waiting on this process
    proc_t *parent = this_proc()->parent;
    if (!parent) {
        sched_yield();
        return;
    }
    thread_t *child = parent->threads;
    do {
        child->sig_deliver |= 1 << 17;
        child->waiting_status = code | (thread->id << 32);
        child = child->next;
    } while (child != parent->threads);
    sched_yield();
}

void sched_yield() {
    lapic_stop_timer();
    this_thread()->flags &= ~TFLAGS_PREEMPTED;
    this_thread()->preempt_count = 0;
    lapic_ipi(this_cpu()->id, SCHED_VEC+1);
}

void sched_pause() {
    lapic_stop_timer();
}

void sched_resume() {
    if (this_thread()) {
        this_thread()->flags &= ~TFLAGS_PREEMPTED;
        this_thread()->preempt_count = 0;
    }
    lapic_ipi(this_cpu()->id, SCHED_VEC+1);
}

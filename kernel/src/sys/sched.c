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

uint64_t sched_tid = 0;
uint64_t sched_pid = 0;

proc_t *sched_proclist[256] = { 0 };

void *sighandle = NULL;

void sched_sighandle(int sig_no, void *handler);
void sched_sighandle_end();

int sched_sigjmp(thread_t *thread, int sig_no) {
    proc_t *parent = thread->parent;
    if (!parent->sig_handlers[sig_no].handler) {
        printf("Unhandled signal %d for tid %d, killing thread.\n", sig_no, thread->id);
        thread->state = THREAD_ZOMBIE;
        return 1;
    }
    thread->sig_fs = read_msr(FS_BASE);
    memcpy(&thread->sig_ctx, &thread->ctx, sizeof(context_t));
    thread->ctx.rsp = thread->sig_stack + PAGE_SIZE;
    *(uint64_t*)(thread->ctx.rsp - 8) = (uint64_t)parent->sig_handlers[sig_no].sa_restorer;
    thread->ctx.rsp -= 8;
    thread->ctx.cs = 0x23;
    thread->ctx.ss = 0x1b;
    thread->ctx.rip = (uint64_t)sighandle;
    thread->ctx.rdi = sig_no;
    thread->ctx.rsi = (uint64_t)parent->sig_handlers[sig_no].handler;
    return 0;
}

void sched_init() {
    idt_install_irq(16, sched_switch);
    idt_set_ist(SCHED_VEC, 1);
    uint64_t sighandle_size = (uint64_t)sched_sighandle_end - (uint64_t)sched_sighandle;
    sighandle = vmm_alloc(kernel_pagemap, DIV_ROUND_UP(sighandle_size, PAGE_SIZE), true);
    memcpy(sighandle, sched_sighandle, sighandle_size);
    // ^~ Wicked way of not copying other kernel functions accidentally
}

proc_t *sched_new_proc() {
    proc_t *proc = (proc_t*)kmalloc(sizeof(proc_t));
    proc->id = sched_pid++;
    proc->cwd = root_node;
    proc->children = NULL;
    proc->parent = NULL;
    proc->pagemap = vmm_new_pagemap();
    memset(proc->sig_handlers, 0, 32 * sizeof(sigaction_t));
    memset(proc->fd_table, 0, 256 * 8);
    proc->fd_table[0] = fd_open("/dev/kb", O_WRONLY);
    proc->fd_table[1] = fd_open("/dev/con", O_WRONLY);
    proc->fd_table[2] = fd_open("/dev/con", O_WRONLY);
    proc->fd_table[3] = fd_open("/dev/serial", O_WRONLY);
    proc->fd_count = 4;
    sched_proclist[proc->id] = proc;
    return proc;
}

void sched_add_thread(cpu_t *cpu, thread_t *thread) {
    spinlock_lock(&cpu->sched_lock);
    cpu->thread_count++;
    if (!cpu->thread_head) {
        thread->list_next = thread;
        thread->list_prev = thread;
        cpu->thread_head = thread;
        spinlock_free(&cpu->sched_lock);
        return;
    }

    thread->list_next = cpu->thread_head;
    thread->list_prev = cpu->thread_head->list_prev;
    cpu->thread_head->list_prev->list_next = thread;
    cpu->thread_head->list_prev = thread;
    spinlock_free(&cpu->sched_lock);
}

void sched_prepare_user_stack(thread_t *thread, int argc, char *argv[]) {
    // Copy the arguments into kernel memory
    char **kernel_argv = (char**)kmalloc(argc * 8);
    for (int i = 0; i < argc; i++) {
        int size = strlen(argv[i]) + 1;
        kernel_argv[i] = (char*)kmalloc(size);
        memcpy(kernel_argv[i], argv[i], size);
    }

    // Copy the arguments to the thread stack.
    uint64_t thread_argv[argc];
    uint64_t stack_top = thread->ctx.rsp;
    uint64_t offset = 0;
    pagemap_t *restore = vmm_switch_pagemap(thread->pagemap);
    for (int i = 0; i < argc; i++) {
        int size = strlen(kernel_argv[i]) + 1;
        offset += ALIGN_UP(size, 16); // Keep aligned to 16 bytes (ABI requirement)
        thread_argv[i] = stack_top - offset;
        memcpy((void*)(stack_top - offset), kernel_argv[i], size);
    }

    // Set up argv and argc
    offset += 8;
    *(uint64_t*)(stack_top - offset) = 0; // NULL env for now

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
}

thread_t *sched_new_thread(proc_t *parent, uint32_t cpu_num, vnode_t *node, int argc, char *argv[]) {
    thread_t *thread = (thread_t*)kmalloc(sizeof(thread_t));
    thread->id = sched_tid++;
    thread->cpu_num = cpu_num;
    thread->parent = parent;
    thread->pagemap = parent->pagemap;
    thread->flags = 0;

    if (!parent->children) {
        parent->children = thread;
        thread->next = thread;
        thread->prev = thread;
    } else {
        thread->next = parent->children;
        thread->prev = parent->children->prev;
        parent->children->prev->next = thread;
        parent->children->prev = thread;
    }

    thread->sig_deliver = 0;
    thread->sig_mask = 0;

    // Load ELF
    uint8_t *buffer = (uint8_t*)kmalloc(node->size);
    vfs_read(node, buffer, 0, node->size);
    thread->ctx.rip = elf_load(buffer, thread->pagemap);

    // Kernel stack (16 KB)
    uint64_t kernel_stack = (uint64_t)vmm_alloc(kernel_pagemap, 4, false);
    memset((void*)kernel_stack, 0, PAGE_SIZE);

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
    sched_prepare_user_stack(thread, argc, argv);
    thread->thread_stack = thread->ctx.rsp;

    thread->fs = 0;

    thread->state = THREAD_RUNNING;
    get_cpu(cpu_num)->has_runnable_thread = true;

    sched_add_thread(get_cpu(cpu_num), thread);

    return thread;
}

thread_t *sched_find_runnable_thread(cpu_t *cpu, bool sleep) {
    if (!cpu->current_thread)
        return cpu->thread_head;
    thread_t *thread = cpu->current_thread->list_next;
    while (thread->state != THREAD_RUNNING) {
        thread = thread->list_next;
        if (thread != cpu->current_thread)
            continue;
        cpu->has_runnable_thread = false;
        if (!sleep)
            return NULL;
        LOG_INFO("No runnable tasks left. Scheduler sleeping.\n");
        spinlock_free(&cpu->sched_lock);
        while (!cpu->has_runnable_thread)
            __asm__ volatile ("pause");
        LOG_INFO("Runnable task found! Waking up scheduler.\n");
        spinlock_lock(&cpu->sched_lock);
    }
    return thread;
}

void sched_switch(context_t *ctx) {
    cpu_t *cpu = this_cpu();
    while (!cpu->thread_head)
        __asm__ volatile ("pause");
    spinlock_lock(&cpu->sched_lock);
    if (cpu->current_thread) {
        thread_t *thread = cpu->current_thread;
        thread->fs = read_msr(FS_BASE);
        thread->ctx = *ctx;
        // Check for signals
        for (int i = 0; i < 63; i++) {
            if (thread->sig_deliver & (1 << i) && !(thread->sig_mask & (1 << i))) {
                sched_sigjmp(thread, i);
                thread->sig_deliver &= ~(1 << i);
            }
        }
    }
    thread_t *next_thread = sched_find_runnable_thread(cpu, true);
    cpu->current_thread = next_thread;
    *ctx = next_thread->ctx;
    vmm_switch_pagemap(next_thread->pagemap);
    write_msr(FS_BASE, next_thread->fs);
    write_msr(KERNEL_GS_BASE, (uint64_t)next_thread);
    spinlock_free(&cpu->sched_lock);
    lapic_oneshot(SCHED_VEC, SCHED_QUANTUM);
    lapic_eoi();
}

thread_t *this_thread() {
    return this_cpu()->current_thread;
}

proc_t *this_proc() {
    return this_cpu()->current_thread->parent;
}

void sched_exit(int code) {
    thread_t *thread = this_thread();
    LOG_INFO("Thread %d exited with code %d.\n", thread->id, code);
    thread->state = THREAD_ZOMBIE;
    sched_yield();
}

void sched_yield() {
    lapic_ipi(this_cpu()->id, SCHED_VEC);
}

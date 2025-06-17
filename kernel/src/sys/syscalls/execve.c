#include <sched.h>
#include <vfs.h>
#include <errno.h>
#include <heap.h>
#include <elf.h>
#include <pmm.h>
#include <apic.h>
#include <stdio.h>
#include <string.h>

void execve_cleanup(int argc, int envc, char **argv, char **envp) {
    for (int i = 0; i < argc; i++)
        kfree(argv[i]);
    kfree(argv);
    for (int j = 0; j < envc; j++)
        kfree(envp[j]);
    kfree(envp);
}

uint64_t sys_execve(const char *u_pathname, const char **u_argv, const char **u_envp) {
    // Copy pathname to kernel
    char *pathname = (char*)kmalloc(strlen(u_pathname)+1);
    memcpy(pathname, u_pathname, strlen(u_pathname)+1);
    // Copy argv, envp to kernel
    int argc = 0;
    if (u_argv) {
        while (u_argv[argc++]);
        argc -= 1;
    }
    int envc = 0;
    if (u_envp) {
        while (u_envp[envc++]);
        envc -= 1;
    }
    char **argv = (char**)kmalloc(argc * 8);
    for (int i = 0; i < argc; i++) {
        int size = strlen(u_argv[i]) + 1;
        char *arg = (char*)kmalloc(size);
        argv[i] = arg;
        memcpy(arg, u_argv[i], size);
    }
    char **envp = (char**)kmalloc(envc * 8);
    for (int i = 0; i < envc; i++) {
        int size = strlen(u_envp[i]) + 1;
        char *env = (char*)kmalloc(size);
        envp[i] = env;
        memcpy(env, u_envp[i], size);
    }
    proc_t *proc = this_proc();
    thread_t *thread = this_thread();
    vnode_t *node = vfs_open(proc->cwd, pathname);
    if (!node) {
        execve_cleanup(argc, envc, argv, envp);
        return -ENOENT;
    }
    lapic_stop_timer();
    vmm_switch_pagemap(kernel_pagemap);
    // Destroy old page map
    vmm_destroy_pagemap(thread->pagemap);
    pagemap_t *new_pagemap = vmm_new_pagemap();
    thread->pagemap = proc->pagemap = new_pagemap;
    thread->sig_deliver = 0;
    thread->sig_mask = 0;
    memset(thread->fx_area, 0, 512);
    *(uint16_t *)(thread->fx_area + 0x00) = 0x037F;
    *(uint32_t *)(thread->fx_area + 0x18) = 0x1F80;

    // Load ELF
    uint8_t *buffer = (uint8_t*)kmalloc(node->size);
    vfs_read(node, buffer, 0, node->size);
    thread->ctx.rip = elf_load(buffer, thread->pagemap);

    thread->ctx.cs = 0x23;
    thread->ctx.ss = 0x1b;
    thread->ctx.rflags = 0x202;

    // Thread stack (32 KB)
    uint64_t thread_stack = (uint64_t)vmm_alloc(thread->pagemap, 8, true);
    uint64_t thread_stack_top = thread_stack + 8 * PAGE_SIZE;
    thread->stack = thread_stack;

    // Sig stack (4 KB)
    uint64_t sig_stack = (uint64_t)vmm_alloc(thread->pagemap, 1, true);
    thread->sig_stack = sig_stack;

    // Set up stack (argc, argv, env)
    thread->ctx.rsp = thread_stack_top;
    sched_prepare_user_stack(thread, argc, argv, envp);
    thread->thread_stack = thread->ctx.rsp;

    execve_cleanup(argc, envc, argv, envp);

    thread->fs = 0;
    vmm_switch_pagemap(new_pagemap);
    __asm__ volatile ("swapgs");
    this_cpu()->current_thread = NULL;
    sched_yield();
    return 0;
}

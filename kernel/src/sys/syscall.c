#include "vmm.h"
#include <syscall.h>
#include <devices.h>
#include <vfs.h>
#include <idt.h>
#include <stdio.h>
#include <log.h>
#include <sched.h>
#include <ipc.h>
#include <spinlock.h>

// Argument order: rdi, rsi, rdx, rcx, r8 and r9

void syscall_get_device_handle(context_t *ctx) {
    char *name = (char*)ctx->rdi;
    dev_t *dev = dev_get(name);
    if (!dev) return;
    task_t *task = this_task();
    task->handles[task->handle_count++] = dev->node;
    ctx->rax = task->handle_count - 1;
}

void syscall_open_file_handle(context_t *ctx) {
    char *path = (char*)ctx->rdi;
    vnode_t *node = vfs_open(path);
    if (!node) return;
    task_t *task = this_task();
    task->handles[task->handle_count++] = node;
    ctx->rax = task->handle_count - 1;
}

void syscall_read_handle(context_t *ctx) {
    uint64_t handle = ctx->rdi;
    uint8_t *buffer = (uint8_t*)ctx->rsi;
    size_t len = ctx->rdx;
    task_t *task = this_task();
    if (task->handle_count <= handle || !task->handles[handle]) return;
    ctx->rax = vfs_read(task->handles[handle], buffer, len);
}

void syscall_write_handle(context_t *ctx) {
    uint64_t handle = ctx->rdi;
    uint8_t *buffer = (uint8_t*)ctx->rsi;
    size_t len = ctx->rdx;
    task_t *task = this_task();
    if (task->handle_count <= handle || !task->handles[handle]) return;
    ctx->rax = vfs_write(task->handles[handle], buffer, len);
}

void syscall_close_handle(context_t *ctx) {
    uint64_t handle = ctx->rdi;
    task_t *task = this_task();
    if (task->handle_count <= handle || !task->handles[handle]) return;
    vfs_close(task->handles[handle]);
    task->handles[handle] = NULL;
}

void syscall_ipc_create_ep(context_t *ctx) {
    char *name = (char*)ctx->rdi;
    ctx->rax = ipc_create_ep(name);
}

void syscall_ipc_find_ep(context_t *ctx) {
    char *name = (char*)ctx->rdi;
    ctx->rax = ipc_find_ep(name);
}

void syscall_ipc_send(context_t *ctx) {
    int16_t ipc = (int16_t)ctx->rdi;
    uint8_t *buffer = (uint8_t*)ctx->rsi;
    size_t len = ctx->rdx;
    ipc_send(buffer, len, ipc);
}

void syscall_ipc_recv(context_t *ctx) {
    int16_t ipc = (int16_t)ctx->rdi;
    uint8_t *buffer = (uint8_t*)ctx->rsi;
    size_t len = ctx->rdx;
    ipc_recv(buffer, len, ipc);
}

void syscall_ipc_close(context_t *ctx) {
}

void syscall_map_device_handle(context_t *ctx) {
    uint64_t handle = ctx->rdi;
    task_t *task = this_task();
    if (task->handle_count <= handle || !task->handles[handle]) return;
    dev_t *dev = (dev_t*)task->handles[handle]->mnt_info;
    if (!dev->map_info.start_addr) return;
    uint64_t phys_addr = vmm_get_phys(kernel_pagemap, dev->map_info.start_addr);
    uint64_t virt_addr = (uint64_t)vmm_alloc_to(task->pagemap, phys_addr, dev->map_info.page_count, true);
    ctx->rax = virt_addr;
}

void syscall_spawn_process(context_t *ctx) {
    uint64_t handle = ctx->rdi;
    task_t *task = this_task();
    if (task->handle_count <= handle || !task->handles[handle]) return;
    if (task->handles[handle]->type != FS_FILE) return;
    int argc = ctx->rsi;
    char **argv = (char**)ctx->rdx;
    // TODO: Pick a random CPU to spawn the elf.
    ctx->rax = sched_load_elf(1, task->handles[handle], argc, argv)->id;
}

void *syscall_handler_table[] = {
    syscall_get_device_handle,
    syscall_open_file_handle,
    syscall_read_handle,
    syscall_write_handle,
    syscall_close_handle,
    syscall_ipc_create_ep,
    syscall_ipc_find_ep,
    syscall_ipc_send,
    syscall_ipc_recv,
    syscall_ipc_close,
    syscall_map_device_handle,
    syscall_spawn_process
};

void syscall_handler(context_t *ctx) {
    if (ctx->rax > sizeof(syscall_handler_table) / sizeof(uint64_t)) {
        printf("Unhandled syscall.\n");
        return;
    }
    void(*handler)(context_t*) = syscall_handler_table[ctx->rax];
    handler(ctx);
}

void syscall_init() {
    idt_install_irq(0x60, syscall_handler);
}

#include <syscall.h>
#include <devices.h>
#include <vfs.h>
#include <idt.h>
#include <stdio.h>
#include <log.h>
#include <sched.h>

// Argument order: rdi, rsi, rdx, rcx, r8 and r9

#define SYSCALL_COUNT 2

void syscall_get_device_handle(context_t *ctx) {
    char *name = (char*)ctx->rdi;
    dev_t *dev = dev_get(name);
    if (!dev) return;
    task_t *task = this_task();
    task->handles[task->handle_count++] = dev->node;
    ctx->rax = task->handle_count - 1;
}

void syscall_write(context_t *ctx) {
    uint64_t handle = ctx->rdi;
    uint8_t *buffer = (uint8_t*)ctx->rsi;
    size_t len = ctx->rdx;
    task_t *task = this_task();
    if (task->handle_count <= handle) return;
    ctx->rax = vfs_write(task->handles[handle], buffer, len);
}

void *syscall_handler_table[SYSCALL_COUNT] = {
    syscall_get_device_handle,
    syscall_write
};

void syscall_handler(context_t *ctx) {
    LOG_INFO("Handling syscall %d\n", ctx->rax);
    if (ctx->rax > SYSCALL_COUNT) {
        printf("Unhandled syscall.\n");
        return;
    }
    void(*handler)(context_t*) = syscall_handler_table[ctx->rax];
    handler(ctx);
}

void syscall_init() {
    idt_install_irq(0x60, syscall_handler);
}

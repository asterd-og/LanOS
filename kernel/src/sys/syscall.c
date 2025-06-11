#include <errno.h>
#include <syscall.h>
#include <vfs.h>
#include <idt.h>
#include <stdio.h>
#include <log.h>
#include <sched.h>
#include <spinlock.h>
#include <smp.h>
#include <fd.h>
#include <cpu.h>
#include <vmm.h>
#include <pmm.h>
#include <string.h>
#include <heap.h>

// Argument order: rdi, rsi, rdx, rcx, r10, r8 and r9

uint64_t sys_read(context_t *ctx);
uint64_t sys_write(context_t *ctx);
uint64_t sys_open(context_t *ctx);
uint64_t sys_stat(context_t *ctx);
uint64_t sys_fstat(context_t *ctx);
uint64_t sys_lseek(context_t *ctx);
uint64_t sys_mmap(context_t *ctx);
uint64_t sys_getpid(context_t *ctx);
uint64_t sys_exit(context_t *ctx);
uint64_t sys_getcwd(context_t *ctx);
uint64_t sys_chdir(context_t *ctx);
uint64_t sys_arch_prctl(context_t *ctx);
uint64_t sys_gettid(context_t *ctx);

void *syscall_handler_table[] = {
    [0] = sys_read,
    [1] = sys_write,
    [2] = sys_open,
    [4] = sys_stat,
    [5] = sys_fstat,
    [8] = sys_lseek,
    [9] = sys_mmap,
    [39] = sys_getpid,
    [60] = sys_exit,
    [79] = sys_getcwd,
    [80] = sys_chdir,
    [158] = sys_arch_prctl,
    [186] = sys_gettid
};

void syscall_handler(context_t *ctx) {
    if (ctx->rax > sizeof(syscall_handler_table) / sizeof(uint64_t) || !syscall_handler_table[ctx->rax]) {
        printf("Unhandled syscall %d.\n", ctx->rax);
        return;
    }
    uint64_t(*handler)(context_t*) = syscall_handler_table[ctx->rax];
    ctx->rax = handler(ctx);
}

void syscall_init() {
    idt_install_irq(0x60, syscall_handler);
}

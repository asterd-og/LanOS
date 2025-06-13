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

// Argument order: rdi, rsi, rdx, r10, r8 and r9

uint64_t sys_read(context_t *ctx);
uint64_t sys_write(context_t *ctx);
uint64_t sys_open(context_t *ctx);
uint64_t sys_stat(context_t *ctx);
uint64_t sys_fstat(context_t *ctx);
uint64_t sys_lseek(context_t *ctx);
uint64_t sys_mmap(context_t *ctx);
uint64_t sys_sigaction(context_t *ctx);
uint64_t sys_getpid(context_t *ctx);
uint64_t sys_exit(context_t *ctx);
uint64_t sys_kill(context_t *ctx);
uint64_t sys_getcwd(context_t *ctx);
uint64_t sys_chdir(context_t *ctx);
uint64_t sys_arch_prctl(context_t *ctx);
uint64_t sys_gettid(context_t *ctx);
uint64_t sys_restoresig(context_t *ctx);

void *syscall_handler_table[] = {
    [0] = sys_read,
    [1] = sys_write,
    [2] = sys_open,
    [4] = sys_stat,
    [5] = sys_fstat,
    [8] = sys_lseek,
    [9] = sys_mmap,
    [13] = sys_sigaction,
    [39] = sys_getpid,
    [60] = sys_exit,
    [62] = sys_kill,
    [79] = sys_getcwd,
    [80] = sys_chdir,
    [158] = sys_arch_prctl,
    [186] = sys_gettid,
    [256] = sys_restoresig
};

uint64_t syscall_handler(syscall_frame_t *frame) {
    if (frame->rax > sizeof(syscall_handler_table) / sizeof(uint64_t) || !syscall_handler_table[frame->rax]) {
        printf("Unhandled syscall %d.\n", frame->rax);
        return -ENOSYS;
    }
    uint64_t(*handler)(uint64_t, uint64_t, uint64_t, uint64_t, uint64_t, uint64_t) =
        syscall_handler_table[frame->rax];
    return handler(frame->rdi, frame->rsi, frame->rdx, frame->r10, frame->r8, frame->r9);
}

void syscall_entry();

void syscall_init() {
    uint64_t efer = read_msr(IA32_EFER);
    efer |= (1 << 0);
    write_msr(IA32_EFER, efer);
    uint64_t star = 0;
    star |= ((uint64_t)0x08 << 32);
    star |= ((uint64_t)0x13 << 48);
    write_msr(IA32_STAR, star);
    write_msr(IA32_LSTAR, (uint64_t)syscall_entry);
}

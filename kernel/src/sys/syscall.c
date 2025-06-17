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
#include <heap.h>

// Argument order: rdi, rsi, rdx, r10, r8 and r9

uint64_t sys_read();
uint64_t sys_write();
uint64_t sys_open();
uint64_t sys_stat();
uint64_t sys_fstat();
uint64_t sys_lseek();
uint64_t sys_mmap();
uint64_t sys_sigaction();
void sys_sigreturn(syscall_frame_t *frame);
uint64_t sys_ioctl();
uint64_t sys_getpid();
uint64_t sys_fork(syscall_frame_t *frame);
uint64_t sys_execve();
uint64_t sys_exit();
uint64_t sys_waitpid();
uint64_t sys_kill();
uint64_t sys_getcwd();
uint64_t sys_chdir();
uint64_t sys_arch_prctl();
uint64_t sys_gettid();

void *syscall_handler_table[] = {
    [0] = sys_read,
    [1] = sys_write,
    [2] = sys_open,
    [4] = sys_stat,
    [5] = sys_fstat,
    [8] = sys_lseek,
    [9] = sys_mmap,
    [13] = sys_sigaction,
    [15] = sys_sigreturn,
    [16] = sys_ioctl,
    [39] = sys_getpid,
    [57] = sys_fork,
    [59] = sys_execve,
    [60] = sys_exit,
    [61] = sys_waitpid,
    [62] = sys_kill,
    [79] = sys_getcwd,
    [80] = sys_chdir,
    [158] = sys_arch_prctl,
    [186] = sys_gettid
};

void syscall_handler(syscall_frame_t *frame) {
    if (frame->rax > sizeof(syscall_handler_table) / sizeof(uint64_t) || !syscall_handler_table[frame->rax]) {
        printf("Unhandled syscall %d.\n", frame->rax);
        frame->rax = -ENOSYS;
        return;
    }
    switch (frame->rax) {
        case 15: // sigreturn
            sys_sigreturn(frame);
            break;
        case 57: // fork
            frame->rax = sys_fork(frame);
            break;
        default:
            this_thread()->syscall_frame = *frame;
            uint64_t(*handler)(uint64_t, uint64_t, uint64_t, uint64_t, uint64_t, uint64_t) =
                syscall_handler_table[frame->rax];
            frame->rax = handler(frame->rdi, frame->rsi, frame->rdx, frame->r10, frame->r8, frame->r9);
            break;
    }
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

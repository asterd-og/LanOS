#include <sched.h>
#include <errno.h>
#include <signal.h>
#include <string.h>
#include <stdio.h>
#include <smp.h>
#include <spinlock.h>
#include <cpu.h>
#include <syscall.h>
#include <pmm.h>

uint64_t sys_sigaction(uint32_t signal, sigaction_t *act, sigaction_t *old_act) {
    return -ENOSYS;
}

uint64_t sys_kill(uint64_t pid, uint32_t signal) {
    return -ENOSYS;
}

void sys_sigreturn(syscall_frame_t *frame) {
    thread_t *thread = this_thread();
    thread->fs = thread->sig_fs;
    if (!(thread->sig_ctx.cs & 3)) {
        // Return the syscall with -EINTR (TODO make blocking syscalls uninterruptable.)
        memcpy(frame, &thread->syscall_frame, sizeof(syscall_frame_t));
        frame->rax = -EINTR;
        return;
    }
    memcpy(frame, &thread->sig_ctx, sizeof(context_t));
    frame->rcx = thread->sig_ctx.rip;
    frame->r11 = thread->sig_ctx.rflags;
    return;
}

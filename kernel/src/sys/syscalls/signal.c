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
    if (signal > 63 || signal == 0)
        return -EINVAL;
    proc_t *proc = this_proc();
    sigaction_t old = proc->sig_handlers[signal];
    if (old_act && old.handler)
        memcpy(old_act, &old, sizeof(sigaction_t));
    if (act)
        memcpy(&proc->sig_handlers[signal], act, sizeof(sigaction_t));
    return 0;
}

uint64_t sys_kill(uint64_t pid, uint32_t signal) {
    if (signal > 63 || signal == 0)
        return -EINVAL;
    if (pid >= sched_pid || !sched_proclist[pid])
        return -ESRCH;
    proc_t *proc = (proc_t*)sched_proclist[pid];
    thread_t *thread = proc->children;
    while (true) {
        thread->sig_deliver |= (1U << signal); // Scheduler checks if this signal isnt masked, if its not, run it!
        thread = thread->next;
        if (thread == proc->children)
            break;
    }
    return 0;
}

void sys_sigreturn(syscall_frame_t *frame) {
    thread_t *thread = this_thread();
    thread->fs = thread->sig_fs;
    if (!(thread->sig_ctx.cs & 3)) {
        // Return the syscall with -EINTR
        memcpy(frame, &thread->syscall_frame, sizeof(syscall_frame_t));
        frame->rax = -EINTR;
        return;
    }
    memcpy(frame, &thread->sig_ctx, sizeof(context_t));
    frame->rcx = thread->sig_ctx.rip;
    frame->r11 = thread->sig_ctx.rflags;
    return;
}

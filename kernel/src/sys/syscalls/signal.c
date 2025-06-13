#include <sched.h>
#include <errno.h>
#include <signal.h>
#include <string.h>
#include <stdio.h>
#include <smp.h>
#include <spinlock.h>

uint64_t sys_sigaction(uint32_t signal, sigaction_t *act, sigaction_t *old_act) {
    if (signal > 31)
        return -EINVAL;
    proc_t *proc = this_proc();
    sigaction_t old = proc->sig_handlers[signal];
    if (old_act && old.handler)
        memcpy(old_act, &old, sizeof(sigaction_t));
    if (act)
        memcpy(&proc->sig_handlers[signal], act, sizeof(sigaction_t));
    return -ENOSYS;
}

uint64_t sys_kill(uint64_t pid, uint32_t signal) {
    if (signal > 31)
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

uint64_t sys_restoresig(context_t *ctx) {
    thread_t *thread = this_thread();
    memcpy(&thread->ctx, &thread->sig_ctx, sizeof(context_t));
    printf("Restored ctx! %p\n", thread->ctx.rip);
    return 0;
}

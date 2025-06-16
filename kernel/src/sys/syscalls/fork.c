#include <sched.h>
#include <stdio.h>
#include <apic.h>

uint64_t sys_fork(syscall_frame_t *frame) {
    lapic_stop_timer();
    proc_t *proc = sched_fork_proc();
    thread_t *thread = sched_fork_thread(proc, this_thread(), frame);
    lapic_oneshot(SCHED_VEC, 5);
    return proc->id;
}

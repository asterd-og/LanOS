#include <sched.h>
#include <apic.h>

uint64_t sys_waitpid(int pid, int *status, int flags) {
    lapic_stop_timer();
    thread_t *thread = this_thread();
    // Check if the thread has already been terminated
    while (!(this_thread()->sig_deliver & (1 << 17)))
        sched_yield();
    *status = thread->waiting_status & 0xffffffff;
    int ret_pid = thread->waiting_status >> 32;
    thread->waiting_status = 0;
    return ret_pid;
}

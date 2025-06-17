#include <sched.h>
#include <errno.h>
#include <stdio.h>

int count = 0;

uint64_t sys_waitpid(int pid, int *status, int flags) {
    thread_t *thread = this_thread();
    thread->waiting_status = pid;
    thread->flags |= TFLAGS_WAITING4;
    thread->state = THREAD_BLOCKED;
    sched_yield();
    *status = thread->waiting_status & 0xffffffff;
    int ret_pid = thread->waiting_status >> 32;
    thread->waiting_status = 0;
    return ret_pid;
}

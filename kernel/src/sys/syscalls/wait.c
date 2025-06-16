#include <sched.h>
#include <errno.h>
#include <stdio.h>

uint64_t sys_waitpid(int pid, int *status, int flags) {
    if (pid >= sched_pid || !sched_proclist[pid])
        return -ESRCH;
    this_thread()->flags |= TFLAGS_WAITING4;
    this_thread()->state = THREAD_BLOCKED;
    sched_yield();
    *status = this_thread()->waiting_status;
    return 0;
}

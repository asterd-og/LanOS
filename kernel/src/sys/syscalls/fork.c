#include <sched.h>

uint64_t sys_fork(syscall_frame_t *frame) {
    sched_pause();
    proc_t *proc = sched_fork_proc();
    thread_t *thread = sched_fork_thread(proc, this_thread(), frame);
    sched_resume();
    return proc->id;
}

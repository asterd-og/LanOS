#include <sched.h>

uint64_t sys_gettid() {
    return this_thread()->id;
}

uint64_t sys_getpid() {
    return this_proc()->id;
}

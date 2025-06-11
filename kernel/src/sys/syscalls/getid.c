#include <sched.h>

uint64_t sys_gettid(context_t *ctx) {
    return this_thread()->id;
}

uint64_t sys_getpid(context_t *ctx) {
    return this_proc()->id;
}

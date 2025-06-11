#include <sched.h>

uint64_t sys_exit(context_t *ctx) {
    sched_exit((int)ctx->rdi);
    return 0;
}

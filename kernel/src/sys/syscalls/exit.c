#include <sched.h>

uint64_t sys_exit(int code) {
    sched_exit(code);
    return 0;
}

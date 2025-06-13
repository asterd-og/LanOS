#include <sched.h>
#include <cpu.h>
#include <errno.h>
#include <stdio.h>

uint64_t sys_arch_prctl(int op, long extra) {
    switch (op) {
        case 0x1002:
            write_msr(FS_BASE, extra);
            return 0;
            break;
        default:
            printf("[sys_arch_prctl]: %d not implemented.\n", op);
            return -EINVAL;
            break;
    }
}

#include <sched.h>
#include <errno.h>
#include <fd.h>
#include <stdio.h>

uint64_t sys_ioctl(int fd_idx, uint64_t request, void *arg) {
    fd_t *fd = this_proc()->fd_table[fd_idx];
    if (!fd) {
        return -EBADF;
    }
    int ret = vfs_ioctl(fd->node, request, arg);
    return ret;
}

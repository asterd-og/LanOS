#include <errno.h>
#include <stdint.h>
#include <idt.h>
#include <sched.h>

uint64_t sys_write(uint32_t fd_idx, void *buf, size_t count) {
    fd_t *fd = this_proc()->fd_table[fd_idx];
    if (!fd)
        return -EBADF;
    size_t written = vfs_write(fd->node, buf, fd->off, count);
    fd->off += written;
    return written;
}

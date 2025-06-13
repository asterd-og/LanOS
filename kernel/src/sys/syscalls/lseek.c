#include <stdint.h>
#include <stddef.h>
#include <sched.h>
#include <errno.h>

typedef ptrdiff_t ssize_t;

uint64_t sys_lseek(uint32_t fd_idx, long offset, int whence) {
    fd_t *fd = this_proc()->fd_table[fd_idx];
    if (!fd)
        return -EBADF;

    switch (whence) {
        case 0: // SEEK_SET
            if (offset < 0)
                return -EINVAL;
            fd->off = offset;
            break;
        case 1: // SEEK_CUR
            if (fd->off + offset < 0)
                return -EINVAL;
            fd->off += offset;
            break;
        case 2: // SEEK_END
            fd->off = fd->node->size;
            break;
        default:
            return -EINVAL;
            break;
    }
    return fd->off;
}

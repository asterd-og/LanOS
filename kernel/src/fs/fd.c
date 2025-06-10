#include <fd.h>
#include <sched.h>
#include <heap.h>

fd_t *fd_open(const char *path, int flags) {
    fd_t *fd = (fd_t*)kmalloc(sizeof(fd_t));
    fd->node = vfs_open(this_proc()->cwd, path);
    fd->flags = flags;
    fd->off = 0;
    return fd;
}

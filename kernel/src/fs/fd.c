#include <fd.h>
#include <sched.h>
#include <heap.h>
#include <evdev.h>

fd_t *fd_open(const char *path, int flags) {
    fd_t *fd = (fd_t*)kmalloc(sizeof(fd_t));
    fd->node = vfs_open(this_proc()->cwd, path);
    if (!fd->node) {
        kfree(fd);
        return NULL;
    }
    fd->flags = flags;
    size_t off = 0;
    if (fd->node->major == 13) {
        // Evdev, set the offset to the last write
        evdev_t *dev = (evdev_t*)fd->node->mnt_info;
        off = dev->ringbuf->write_idx;
    }
    fd->off = off;
    return fd;
}

#include <errno.h>
#include <stdint.h>
#include <idt.h>
#include <sched.h>

uint64_t sys_write(context_t *ctx) {
    uint32_t fd_idx = (uint32_t)ctx->rdi;
    void *buf = (void*)ctx->rsi;
    size_t count = (size_t)ctx->rdx;
    fd_t *fd = this_proc()->fd_table[fd_idx];
    if (!fd)
        return -EBADF;
    size_t written = vfs_write(fd->node, buf, fd->off, count);
    fd->off += written;
    return written;
}

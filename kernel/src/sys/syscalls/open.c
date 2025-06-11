#include <errno.h>
#include <idt.h>
#include <sched.h>

uint64_t sys_open(context_t *ctx) {
    const char *path = (const char*)ctx->rdi;
    int flags = (int)ctx->rsi;
    unsigned int mode = (unsigned int)ctx->rdx;
    proc_t *proc = this_proc();
    fd_t *fd = fd_open(path, flags);
    if (!fd)
        return -ENOENT;
    proc->fd_table[proc->fd_count++] = fd_open(path, flags);
    return proc->fd_count - 1;
}

#include <errno.h>
#include <idt.h>
#include <sched.h>

uint64_t sys_open(const char *path, int flags, unsigned int mode) {
    proc_t *proc = this_proc();
    fd_t *fd = fd_open(path, flags);
    if (!fd)
        return -ENOENT;
    proc->fd_table[proc->fd_count++] = fd_open(path, flags);
    return proc->fd_count - 1;
}

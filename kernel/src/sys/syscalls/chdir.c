#include <sched.h>
#include <errno.h>
#include <vfs.h>

uint64_t sys_chdir(context_t *ctx) {
    const char *path = (const char*)ctx->rdi;
    vnode_t *node = vfs_open(this_proc()->cwd, path);
    if (!node)
        return -ENOENT;
    this_proc()->cwd = node;
    return 0;
}

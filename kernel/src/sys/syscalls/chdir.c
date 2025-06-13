#include <sched.h>
#include <errno.h>
#include <vfs.h>

uint64_t sys_chdir(const char *path) {
    vnode_t *node = vfs_open(this_proc()->cwd, path);
    if (!node)
        return -ENOENT;
    this_proc()->cwd = node;
    return 0;
}

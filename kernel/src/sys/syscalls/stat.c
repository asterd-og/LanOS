#include <errno.h>
#include <idt.h>
#include <sched.h>
#include <stdint.h>

typedef struct {
    uint64_t st_dev;
    uint64_t st_ino;
    uint32_t st_mode;
    uint32_t st_nlink;
    uint32_t st_uid;
    uint32_t st_gid;
    uint64_t st_rdev;
    int64_t  st_size;
    int64_t  st_blksize;
    int64_t  st_blocks;

    int64_t st_atime;
    int64_t st_mtime;
    int64_t st_ctime;
} stat_t;

#define S_IFMT  0170000
#define S_IFDIR 0040000
#define S_IFREG 0100000
#define S_IFLNK 0120000

#define S_IRUSR 00400
#define S_IWUSR 00200
#define S_IXUSR 00100

uint64_t sys_stat(const char *pathname, stat_t *statbuf) {
    vnode_t *node = vfs_open(this_proc()->cwd, pathname);
    if (!node)
        return -ENOENT;

    statbuf->st_mode = (node->type == FS_FILE ? S_IFREG : S_IFDIR) | S_IRUSR | S_IWUSR;
    statbuf->st_nlink = 0;
    statbuf->st_uid = 0;
    statbuf->st_gid = 0;
    statbuf->st_size = node->size;
    statbuf->st_ino = node->inode;
    return 0;
}

uint64_t sys_fstat(uint32_t fd_idx, stat_t *statbuf) {
    if (fd_idx > this_proc()->fd_count)
        return -EBADF;

    fd_t *fd = this_proc()->fd_table[fd_idx];
    if (!fd)
        return -EBADF;

    vnode_t *node = fd->node;
    if (!node)
        return -ENOENT;

    statbuf->st_mode = (node->type == FS_FILE ? S_IFREG : S_IFDIR) | S_IRUSR | S_IWUSR;
    statbuf->st_nlink = 0;
    statbuf->st_uid = 0;
    statbuf->st_gid = 0;
    statbuf->st_size = node->size;
    return 0;
}

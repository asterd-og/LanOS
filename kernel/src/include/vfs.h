#pragma once

#include <stdint.h>
#include <stddef.h>
#include <mutex.h>
#include <ahci.h>

#define FS_FILE 0x1
#define FS_DIR  0x2

#define POLLIN   0x001
#define POLLPRI  0x002
#define POLLOUT  0x004
#define POLLERR  0x008
#define POLLHUP  0x010
#define POLLNVAL 0x020

typedef struct vnode_t {
    uint8_t type;
    uint32_t size;
    uint32_t inode;
    char name[256];
    int refcount;
    mutex_t *mutex;
    int major;
    int minor;
    struct vnode_t *parent;
    struct vnode_t *child;
    struct vnode_t *next;
    size_t (*read)(struct vnode_t *node, uint8_t *buffer, size_t off, size_t len);
    size_t (*write)(struct vnode_t *node, uint8_t *buffer, size_t off, size_t len);
    struct vnode_t *(*lookup)(struct vnode_t *node, const char *name);
    int (*ioctl)(struct vnode_t *node, uint64_t request, void *arg);
    int (*poll)(struct vnode_t *node, int events);
    void (*populate)(struct vnode_t *node);
    void *mnt_info;
} vnode_t;

extern vnode_t *root_node;

void vfs_init();
void vfs_add_node(vnode_t *parent, vnode_t *child);

vnode_t *vfs_open(vnode_t *root, const char *path);
void vfs_close(vnode_t *node);
size_t vfs_read(vnode_t *node, uint8_t *buffer, size_t off, size_t len);
size_t vfs_write(vnode_t *node, uint8_t *buffer, size_t off, size_t len);
vnode_t *vfs_lookup(vnode_t *node, const char *name);
int vfs_ioctl(vnode_t *node, uint64_t request, void *arg);
int vfs_poll(vnode_t *node, int events);
void vfs_populate(vnode_t *node);

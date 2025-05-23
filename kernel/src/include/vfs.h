#pragma once

#include <stdint.h>
#include <stddef.h>
#include <ahci.h>

#define FS_FILE 0x1
#define FS_DIR  0x2
#define FS_DEV  0x4
#define FS_IPC  0x8

typedef struct vnode_t {
    int type;
    char name[256];
    uint32_t size;
    size_t (*read)(struct vnode_t *node, uint8_t *buffer, size_t len);
    size_t (*write)(struct vnode_t *node, uint8_t *buffer, size_t len);
    char *(*read_dir)(struct vnode_t *node, uint32_t index);
    struct vnode_t *(*find_dir)(struct vnode_t *node, char *name);
    void *data;
    void *mnt_info;
} vnode_t;

char vfs_mount_disk(ahci_port_t *port);
vnode_t *vfs_get_mount(char letter);

vnode_t *vfs_open(char *path);
void vfs_close(vnode_t *node);
size_t vfs_read(vnode_t *node, uint8_t *buffer, size_t len);
size_t vfs_write(vnode_t *node, uint8_t *buffer, size_t len);
char *vfs_read_dir(vnode_t *node, uint32_t index);
vnode_t *vfs_find_dir(vnode_t *node, char *name);

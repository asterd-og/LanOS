#include <devfs.h>
#include <string.h>
#include <stdio.h>
#include <heap.h>
#include <evdev.h>

vnode_t *dev_node = NULL;

vnode_t *devfs_register_dev(char *name, void *read, void *write, void *ioctl) {
    vnode_t *node = (vnode_t*)kmalloc(sizeof(vnode_t));
    memset(node, 0, sizeof(vnode_t));
    node->type = 0; // TODO
    node->size = 4096;
    node->inode = 0;
    node->read = read;
    node->write = write;
    node->ioctl = ioctl;
    node->mutex = mutex_create();
    memcpy(node->name, name, strlen(name) + 1);
    vfs_add_node(dev_node, node);
    return node;
}

vnode_t *devfs_lookup(vnode_t *node, const char *name) {
    vnode_t *current = node->child;
    while (current) {
        if (!strcmp(current->name, name))
            break;
        current = current->next;
        if (!current)
            return NULL;
    }
    return current;
}

void devfs_init() {
    dev_node = (vnode_t*)kmalloc(sizeof(vnode_t));
    memset(dev_node, 0, sizeof(vnode_t));
    dev_node->type = FS_DIR;
    dev_node->size = 4096;
    dev_node->inode = 0;
    memcpy(dev_node->name, "dev", 4);
    dev_node->lookup = devfs_lookup;
    vfs_add_node(root_node, dev_node);
    evdev_init();
    devfb_init();
}

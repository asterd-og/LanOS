#include <evdev.h>
#include <heap.h>
#include <string.h>
#include <serial.h>

vnode_t *input_node = NULL;

size_t evdev_read(vnode_t *node, uint8_t *buffer, size_t off, size_t len) {
    evdev_t *dev = (evdev_t*)node->mnt_info;
    size_t current_idx = off / sizeof(input_event_t);
    size_t read = 0;
    for (size_t i = 0; i < len / sizeof(input_event_t); i++) {
        // Ringbuffer get already sleeps the thread if it's waiting for more events.
        ringbuffer_get(dev->ringbuf, buffer + sizeof(input_event_t) * i, &current_idx);
        read++;
    }
    return sizeof(input_event_t) * read;
}

evdev_t *evdev_create(char *name) {
    evdev_t *dev = (evdev_t*)kmalloc(sizeof(evdev_t));
    dev->ringbuf = ringbuffer_create(EVDEV_BUF_SIZE, sizeof(input_event_t));
    // Create node
    vnode_t *node = (vnode_t*)kmalloc(sizeof(vnode_t));
    memset(node, 0, sizeof(vnode_t));
    node->type = 0; // TODO
    node->major = 13;
    node->size = 4096;
    node->inode = 0;
    node->read = evdev_read;
    node->mutex = mutex_create();
    node->mnt_info = dev;
    memcpy(node->name, name, strlen(name) + 1);
    vfs_add_node(input_node, node);
    return dev;
}

vnode_t *evdev_lookup(vnode_t *node, const char *name) {
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

void evdev_init() {
    input_node = (vnode_t*)kmalloc(sizeof(vnode_t));
    memset(input_node, 0, sizeof(vnode_t));
    memcpy(input_node->name, "input", 6);
    input_node->type = FS_DIR;
    input_node->size = 4096;
    input_node->inode = 0;
    input_node->lookup = evdev_lookup;
    vfs_add_node(vfs_open(root_node, "dev"), input_node);
}

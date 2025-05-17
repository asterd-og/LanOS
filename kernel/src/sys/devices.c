#include <devices.h>
#include <heap.h>
#include <string.h>
#include <stdio.h>

dev_t *devices[128];
int dev_count = 1;

size_t con_write(vnode_t *node, uint8_t *buffer, size_t len) {
    return printf("%.*s", len, buffer);
}

void dev_init() {
    // Initialize con device (console)
    dev_t *con_dev = (dev_t*)kmalloc(sizeof(dev_t));
    con_dev->name = (char*)kmalloc(4);
    memcpy(con_dev->name, "CON\0", 4);
    vnode_t *con_node = (vnode_t*)kmalloc(sizeof(vnode_t));
    memset(con_node, 0, sizeof(vnode_t));
    memcpy(con_node->name, con_dev->name, 4);
    con_node->type = FS_DEV;
    con_node->write = con_write;
    con_dev->node = con_node;
    devices[0] = con_dev;
}

dev_t *dev_get(const char *name) {
    for (int i = 0; i < dev_count; i++) {
        if (!strcmp(devices[i]->name, name))
            return devices[i];
    }
    return NULL;
}

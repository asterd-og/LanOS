#include <devices.h>
#include <heap.h>
#include <string.h>
#include <stdio.h>
#include <kernel.h>
#include <pmm.h>
#include <serial.h>

dev_t *devices[128];
int dev_count = 0;

size_t con_write(vnode_t *node, uint8_t *buffer, size_t len) {
    return printf("%.*s", len, buffer);
}

size_t fb_read(vnode_t *node, uint8_t *buffer, size_t len) {
    struct limine_framebuffer *fb = node->data;
    uint64_t *buf = (uint64_t*)buffer;
    buf[0] = fb->width;
    buf[1] = fb->height;
    buf[2] = fb->pitch;
    buf[3] = fb->bpp;
    return 32;
}

void dev_init() {
    // Initialize con device (console) and fb device (framebuffer)
    dev_new("CON", NULL, con_write);
    dev_t *fb_dev = dev_new("FB0", fb_read, NULL);
    struct limine_framebuffer *fb = framebuffer_response->framebuffers[0];
    fb_dev->node->data = fb;
    fb_dev->map_info.start_addr = (uint64_t)fb->address;
    fb_dev->map_info.page_count = DIV_ROUND_UP(fb->width * fb->height * (fb->bpp / 4), PAGE_SIZE);
}

dev_t *dev_new(const char *name, void *read, void *write) {
    dev_t *dev = (dev_t*)kmalloc(sizeof(dev_t));
    memset(dev, 0, sizeof(dev_t));
    int name_len = strlen(name);
    dev->name = (char*)kmalloc(name_len + 1);
    memset(dev->name, 0, name_len + 1);
    memcpy(dev->name, name, name_len);
    vnode_t *dev_node = (vnode_t*)kmalloc(sizeof(vnode_t));
    memset(dev_node, 0, sizeof(vnode_t));
    memcpy(dev_node->name, dev->name, name_len);
    dev_node->type = FS_DEV;
    dev_node->read = read;
    dev_node->write = write;
    dev_node->mnt_info = dev;
    dev->node = dev_node;
    devices[dev_count++] = dev;
    return dev;
}

dev_t *dev_get(const char *name) {
    for (int i = 0; i < dev_count; i++) {
        if (!strcmp(devices[i]->name, name))
            return devices[i];
    }
    return NULL;
}

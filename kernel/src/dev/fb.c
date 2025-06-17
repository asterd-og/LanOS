#include <devfs.h>
#include <kernel.h>
#include <string.h>
#include <stdio.h>
#include <heap.h>
#include <errno.h>

#define FBIOGET_VSCREENINFO	0x4600

struct fb_bitfield {
    uint32_t offset;
    uint32_t length;
    uint32_t msb_right;
};

struct fb_var_screeninfo {
    uint32_t xres;
    uint32_t yres;
    uint32_t xres_virtual;
    uint32_t yres_virtual;
    uint32_t xoffset;
    uint32_t yoffset;
    uint32_t bits_per_pixel;
    struct fb_bitfield red;
    struct fb_bitfield green;
    struct fb_bitfield blue;
    struct fb_bitfield transp;
    uint32_t height;
    uint32_t width;
};

typedef struct {
    struct fb_var_screeninfo vsi;
    struct limine_framebuffer *limine_fb;
} fb_data_t;

int devfb_ioctl(vnode_t *node, uint64_t request, void *arg) {
    switch (request) {
        case FBIOGET_VSCREENINFO: {
            fb_data_t *data = (fb_data_t*)node->mnt_info;
            memcpy(arg, &data->vsi, sizeof(struct fb_var_screeninfo));
            break;
        }
        default:
            return -EINVAL;
    }
    return 0;
}

size_t devfb_write(vnode_t *node, uint8_t *buffer, size_t off, size_t len) {
    fb_data_t *data = (fb_data_t*)node->mnt_info;
    size_t fb_size = data->limine_fb->pitch * data->limine_fb->height;
    if (off >= fb_size) return 0;
    if (off + len > fb_size) len = fb_size - off;

    memcpy((uint8_t*)data->limine_fb->address + off, buffer, len);
    return len;
}

void devfb_setup_fb(vnode_t *node, struct limine_framebuffer *fb) {
    fb_data_t *data = (fb_data_t*)kmalloc(sizeof(fb_data_t));
    memset(data, 0, sizeof(fb_data_t));
    data->vsi.bits_per_pixel = fb->bpp;
    data->vsi.width = fb->width;
    data->vsi.height = fb->height;
    data->vsi.xres = fb->width;
    data->vsi.yres = fb->height;
    data->vsi.red.offset = fb->red_mask_shift;
    data->vsi.red.length = fb->red_mask_size;
    data->vsi.green.length = fb->green_mask_size;
    data->vsi.green.length = fb->green_mask_size;
    data->vsi.blue.length = fb->blue_mask_size;
    data->vsi.blue.length = fb->blue_mask_size;
    data->limine_fb = fb;
    node->mnt_info = data;
}

void devfb_init() {
    char name[64];
    for (int i = 0; i < framebuffer_response->framebuffer_count; i++) {
        sprintf(name, "fb%d", i);
        vnode_t *node = devfs_register_dev(name, NULL, devfb_write, devfb_ioctl);
        devfb_setup_fb(node, framebuffer_response->framebuffers[i]);
    }
}

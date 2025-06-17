#pragma once

#include <stdint.h>
#include <stddef.h>
#include <vfs.h>

void devfs_init();
vnode_t *devfs_register_dev(char *name, void *read, void *write, void *ioctl);
void devfb_init();
void tty_init();

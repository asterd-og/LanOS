#include <serial.h>
#include <ports.h>
#include <vfs.h>
#include <devfs.h>
#include <stdio.h>

void serial_printf(const char *str, ...) {
    va_list va;
    va_start(va, str);
    char buffer[1024];
    vsnprintf(buffer, (size_t)-1, str, va);
    va_end(va);
    int i = 0;
    while (buffer[i])
        outb(0xe9, buffer[i++]);
}

size_t serial_write(vnode_t *node, uint8_t *buffer, size_t off, size_t len) {
    for (int i = 0; i < len; i++) {
        // outb(0xe9, buffer[i]);
    }
    return len;
}

void serial_init() {
    devfs_register_dev("serial", NULL, serial_write, NULL);
}

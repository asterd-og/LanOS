#pragma once

#include <stdint.h>
#include <vfs.h>

typedef struct {
    char *name;
    vnode_t *node;
    struct {
        uint64_t start_addr;
        uint64_t page_count;
    } map_info;
} dev_t;

void dev_init();
dev_t *dev_new(const char *name, void *read, void *write);
dev_t *dev_get(const char *name);

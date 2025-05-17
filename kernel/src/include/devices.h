#pragma once

#include <stdint.h>
#include <vfs.h>

typedef struct {
    char *name;
    vnode_t *node;
} dev_t;

void dev_init();
dev_t *dev_get(const char *name);

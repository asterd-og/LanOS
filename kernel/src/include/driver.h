#pragma once

#include <stdint.h>
#include <vfs.h>

void driver_load(uint8_t *elf);
void driver_load_node(vnode_t *node);

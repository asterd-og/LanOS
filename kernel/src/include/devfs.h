#pragma once

#include <stdint.h>
#include <stddef.h>

void devfs_init();
void devfs_register_dev(char *name, void *read, void *write);

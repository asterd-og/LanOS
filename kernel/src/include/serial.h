#pragma once

#include <stdint.h>
#include <stddef.h>

void serial_init();
void serial_printf(const char *str, ...);

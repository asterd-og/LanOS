#pragma once

#include <stdint.h>

#define FS_BASE 0xC0000100

uint64_t read_msr(uint32_t msr);
void write_msr(uint32_t msr, uint64_t value);
void cpu_enable_sse();

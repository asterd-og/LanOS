#pragma once

#include <stdint.h>

uint64_t read_msr(uint32_t msr);
void write_msr(uint32_t msr, uint64_t value);

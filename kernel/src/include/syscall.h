#pragma once

#include <stdint.h>
#include <stddef.h>

typedef struct {
    uint64_t rax;
    uint64_t rdi;
    uint64_t rsi;
    uint64_t rdx;
    uint64_t r10;
    uint64_t r8;
    uint64_t r9;
} syscall_frame_t;

void syscall_init();

#pragma once

#include <stdint.h>
#include <stddef.h>
#include <log.h>

#define ASSERT(x) \
    do { \
        if (!(x)) { \
            LOG_ERROR("Assertion failed: " #x "\n"); \
            while (1) { \
                __asm__ volatile ("cli;hlt"); \
            } \
        } \
    } while (0)

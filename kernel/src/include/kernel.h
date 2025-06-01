#pragma once

#include <stdint.h>
#include <stddef.h>
#include <limine.h>

typedef struct {
    const char *name;
    void *addr;
} kernel_sym_t;

extern struct limine_framebuffer_response *framebuffer_response;

extern uint64_t hhdm_offset;

uint64_t kernel_find_sym(const char *name);

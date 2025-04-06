#pragma once

#include <stdint.h>
#include <stddef.h>
#include <kernel.h>
#include <limine.h>

#define PAGE_SIZE 4096

#define HIGHER_HALF(x) (typeof(x))((uint64_t)x + hhdm_offset)
#define PHYSICAL(x) (typeof(x))((uint64_t)x - hhdm_offset)

#define DIV_ROUND_UP(x, y) (x + (y - 1)) / y
#define ALIGN_UP(x, y) DIV_ROUND_UP(x, y) * y
#define ALIGN_DOWN(x, y) ((x / y) * y)

extern struct limine_memmap_response *memmap;

extern uint64_t pmm_bitmap_start;
extern uint64_t pmm_bitmap_size;
extern uint64_t pmm_bitmap_pages;

void pmm_init();
void *pmm_request();
void pmm_free(void *ptr);

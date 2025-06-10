#include <pmm.h>
#include <stdio.h>
#include <limine.h>
#include <log.h>
#include <string.h>
#include <stdbool.h>

uint8_t *bitmap = NULL;
uint64_t bitmap_size = 0;
uint64_t bitmap_last_free = 0;

uint64_t pmm_bitmap_start = 0;
uint64_t pmm_bitmap_size = 0;
uint64_t pmm_bitmap_pages = 0;

__attribute__((used, section(".limine_requests")))
static volatile struct limine_memmap_request limine_memmap = {
    .id = LIMINE_MEMMAP_REQUEST,
    .revision = 0
};

struct limine_memmap_response *memmap = NULL;

void bitmap_clear(uint64_t bit);

void pmm_init() {
    memmap = limine_memmap.response;
    uint64_t page_count = 0;
    for (uint64_t i = 0; i < memmap->entry_count; i++) {
        struct limine_memmap_entry *entry = memmap->entries[i];
        if (entry->length % PAGE_SIZE != 0)
            LOG_WARNING("Memory map entry length is NOT divisible by the page size. Memory issues MAY arise.\n");
        page_count += entry->length;
    }
    page_count /= PAGE_SIZE;
    pmm_bitmap_pages = page_count;
    bitmap_size = ALIGN_UP(page_count / 8, PAGE_SIZE);
    for (uint64_t i = 0; i < memmap->entry_count; i++) {
        struct limine_memmap_entry *entry = memmap->entries[i];
        if (entry->length >= bitmap_size && entry->type == LIMINE_MEMMAP_USABLE) {
            bitmap = HIGHER_HALF((uint8_t*)entry->base);
            entry->length -= bitmap_size;
            entry->base += bitmap_size;
            memset(bitmap, 0xFF, bitmap_size);
            break;
        }
    }
    for (uint64_t i = 0; i < memmap->entry_count; i++) {
        struct limine_memmap_entry *entry = memmap->entries[i];
        if (entry->type == LIMINE_MEMMAP_USABLE)
            for (uint64_t j = 0; j < entry->length; j += PAGE_SIZE)
                bitmap_clear((entry->base + j) / PAGE_SIZE);
    }
    pmm_bitmap_start = (uint64_t)bitmap;
    pmm_bitmap_size = bitmap_size;
    LOG_OK("PMM Initialised.\n");
}

void bitmap_clear(uint64_t bit) {
    bitmap[bit / 8] &= ~(1 << (bit % 8));
}

void bitmap_set(uint64_t bit) {
    bitmap[bit / 8] |= (1 << (bit % 8));
}

bool bitmap_test(uint64_t bit) {
    return (bitmap[bit / 8] & (1 << (bit % 8))) != 0;
}

void *pmm_request() {
    // TODO: Handle when we can't find a free bit after bitmap_last_free
    // (reiterate through the bitmap, and if we dont find ANY frees, we panic.)
    uint64_t bit = bitmap_last_free;
    while (bit < pmm_bitmap_pages && bitmap_test(bit))
        bit++;
    if (bit >= pmm_bitmap_pages) {
        bit = 0;
        while (bit < bitmap_last_free && bitmap_test(bit))
            bit++;
        if (bit == bitmap_last_free) {
            LOG_ERROR("PMM Out of memory!\n");
            return NULL;
        }
    }
    bitmap_set(bit);
    bitmap_last_free = bit;
    return (void*)(bit * PAGE_SIZE);
}

void pmm_free(void *ptr) {
    uint64_t bit = (uint64_t)ptr / PAGE_SIZE;
    bitmap_clear(bit);
}

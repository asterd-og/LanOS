#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <limine.h>
#include <kernel.h>
#include <string.h>
#include <stdio.h>
#include <flanterm/flanterm.h>
#include <flanterm/backends/fb.h>
#include <gdt.h>
#include <idt.h>
#include <pmm.h>
#include <vmm.h>
#include <log.h>
#include <heap.h>
#include <acpi.h>
#include <madt.h>
#include <apic.h>
#include <smp.h>
#include <assert.h>
#include <pit.h>

__attribute__((used, section(".limine_requests")))
static volatile LIMINE_BASE_REVISION(3);

__attribute__((used, section(".limine_requests")))
static volatile struct limine_framebuffer_request framebuffer_request = {
    .id = LIMINE_FRAMEBUFFER_REQUEST,
    .revision = 0
};

__attribute__((used, section(".limine_requests")))
static volatile struct limine_hhdm_request hhdm_request = {
    .id = LIMINE_HHDM_REQUEST,
    .revision = 0
};

__attribute__((used, section(".limine_requests_start")))
static volatile LIMINE_REQUESTS_START_MARKER;

__attribute__((used, section(".limine_requests_end")))
static volatile LIMINE_REQUESTS_END_MARKER;

struct flanterm_context *flanterm_ctx = NULL;
uint64_t hhdm_offset = 0;

static void hcf(void) {
    for (;;) {
#if defined (__x86_64__)
        asm ("hlt");
#elif defined (__aarch64__) || defined (__riscv)
        asm ("wfi");
#elif defined (__loongarch64)
        asm ("idle 0");
#endif
    }
}

void _putchar(char c) {
    flanterm_write(flanterm_ctx, &c, 1);
}

void kernel_task(void);

void kmain() {
    if (LIMINE_BASE_REVISION_SUPPORTED == false) {
        hcf();
    }

    hhdm_offset = hhdm_request.response->offset;

    if (framebuffer_request.response == NULL
     || framebuffer_request.response->framebuffer_count < 1) {
        hcf();
    }

    struct limine_framebuffer *framebuffer = framebuffer_request.response->framebuffers[0];

    flanterm_ctx = flanterm_fb_init(
            NULL,
            NULL,
            framebuffer->address,
            framebuffer->width, framebuffer->height, framebuffer->pitch,
            framebuffer->red_mask_size, framebuffer->red_mask_shift,
            framebuffer->green_mask_size, framebuffer->green_mask_shift,
            framebuffer->blue_mask_size, framebuffer->blue_mask_shift,
            NULL,
            NULL, NULL,
            NULL, NULL,
            NULL, NULL,
            NULL, 0, 0, 1,
            0, 0,
            0
        );

    gdt_init();
    idt_init();
    pmm_init();
    vmm_init();
    slab_init();
    acpi_init();
    madt_init();
    lapic_init();
    ioapic_init();
    pit_init();
    smp_init();
    sched_install();
    sched_init();
    sched_new_task(0, kernel_task);
    lapic_ipi_all(0, SCHED_VEC);

    hcf();
}

void kernel_task(void) {
    printf("\x1b[36mLanOS\x1b[37m Booted successfully!\n");
    while (1) {
    }
}

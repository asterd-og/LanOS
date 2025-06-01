#include "spinlock.h"
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
#include <pit.h>
#include <pci.h>
#include <ahci.h>
#include <sched.h>
#include <ipc.h>
#include <vfs.h>
#include <syscall.h>
#include <devices.h>
#include <driver.h>
#include <ports.h>

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

struct limine_framebuffer_response *framebuffer_response;

void kmain() {
    if (LIMINE_BASE_REVISION_SUPPORTED == false) {
        hcf();
    }

    hhdm_offset = hhdm_request.response->offset;

    if (framebuffer_request.response == NULL
     || framebuffer_request.response->framebuffer_count < 1) {
        hcf();
    }

    framebuffer_response = framebuffer_request.response;
    struct limine_framebuffer *framebuffer = framebuffer_response->framebuffers[0];

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

    gdt_init(0);
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
    pci_init();
    ahci_init();
    char mnt_point = vfs_mount_disk(ahci_ports[0]);
    LOG_OK("Mounted disk #0 as FAT32 (Letter %c).\n", mnt_point);
    sched_install();
    sched_init();
    dev_init();
    syscall_init();

    vnode_t *kb_node = vfs_open("A:/ps2kb.o");
    driver_load_node(kb_node);
    LOG_OK("PS/2 Keyboard driver loaded.\n");
    vnode_t *mouse_node = vfs_open("A:/ps2mouse.o");
    driver_load_node(mouse_node);
    LOG_OK("PS/2 Mouse driver loaded.\n");

    vnode_t *init = vfs_open("A:/init");
    char *argv[] = {"init"};
    sched_load_elf(2, init, 1, argv);
    vfs_close(init);

    lapic_ipi_all(0, SCHED_VEC);

    hcf();
}

kernel_sym_t kernel_sym_table[] ={
    {"printf_", printf},
    {"idt_install_irq", idt_install_irq},
    {"lapic_eoi", lapic_eoi},
    {"memset", memset},
    {"memcpy", memcpy},
    {"kmalloc", kmalloc},
    {"krealloc", krealloc},
    {"kfree", kfree},
    {"spinlock_lock", spinlock_lock},
    {"spinlock_free", spinlock_free},
    {"dev_new", dev_new},
};

uint64_t kernel_find_sym(const char *name) {
    for (int i = 0; i < sizeof(kernel_sym_table) / sizeof(kernel_sym_t); i++) {
        if (!strcmp(name, kernel_sym_table[i].name))
            return (uint64_t)kernel_sym_table[i].addr;
    }
    return 0;
}

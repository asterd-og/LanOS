#include <sched.h>
#include <pmm.h>
#include <string.h>

uint64_t sys_mmap(context_t *ctx) {
    void *addr = (void*)ctx->rdi;
    size_t length = (size_t)ctx->rsi;
    int prot = (size_t)ctx->rdx;
    int flags = (size_t)ctx->rcx;
    int fd = (size_t)ctx->r10;

    uint64_t vm_flags = MM_USER;
    if (prot & 2) vm_flags |= MM_READ;
    if (prot & 4) vm_flags |= MM_WRITE;
    if (!(prot & 1)) vm_flags |= MM_NX;

    size_t pages = DIV_ROUND_UP(length, PAGE_SIZE);
    void *virt_addr = NULL;

    if (!addr) {
        // Allocate new address
        virt_addr = vmm_alloc(this_thread()->pagemap, pages, true);
    } else {
        // TODO: Check if a mapping already exists in this address
        // if it does, pick another address.
        uint64_t phys = 0;
        for (size_t i = 0; i < pages; i++) {
            phys = (uint64_t)pmm_request();
            vmm_map(this_thread()->pagemap, (uint64_t)addr + i * PAGE_SIZE, phys, vm_flags);
        }
        virt_addr = addr;
    }

    if (flags & 1)
        memset(virt_addr, 0, length);

    return (uint64_t)virt_addr;
}

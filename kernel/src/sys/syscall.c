#include <errno.h>
#include <syscall.h>
#include <vfs.h>
#include <idt.h>
#include <stdio.h>
#include <log.h>
#include <sched.h>
#include <spinlock.h>
#include <smp.h>
#include <fd.h>
#include <cpu.h>
#include <vmm.h>
#include <pmm.h>
#include <string.h>

typedef ptrdiff_t ssize_t;

// Argument order: rdi, rsi, rdx, rcx, r10, r8 and r9

void sys_read(context_t *ctx) {
    uint32_t fd_idx = (uint32_t)ctx->rdi;
    void *buf = (void*)ctx->rsi;
    size_t count = (size_t)ctx->rdx;
    fd_t *fd = this_proc()->fd_table[fd_idx];
    if (!fd) {
        ctx->rax = (ssize_t)-EBADF;
        return;
    }
    ctx->rax = vfs_read(fd->node, buf, fd->off, count);
    fd->off += ctx->rax;
}

void sys_write(context_t *ctx) {
    uint32_t fd_idx = (uint32_t)ctx->rdi;
    void *buf = (void*)ctx->rsi;
    size_t count = (size_t)ctx->rdx;
    fd_t *fd = this_proc()->fd_table[fd_idx];
    if (!fd) {
        ctx->rax = (ssize_t)-EBADF;
        return;
    }
    ctx->rax = vfs_write(fd->node, buf, fd->off, count);
    fd->off += ctx->rax;
}

void sys_open(context_t *ctx) {
    const char *path = (const char*)ctx->rdi;
    int flags = (int)ctx->rsi;
    unsigned int mode = (unsigned int)ctx->rdx;
    proc_t *proc = this_proc();
    proc->fd_table[proc->fd_count++] = fd_open(path, flags);
    ctx->rax = proc->fd_count - 1;
}

void sys_lseek(context_t *ctx) {
    uint32_t fd_idx = (uint32_t)ctx->rdi;
    long offset = (long)ctx->rsi;
    int whence = (int)ctx->rdx;
    fd_t *fd = this_proc()->fd_table[fd_idx];
    if (!fd) {
        ctx->rax = (ssize_t)-EBADF;
        return;
    }
    switch (whence) {
        case 0: // SEEK_SET
            if (offset < 0) {
                ctx->rax = (ssize_t)-EINVAL;
                return;
            }
            fd->off = offset;
            break;
        case 1: // SEEK_CUR
            if (fd->off + offset < 0) {
                ctx->rax = (ssize_t)-EINVAL;
                return;
            }
            fd->off += offset;
            break;
        case 2: // SEEK_END
            fd->off = fd->node->size;
            break;
        default:
            ctx->rax = (ssize_t)-EINVAL;
            return;
            break;
    }
    ctx->rax = fd->off;
}

void sys_mmap(context_t *ctx) {
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

    ctx->rax = (uint64_t)virt_addr;
}

void sys_exit(context_t *ctx) {
    sched_exit((int)ctx->rdi);
}

void sys_arch_prctl(context_t *ctx) {
    int op = (int)ctx->rdi;
    long extra = (long)ctx->rsi;
    switch (op) {
        case 0x1002:
            write_msr(0xC0000100, extra);
            ctx->rax = 0;
            break;
        default:
            printf("[sys_arch_prctl]: %d not implemented.\n", op);
            ctx->rax = -EINVAL;
            break;
    }
}

void sys_gettid(context_t *ctx) {
    ctx->rax = this_thread()->id;
}

void *syscall_handler_table[] = {
    [0] = sys_read,
    [1] = sys_write,
    [2] = sys_open,
    [8] = sys_lseek,
    [9] = sys_mmap,
    [60] = sys_exit,
    [158] = sys_arch_prctl,
    [186] = sys_gettid
};

void syscall_handler(context_t *ctx) {
    if (ctx->rax > sizeof(syscall_handler_table) / sizeof(uint64_t) || !syscall_handler_table[ctx->rax]) {
        printf("Unhandled syscall %d.\n", ctx->rax);
        return;
    }
    void(*handler)(context_t*) = syscall_handler_table[ctx->rax];
    handler(ctx);
}

void syscall_init() {
    idt_install_irq(0x60, syscall_handler);
}

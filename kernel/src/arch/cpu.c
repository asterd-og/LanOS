#include <cpu.h>

uint64_t read_msr(uint32_t msr) {
    uint32_t low, high;
    __asm__ volatile ("rdmsr" : "=a"(low), "=d"(high) : "c"(msr));
    return ((uint64_t)high << 32) | low;
}

void write_msr(uint32_t msr, uint64_t value) {
    __asm__ volatile ("wrmsr" : : "a"(value), "d"((uint32_t)(value >> 32)), "c"(msr));
}

void cpu_enable_sse() {
    __asm__ __volatile__ (
        "mov %cr0, %rax\n"
        "and $~(1 << 2), %rax\n"  // Clear EM (bit 2)
        "or $(1 << 1), %rax\n"    // Set MP (bit 1)
        "mov %rax, %cr0\n"
        "mov %cr4, %rax\n"
        "or $(3 << 9), %rax\n"    // Set OSFXSR (bit 9) and OSXMMEXCPT (bit 10)
        "mov %rax, %cr4\n"
    );
    __asm__ volatile ("fninit");
}

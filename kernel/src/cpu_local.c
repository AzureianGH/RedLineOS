#include <cpu_local.h>
#include <stddef.h>
#include <stdint.h>

#define MSR_FS_BASE 0xC0000100

static inline void wrmsr(uint32_t msr, uint64_t val) {
    uint32_t lo = (uint32_t)val;
    uint32_t hi = (uint32_t)(val >> 32);
    __asm__ __volatile__("wrmsr" :: "c"(msr), "a"(lo), "d"(hi));
}

void cpu_local_set(cpu_local_t* ptr) {
    wrmsr(MSR_FS_BASE, (uint64_t)ptr);
}

cpu_local_t* cpu_local_get(void) {
    cpu_local_t* ptr;
    __asm__ __volatile__("mov %%fs:0, %0" : "=r"(ptr));
    return ptr;
}

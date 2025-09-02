#include <ioremap.h>
#include <vmm.h>
#include <palloc.h>
#include <lprintf.h>

// Simple bump-pointer VM region for MMIO: 0xFFFF80C000000000 .. 0xFFFF80FFFF000000 (arbitrary)
#define IOR_BASE  0xFFFF80C000000000ULL
#define IOR_SIZE  (256ULL * 1024 * 1024) // 256 MiB window to start

static uint64_t ior_next = IOR_BASE;
static const uint64_t ior_end = IOR_BASE + IOR_SIZE;

void* ioremap(uint64_t phys, size_t size) {
    if (size == 0) return (void*)(uintptr_t)(phys + 0ULL); // degenerate
    // Align to 4K
    uint64_t pa = phys & ~0xFFFULL;
    uint64_t off = (uint64_t)phys & 0xFFFULL;
    uint64_t len = (size + off + 0xFFFULL) & ~0xFFFULL;
    if (ior_next + len > ior_end) {
        error_printf("ioremap: out of VM window (need %llu bytes)\n", (unsigned long long)len);
        return 0;
    }
    uint64_t va = ior_next;
    for (uint64_t i = 0; i < len; i += 0x1000ULL) {
        int rc = vmm_map_page(va + i, pa + i, VMM_P_PRESENT | VMM_P_WRITABLE | VMM_P_NX);
        if (rc) {
            error_printf("ioremap: map fail pa=%#016llx va=%#016llx rc=%d\n", (unsigned long long)(pa+i), (unsigned long long)(va+i), rc);
            return 0;
        }
    }
    ior_next += len;
    debug_printf("ioremap: phys=%#016llx size=%llu -> va=%#016llx\n", (unsigned long long)phys, (unsigned long long)size, (unsigned long long)(va + off));
    return (void*)(uintptr_t)(va + off);
}

void iounmap(void* virt, size_t size) {
    (void)virt; (void)size; // TODO: unmap when vmm_get/unmap exists
}

#include <vmm.h>
#include <boot.h>
#include <palloc.h>

static volatile uint64_t *pml4 = 0; // HHDM-mapped pointer to current PML4

static inline uint64_t read_cr3(void) {
    uint64_t val; __asm__ volatile ("mov %%cr3,%0" : "=r"(val)); return val;
}

static inline void *phys_to_virt(uint64_t phys) {
    return (void *)(phys + hhdm_request.response->offset);
}

void vmm_init(void) {
    uint64_t cr3 = read_cr3();
    uint64_t pml4_phys = cr3 & ~0xFFFULL;
    pml4 = (volatile uint64_t *)phys_to_virt(pml4_phys);
}

static inline volatile uint64_t *ensure_table(volatile uint64_t *parent, size_t idx, uint64_t flags) {
    uint64_t entry = parent[idx];
    if (!(entry & VMM_P_PRESENT)) {
        void *page = palloc_allocate_page();
        if (!page) return 0;
        // Zero new table
        for (size_t i = 0; i < 4096/8; i++) ((volatile uint64_t *)page)[i] = 0;
        uint64_t phys = (uint64_t)(uintptr_t)page - hhdm_request.response->offset;
        parent[idx] = phys | (flags & (VMM_P_PRESENT|VMM_P_WRITABLE|VMM_P_USER));
        entry = parent[idx];
    }
    uint64_t child_phys = entry & ~0xFFFULL;
    return (volatile uint64_t *)phys_to_virt(child_phys);
}

int vmm_map_page(uint64_t va, uint64_t pa, uint64_t flags) {
    if (!pml4) vmm_init();
    size_t pml4_i = (va >> 39) & 0x1FF;
    size_t pdpt_i = (va >> 30) & 0x1FF;
    size_t pd_i   = (va >> 21) & 0x1FF;
    size_t pt_i   = (va >> 12) & 0x1FF;

    volatile uint64_t *pdpt = ensure_table(pml4, pml4_i, VMM_P_PRESENT|VMM_P_WRITABLE);
    if (!pdpt) return -1;
    volatile uint64_t *pd   = ensure_table(pdpt, pdpt_i, VMM_P_PRESENT|VMM_P_WRITABLE);
    if (!pd) return -1;
    volatile uint64_t *pt   = ensure_table(pd, pd_i, VMM_P_PRESENT|VMM_P_WRITABLE);
    if (!pt) return -1;

    pt[pt_i] = (pa & ~0xFFFULL) | (flags & ~(0ULL)) | VMM_P_PRESENT;
    // Invalidate TLB for VA (local)
    __asm__ volatile ("invlpg (%0)" :: "r"(va) : "memory");
    return 0;
}

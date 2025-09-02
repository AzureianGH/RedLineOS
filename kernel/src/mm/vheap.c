#include <vheap.h>
#include <vmm.h>
#include <palloc.h>
#include <boot.h>

static uint64_t heap_base = 0;
static uint64_t heap_size = 0;
static uint64_t heap_commit = 0;

static inline uint64_t align_up(uint64_t x, uint64_t a) { return (x + (a-1)) & ~(a-1); }

int vheap_init(uint64_t base_va, uint64_t size_bytes) {
    heap_base = align_up(base_va, 0x1000);
    heap_size = size_bytes & ~0xFFFULL;
    heap_commit = heap_base;
    if (!heap_size) return -1;
    return 0;
}

uint64_t vheap_commit(size_t bytes) {
    bytes = (size_t)align_up(bytes, 0x1000);
    if (heap_base == 0 || bytes == 0) return 0;
    if ((heap_commit + bytes) > (heap_base + heap_size)) return 0;

    uint64_t va = heap_commit;
    for (uint64_t off = 0; off < bytes; off += 0x1000) {
        void *page = palloc_allocate_page();
        if (!page) return 0;
        uint64_t pa = (uint64_t)(uintptr_t)page - hhdm_request.response->offset;
        if (vmm_map_page(va + off, pa, VMM_P_PRESENT|VMM_P_WRITABLE) != 0) return 0;
    }
    heap_commit += bytes;
    return va;
}

void vheap_bounds(uint64_t* base_va, uint64_t* size_bytes) {
    if (base_va) *base_va = heap_base;
    if (size_bytes) *size_bytes = heap_size;
}

int vheap_map_one(uint64_t va) {
    if (heap_base == 0) return -1;
    if (va < heap_base || va >= (heap_base + heap_size)) return -1;
    void *page = palloc_allocate_page();
    if (!page) return -1;
    uint64_t pa = (uint64_t)(uintptr_t)page - hhdm_request.response->offset;
    return vmm_map_page(va & ~0xFFFULL, pa, VMM_P_PRESENT|VMM_P_WRITABLE);
}

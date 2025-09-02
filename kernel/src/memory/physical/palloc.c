// Freestanding physical page allocator using a singly linked free list
// embedded in pages themselves. Pages are returned as HHDM-mapped virtual
// addresses so the kernel can directly access them.

#include <palloc.h>
#include <boot.h>
#include <limine.h>
#include <stddef.h>
#include <stdbool.h>
#include <extendedint.h>
#include <lock.h>

#define PAGE_SIZE 4096ULL
#define PAGE_MASK (PAGE_SIZE - 1ULL)

static ulong totalentrycount = 0;     // total memmap entries observed (debug)
static ulong totalpagecount = 0;      // total USABLE pages managed by palloc
static ulong freepagecount = 0;       // pages in free list
static ulong usedpagecount = 0;       // allocated pages (accounting only)

// Head of the free list (HHDM-mapped address of a free page). The first
// pointer-sized word in a free page stores the next pointer.
static void *free_list_head = NULL;
static spinlock_t palloc_lock;

static inline uint64_t align_up_u64(uint64_t x, uint64_t a) {
    return (x + (a - 1)) & ~(a - 1);
}
static inline uint64_t align_down_u64(uint64_t x, uint64_t a) {
    return x & ~(a - 1);
}

static inline void *phys_to_virt(uint64_t phys) {
    return (void *)(phys + hhdm_request.response->offset);
}

static void push_free_page(uint64_t phys_addr) {
    void **page_virt = (void **)phys_to_virt(align_down_u64(phys_addr, PAGE_SIZE));
    // Store current head as next
    *page_virt = free_list_head;
    free_list_head = (void *)page_virt;
    freepagecount++;
}

void palloc_init(struct limine_memmap_response* memmap) {
    spinlock_init(&palloc_lock);
    free_list_head = NULL;
    totalentrycount = 0;
    totalpagecount = 0;
    freepagecount = 0;
    usedpagecount = 0;

    if (memmap == NULL || memmap->entry_count == 0) {
        return;
    }

    totalentrycount = (ulong)memmap->entry_count;

    // Find Uusable
    for (uint64_t i = 0; i < memmap->entry_count; i++) {
        struct limine_memmap_entry *e = memmap->entries[i];
        if (e == NULL) continue;
        if (e->type != LIMINE_MEMMAP_USABLE) continue;

        uint64_t start = align_up_u64(e->base, PAGE_SIZE);
        uint64_t end   = align_down_u64(e->base + e->length, PAGE_SIZE);
        if (end <= start) continue;

        uint64_t pages = (end - start) / PAGE_SIZE;
        totalpagecount += (ulong)pages;

        for (uint64_t p = start; p < end; p += PAGE_SIZE) {
            push_free_page(p);
        }
    }
}

void* palloc_allocate_page(void) {
    spin_lock(&palloc_lock);
    if (free_list_head == NULL) {
        spin_unlock(&palloc_lock);
        return NULL;
    }
    void *page = free_list_head;
    void *next = *(void **)page;
    free_list_head = next;
    freepagecount--;
    usedpagecount++;
    spin_unlock(&palloc_lock);
    return page;
}

void* palloc_zero_allocate_page(void) {
    void *page = palloc_allocate_page();
    if (page != NULL) {
        // Zero the page
        for (size_t i = 0; i < PAGE_SIZE; i++) {
            ((uint8_t *)page)[i] = 0;
        }
    }
    return page;
}

void palloc_free_page(void* page) {
    if (page == NULL) return;
    // Ensure alignment
    uint64_t addr = (uint64_t)(uintptr_t)page;
    if (addr & PAGE_MASK) {
        // Not page-aligned, ignore.
        return;
    }
    // Push back onto the free list
    spin_lock(&palloc_lock);
    *(void **)page = free_list_head;
    free_list_head = page;
    freepagecount++;
    if (usedpagecount > 0) usedpagecount--;
    spin_unlock(&palloc_lock);
}

bool palloc_is_page_allocated(void* page) {
    if (page == NULL) return false;
    // Linear scan of free list. This is O(n), acceptable as a debug utility.
    for (void *it = free_list_head; it != NULL; it = *(void **)it) {
        if (it == page) return false; // found in free list
    }
    return true; // not found => considered allocated (or not managed)
}

size_t palloc_get_free_page_count(void) {
    return (size_t)freepagecount;
}

size_t palloc_get_total_page_count(void) {
    return (size_t)totalpagecount;
}

size_t palloc_get_used_page_count(void) {
    return (size_t)usedpagecount;
}
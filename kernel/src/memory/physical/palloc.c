// Freestanding physical page allocator using a singly linked free list
// embedded in pages themselves. Pages are returned as HHDM-mapped virtual
// addresses so the kernel can directly access them.

#include <palloc.h>
#include <boot.h>
#include <limine.h>
#include <stddef.h>
#include <stdbool.h>
#include <extendedint.h>
#include <stdint.h>
#include <lock.h>
#include <string.h>

#define PAGE_SIZE 4096ULL
#define PAGE_MASK (PAGE_SIZE - 1ULL)

static ulong totalentrycount = 0;     // total memmap entries observed (debug)
static ulong totalpagecount = 0;      // total USABLE pages managed by palloc
static ulong freepagecount = 0;       // free pages available (ranges + free list)
static ulong usedpagecount = 0;       // allocated pages (accounting only)

// Head of the free list (HHDM-mapped address of a free page). The first
// pointer-sized word in a free page stores the next pointer.
static void *free_list_head = NULL;
static spinlock_t palloc_lock;

// Lazy allocation from usable ranges instead of pushing every page at init
typedef struct {
    uint64_t start;   // inclusive physical address (aligned)
    uint64_t end;     // exclusive physical address (aligned)
    uint64_t cursor;  // next physical address to hand out
} p_range_t;

#define PALLOC_MAX_RANGES 128
static p_range_t ranges[PALLOC_MAX_RANGES];
static uint32_t range_count = 0;
static uint32_t range_curr = 0;

static inline uint64_t align_up_u64(uint64_t x, uint64_t a) {
    return (x + (a - 1)) & ~(a - 1);
}
static inline uint64_t align_down_u64(uint64_t x, uint64_t a) {
    return x & ~(a - 1);
}

static inline void *phys_to_virt(uint64_t phys) {
    return (void *)(phys + hhdm_request.response->offset);
}

void palloc_init(struct limine_memmap_response* memmap) {
    spinlock_init(&palloc_lock);
    free_list_head = NULL;
    totalentrycount = 0;
    totalpagecount = 0;
    freepagecount = 0;
    usedpagecount = 0;
    range_count = 0;
    range_curr = 0;

    if (memmap == NULL || memmap->entry_count == 0) {
        return;
    }

    totalentrycount = (ulong)memmap->entry_count;

    // Collect usable ranges lazily (no per-page touching)
    for (uint64_t i = 0; i < memmap->entry_count; i++) {
        struct limine_memmap_entry *e = memmap->entries[i];
        if (e == NULL) continue;
        if (e->type != LIMINE_MEMMAP_USABLE) continue;

        uint64_t start = align_up_u64(e->base, PAGE_SIZE);
        uint64_t end   = align_down_u64(e->base + e->length, PAGE_SIZE);
        if (end <= start) continue;
        uint64_t pages = (end - start) / PAGE_SIZE;
        totalpagecount += (ulong)pages;
        if (range_count < PALLOC_MAX_RANGES) {
            ranges[range_count].start = start;
            ranges[range_count].end = end;
            ranges[range_count].cursor = start;
            range_count++;
        } else {
            // If too many ranges, conservatively skip adding; we still track totals.
            // In the worst case this reduces available memory management but keeps booting.
        }
    }
    // Initially, all pages are free (either as not-yet-handed-out range space or in the free list)
    freepagecount = totalpagecount;
}

void* palloc_allocate_page(void) {
    spin_lock(&palloc_lock);
    // Prefer reusing freed pages
    if (free_list_head != NULL) {
        void *page = free_list_head;
        void *next = *(void **)page;
        free_list_head = next;
        if (freepagecount > 0) freepagecount--; // consume one free page
        usedpagecount++;
        spin_unlock(&palloc_lock);
        return page;
    }
    // Else allocate lazily from usable ranges
    while (range_curr < range_count) {
        p_range_t *r = &ranges[range_curr];
        if (r->cursor < r->end) {
            uint64_t phys = r->cursor;
            r->cursor += PAGE_SIZE;
            if (freepagecount > 0) freepagecount--; // one less free page remaining
            usedpagecount++;
            void *page = phys_to_virt(phys);
            spin_unlock(&palloc_lock);
            return page;
        }
        // Move to next range
        range_curr++;
    }
    // Out of memory
    spin_unlock(&palloc_lock);
    return NULL;
}

void* palloc_zero_allocate_page(void) {
    void *page = palloc_allocate_page();
    if (page != NULL) {
        // Zero the page (use memset for speed)
        memset(page, 0, PAGE_SIZE);
    }
    return page;
}

void palloc_free_page(void* page) {
    if (page == NULL) return;
    // Ensure alignment
    uint64_t addr = (uint64_t)(unsigned long long)page;
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
    // Convert to phys to check lazy ranges
    uint64_t phys = (uint64_t)(unsigned long long)page - hhdm_request.response->offset;
    // First, check if it lies within any managed range
    for (uint32_t i = 0; i < range_count; ++i) {
        p_range_t *r = &ranges[i];
        if (phys >= r->start && phys < r->end) {
            // If not yet handed out, it's free
            if (phys >= r->cursor) return false;
            // If handed out before, check if it was freed back into free list
            for (void *it = free_list_head; it != NULL; it = *(void **)it) {
                if (it == page) return false;
            }
            return true; // handed out and not in free list => allocated
        }
    }
    // Not in managed ranges: fallback to free list scan (could be manually freed non-managed page)
    for (void *it = free_list_head; it != NULL; it = *(void **)it) {
        if (it == page) return false;
    }
    return true;
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
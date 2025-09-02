// Stelloc: simple free-list allocator that grows by acquiring pages from palloc.
// Block layout in free list: [size (ulong)] [next (ulong)] ...payload...

#include <stelloc.h>
#include <palloc.h>
#include <extendedint.h>
#include <stddef.h>
#include <vheap.h>
#include <lock.h>
#include <slab.h>

#define FREE_HEADER_SIZE  (sizeof(ulong) * 2) // size + next
#define ALLOC_HEADER_SIZE (sizeof(ulong) * 1) // size only
#define ALIGN8(x) (((x) + 7UL) & ~7UL)

static ulong FreeListHead = 0;   // pointer to first free block (virtual)
static int g_mode = STELLOC_SMART;
// Tail bump region taken from a free block to pack small allocations tightly.
static ulong TailPtr = 0;
static ulong TailSize = 0;
static spinlock_t stelloc_lock;

static inline ulong ptr_to_ulong(void *p) { return (ulong)(uintptr_t)p; }
static inline void *ulong_to_ptr(ulong v) { return (void *)(uintptr_t)v; }

// Insert a free block in address-sorted order and coalesce with neighbors.
static void insert_free_sorted(void *addr, ulong size) {
    ulong block = ptr_to_ulong(addr);
    ulong start = block;
    ulong end = block + size;

    // Empty list
    if (FreeListHead == 0) {
        *(ulong *)block = size;
        *((ulong *)block + 1) = 0;
        FreeListHead = block;
        return;
    }

    // Insert at head if before current head
    if (block < FreeListHead) {
        // Try coalesce with old head if adjacent
        ulong head = FreeListHead;
        if (end == head) {
            size += *(ulong *)head;
            *(ulong *)block = size;
            *((ulong *)block + 1) = *((ulong *)head + 1);
            FreeListHead = block;
            return;
        }
        *(ulong *)block = size;
        *((ulong *)block + 1) = FreeListHead;
        FreeListHead = block;
        return;
    }

    // Find insertion point: prev < block <= curr
    ulong prev = FreeListHead;
    ulong curr = *((ulong *)prev + 1);
    while (curr != 0 && curr < block) {
        prev = curr;
        curr = *((ulong *)curr + 1);
    }

    // Check coalesce with prev (prev_end == block)
    ulong prev_size = *(ulong *)prev;
    ulong prev_end = prev + prev_size;
    if (prev_end == block) {
        // Merge into prev
        *(ulong *)prev = prev_size + size;
        // Also try to merge with curr if now adjacent
        if (curr != 0) {
            ulong curr_size = *(ulong *)curr;
            if (prev + *(ulong *)prev == curr) { // prev_end(updated) == curr
                *(ulong *)prev += curr_size;
                *((ulong *)prev + 1) = *((ulong *)curr + 1);
            }
        }
        return;
    }

    // No merge with prev, insert between prev and curr
    *(ulong *)block = size;
    *((ulong *)block + 1) = curr;
    *((ulong *)prev + 1) = block;

    // Coalesce with curr if adjacent (end == curr)
    if (curr != 0) {
        if (end == curr) {
            ulong curr_size = *(ulong *)curr;
            *(ulong *)block = size + curr_size;
            *((ulong *)block + 1) = *((ulong *)curr + 1);
        }
    }
}

static void grow_heap(ulong min_bytes) {
    // Decide how many pages to acquire
    size_t pages = 1;
    if (g_mode == STELLOC_SMART) pages = 4;
    else if (g_mode == STELLOC_AGGRESSIVE) pages = 16;

    // Ensure we cover min_bytes
    size_t need_pages = (size_t)((min_bytes + (4096 - 1)) / 4096);
    if (need_pages > pages) pages = need_pages;

    // Try to batch pages contiguously when possible; we still accept non-contiguous pages
    // by c..c...alescing? (I still can't spell it.) adjacent ones into larger blocks.
    for (size_t i = 0; i < pages; i++) {
        void *pg = palloc_allocate_page();
        if (!pg) break;
        insert_free_sorted(pg, 4096UL);
    }
}

void stelloc_set_mode(int mode) {
    if (mode == STELLOC_DUMB || mode == STELLOC_SMART || mode == STELLOC_AGGRESSIVE) {
        g_mode = mode;
    }
}

int stelloc_get_mode(void) { return g_mode; }

void stelloc_init_heap() {
    FreeListHead = 0;
    g_mode = STELLOC_SMART;
    TailPtr = 0; TailSize = 0;
    spinlock_init(&stelloc_lock);

    vmm_init();
    (void)vheap_init(0xffff900000000000ULL, 16ULL * 1024ULL * 1024ULL * 1024ULL); // 16 GiB
    slab_init();
}

void *stelloc_allocate(ulong size) {
    spin_lock(&stelloc_lock);
    size = ALIGN8(size);
    ulong prev = 0;
    ulong current = FreeListHead;

    // First-fit search (prefer reusing freed space)
    while (current != 0) {
        ulong blockSize = *(ulong *)current;      // full size of free block
        ulong next = *((ulong *)current + 1);
        if (blockSize >= size + ALLOC_HEADER_SIZE) {
            // Remove current free block entirely from free list
            if (prev == 0) FreeListHead = next; else *((ulong *)prev + 1) = next;

            // Carve the allocated block at the beginning
            *(ulong *)current = size; // allocation header (size only)
            void *ret = ulong_to_ptr(current + ALLOC_HEADER_SIZE);

            // Set up tail bump region with the remainder (no free header yet)
            ulong consumed = ALLOC_HEADER_SIZE + size;
            if (blockSize > consumed) {
                TailPtr = current + consumed;
                TailSize = blockSize - consumed;
            } else {
                TailPtr = 0; TailSize = 0;
            }
            // If tail is too small to be useful for another alloc, push it back
            if (TailSize && TailSize < FREE_HEADER_SIZE + 8) {
                insert_free_sorted(ulong_to_ptr(TailPtr), TailSize);
                TailPtr = 0; TailSize = 0;
            }
            spin_unlock(&stelloc_lock);
            return ret;
        }
        prev = current;
        current = next;
    }

    // Next, try the tail bump region if available
    if (TailSize >= size + ALLOC_HEADER_SIZE) {
        ulong header = TailPtr;
        *(ulong *)header = size; // store size for free()
        void *ret = ulong_to_ptr(header + ALLOC_HEADER_SIZE);
        TailPtr += ALLOC_HEADER_SIZE + size;
        TailSize -= ALLOC_HEADER_SIZE + size;
        if (TailSize < FREE_HEADER_SIZE + 8) {
            if (TailSize >= 8) insert_free_sorted(ulong_to_ptr(TailPtr), TailSize);
            TailPtr = 0; TailSize = 0;
        }
    spin_unlock(&stelloc_lock);
    return ret;
    }

    // No block large enough: grow virtual heap and/or physical supply.
    // Commit at least one page worth (plus some headroom for metadata).
    uint64_t need = (uint64_t)(size + FREE_HEADER_SIZE);
    if (need < 4096) need = 4096;
    uint64_t va = vheap_commit((size_t)need);
    if (va) {
        // Insert the newly committed region as a free block and retry
        insert_free_sorted((void *)va, (ulong)need);
    } else {
        // Fallback: try to grow physical free list as before
        grow_heap(size + FREE_HEADER_SIZE);
    }
    prev = 0;
    current = FreeListHead;
    while (current != 0) {
        ulong blockSize = *(ulong *)current;
        ulong next = *((ulong *)current + 1);
        if (blockSize >= size + ALLOC_HEADER_SIZE) {
            if (prev == 0) FreeListHead = next; else *((ulong *)prev + 1) = next;
            *(ulong *)current = size;
            void *ret = ulong_to_ptr(current + ALLOC_HEADER_SIZE);
            ulong consumed = ALLOC_HEADER_SIZE + size;
            if (blockSize > consumed) {
                TailPtr = current + consumed;
                TailSize = blockSize - consumed;
            } else { TailPtr = 0; TailSize = 0; }
            if (TailSize && TailSize < FREE_HEADER_SIZE + 8) {
                insert_free_sorted(ulong_to_ptr(TailPtr), TailSize);
                TailPtr = 0; TailSize = 0;
            }
            spin_unlock(&stelloc_lock);
            return ret;
        }
        prev = current;
        current = next;
    }

    // Final attempt from tail after growth
    if (TailSize >= size + ALLOC_HEADER_SIZE) {
        ulong header = TailPtr;
        *(ulong *)header = size;
        void *ret = ulong_to_ptr(header + ALLOC_HEADER_SIZE);
        TailPtr += ALLOC_HEADER_SIZE + size;
        TailSize -= ALLOC_HEADER_SIZE + size;
        if (TailSize < FREE_HEADER_SIZE + 8) {
            if (TailSize >= 8) insert_free_sorted(ulong_to_ptr(TailPtr), TailSize);
            TailPtr = 0; TailSize = 0;
        }
        spin_unlock(&stelloc_lock);
        return ret;
    }

    spin_unlock(&stelloc_lock);
    return NULL;
}

void stelloc_free(void *ptr) {
    if (!ptr) return;
    spin_lock(&stelloc_lock);
    ulong block = ptr_to_ulong(ptr) - ALLOC_HEADER_SIZE;
    ulong size = *(ulong *)block;
    // Reconstruct full block size with header
    ulong full = size + ALLOC_HEADER_SIZE;

    // If this block is immediately before the current tail, "rewind" the bump.
    if (TailPtr != 0 && block + full == TailPtr) {
        TailPtr = block;
        TailSize += full;
        spin_unlock(&stelloc_lock);
        return;
    }

    // Otherwise insert back into the sorted free list and coalesce.
    insert_free_sorted(ulong_to_ptr(block), full);
    spin_unlock(&stelloc_lock);
}
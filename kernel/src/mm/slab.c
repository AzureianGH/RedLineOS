#include <slab.h>
#include <vheap.h>
#include <vmm.h>
#include <palloc.h>
#include <lock.h>

#define PAGE_SIZE 4096ULL
#define SLAB_MIN_ALIGN 8U

typedef struct slab_header {
    struct slab_header *next;
    uint16_t obj_size;
    uint16_t obj_per_slab;
    uint16_t free_count;
    uint16_t first_free_index; // head of free index list (intrusive in objects)
} slab_header_t;

typedef struct slab_cache {
    slab_header_t *partial;
    slab_header_t *full;
    uint16_t obj_size;
} slab_cache_t;

static slab_cache_t caches[] = {
    { .obj_size = 8 }, { .obj_size = 16 }, { .obj_size = 32 }, { .obj_size = 64 },
    { .obj_size = 128 }, { .obj_size = 256 }, { .obj_size = 512 }, { .obj_size = 1024 },
};

static inline size_t cache_count(void) { return sizeof(caches)/sizeof(caches[0]); }
static spinlock_t slab_lock;

static inline size_t align_up_sz(size_t x, size_t a) { return (x + (a-1)) & ~(a-1); }

static inline slab_cache_t *pick_cache(size_t size) {
    if (size == 0 || size > SLAB_MAX_SIZE) return NULL;
    for (size_t i = 0; i < cache_count(); i++) {
        if (size <= caches[i].obj_size) return &caches[i];
    }
    return &caches[cache_count()-1];
}

static inline uint64_t align_up(uint64_t x, uint64_t a) { return (x + (a-1)) & ~(a-1); }

static slab_header_t *new_slab(slab_cache_t *c) {
    // Allocate and map one page for a slab
    uint64_t va = vheap_commit(PAGE_SIZE);
    if (!va) return NULL;
    slab_header_t *sl = (slab_header_t *)va;
    // Layout: | slab_header | objects[...]
    // Align object region to at least the object size to guarantee alignment
    size_t hdr_align = c->obj_size;
    if (hdr_align < 16) hdr_align = 16; // keep a reasonable minimum for header alignment
    uint64_t hdr_sz = align_up(sizeof(slab_header_t), hdr_align);
    uint16_t count = (uint16_t)((PAGE_SIZE - hdr_sz) / c->obj_size);
    if (count == 0) return NULL;
    sl->next = NULL;
    sl->obj_size = (uint16_t)c->obj_size;
    sl->obj_per_slab = count;
    sl->free_count = count;
    sl->first_free_index = 0;

    // Initialize free index list intrusively inside objects
    uint8_t *base = (uint8_t *)sl + hdr_sz;
    for (uint16_t i = 0; i < count; i++) {
        uint16_t *slot = (uint16_t *)(base + (size_t)i * c->obj_size);
        *slot = (uint16_t)(i + 1); // next index; last will be count
    }
    return sl;
}

void slab_init(void) {
    spinlock_init(&slab_lock);
    for (size_t i = 0; i < cache_count(); i++) {
        caches[i].partial = NULL;
        caches[i].full = NULL;
    }
}

static inline bool in_slab(slab_header_t *sl, void *ptr) {
    uint8_t *begin = (uint8_t *)sl;
    uint8_t *end = begin + PAGE_SIZE;
    return (uint8_t *)ptr >= begin && (uint8_t *)ptr < end;
}

void *slab_alloc(size_t size) {
    size = align_up_sz(size, SLAB_MIN_ALIGN);
    slab_cache_t *c = pick_cache(size);
    if (!c) return NULL;
    spin_lock(&slab_lock);
    slab_header_t *sl = c->partial;
    if (!sl) {
        // Grab a new slab
        sl = new_slab(c);
        if (!sl) { spin_unlock(&slab_lock); return NULL; }
        sl->next = c->partial;
        c->partial = sl;
    }
    // Compute object base (align header to obj size like new_slab)
    size_t hdr_align = sl->obj_size < 16 ? 16 : sl->obj_size;
    uint64_t hdr_sz = align_up(sizeof(slab_header_t), hdr_align);
    uint8_t *base = (uint8_t *)sl + hdr_sz;

    // Pop from free list (index stored at object start)
    uint16_t idx = sl->first_free_index;
    uint8_t *obj = base + (size_t)idx * c->obj_size;
    sl->first_free_index = *(uint16_t *)obj;
    sl->free_count--;

    if (sl->free_count == 0) {
        // Move to full list
        c->partial = sl->next;
        sl->next = c->full;
        c->full = sl;
    }
    void *ret = (void *)obj;
    spin_unlock(&slab_lock);
    return ret;
}

void slab_free(void *ptr) {
    if (!ptr) return;
    spin_lock(&slab_lock);
    // Find which cache this pointer could belong to by scanning partial+full lists
    for (size_t k = 0; k < cache_count(); k++) {
        slab_cache_t *c = &caches[k];
        slab_header_t **lists[2] = { &c->partial, &c->full };
        for (int li = 0; li < 2; li++) {
            slab_header_t *prev = NULL;
            for (slab_header_t *sl = *lists[li]; sl != NULL; prev = sl, sl = sl->next) {
                if (!in_slab(sl, ptr)) continue;
                // Compute index and push back to free list
                size_t hdr_align = sl->obj_size < 16 ? 16 : sl->obj_size;
                uint64_t hdr_sz = align_up(sizeof(slab_header_t), hdr_align);
                uint8_t *base = (uint8_t *)sl + hdr_sz;
                size_t offset = (uint8_t *)ptr - base;
                if (offset % c->obj_size != 0) { spin_unlock(&slab_lock); return; } // invalid ptr
                uint16_t idx = (uint16_t)(offset / c->obj_size);
                *(uint16_t *)ptr = sl->first_free_index;
                sl->first_free_index = idx;
                sl->free_count++;

                // If was in full, move back to partial
                if (li == 1) {
                    if (prev) prev->next = sl->next; else *lists[li] = sl->next;
                    sl->next = c->partial; c->partial = sl;
                }
                // Optional: if slab becomes empty, we could release the page back to vheap/palloc.
                spin_unlock(&slab_lock);
                return;
            }
        }
    }
    spin_unlock(&slab_lock);
}

bool slab_owns(void *ptr) {
    if (!ptr) return false;
    spin_lock(&slab_lock);
    for (size_t k = 0; k < cache_count(); k++) {
        slab_cache_t *c = &caches[k];
        for (slab_header_t *sl = c->partial; sl; sl = sl->next) if (in_slab(sl, ptr)) { spin_unlock(&slab_lock); return true; }
        for (slab_header_t *sl = c->full; sl; sl = sl->next) if (in_slab(sl, ptr)) { spin_unlock(&slab_lock); return true; }
    }
    spin_unlock(&slab_lock);
    return false;
}

size_t slab_usable_size(void *ptr) {
    if (!ptr) return 0;
    size_t result = 0;
    spin_lock(&slab_lock);
    for (size_t k = 0; k < cache_count(); k++) {
        slab_cache_t *c = &caches[k];
        for (slab_header_t *sl = c->partial; sl; sl = sl->next) if (in_slab(sl, ptr)) { result = c->obj_size; goto out; }
        for (slab_header_t *sl = c->full; sl; sl = sl->next) if (in_slab(sl, ptr)) { result = c->obj_size; goto out; }
    }
out:
    spin_unlock(&slab_lock);
    return result;
}

#include <stdlib.h>
#include <stelloc.h>
#include <slab.h>

void *malloc(size_t size)
{
    if (size == 0) return NULL;
    if (size <= SLAB_MAX_SIZE) {
        void *p = slab_alloc(size);
        if (p) return p;
    }
    return stelloc_allocate((ulong)size);
}
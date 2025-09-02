#include <stdlib.h>
#include <stelloc.h>
#include <slab.h>

void free(void *ptr)
{
    if (!ptr) return;
    if (slab_owns(ptr)) { slab_free(ptr); return; }
    stelloc_free(ptr);
}
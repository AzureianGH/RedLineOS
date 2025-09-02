#include <stdlib.h>
#include <string.h>
#include <stelloc.h>

void *realloc(void *ptr, size_t size)
{
    if (ptr == NULL) {
        return malloc(size);
    }
    if (size == 0) {
        free(ptr);
        return NULL;
    }

    // Allocate new memory block
    void *new_ptr = malloc(size);
    if (new_ptr == NULL) {
        return NULL; // Allocation failed
    }

    memcpy(new_ptr, ptr, size);

    // Free the old block
    free(ptr);

    return new_ptr;
}
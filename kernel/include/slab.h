#pragma once

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

// Maximum size to serve from slab. Larger sizes go to stelloc.
#define SLAB_MAX_SIZE 1024U

void slab_init(void);
void *slab_alloc(size_t size);
void slab_free(void *ptr);

// Helper: returns true if ptr points to a slab-managed object
bool slab_owns(void *ptr);
// Helper: usable size of a slab allocation (its cache size)
size_t slab_usable_size(void *ptr);

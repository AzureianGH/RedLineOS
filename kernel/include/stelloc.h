#ifndef STELLOC_H
#define STELLOC_H

#include <extendedint.h>
#include <stddef.h>
#include <stdbool.h>

// Stelloc growth modes control how many physical pages are fetched from palloc
// when the allocator needs to grow its heap.
#define STELLOC_DUMB        1  // acquire 1 page on demand
#define STELLOC_SMART       2  // acquire a small batch (e.g., 4 pages)
#define STELLOC_AGGRESSIVE  3  // acquire a larger batch (e.g., 16 pages)

void stelloc_init_heap();
void *stelloc_allocate(ulong size);
void stelloc_free(void *ptr);

// Configure stelloc's growth behavior. Default is STELLOC_SMART.
void stelloc_set_mode(int mode);
int  stelloc_get_mode(void);

#endif /* STELLOC_H */
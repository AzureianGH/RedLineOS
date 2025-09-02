#pragma once

#include <extendedint.h>
#include <stddef.h>
#include <stdbool.h>
#include <stdint.h>

// Initialize a virtually contiguous heap at 'base_va' with size 'size_bytes'.
// Returns 0 on success.
int vheap_init(uint64_t base_va, uint64_t size_bytes);

// Ensure there is at least 'bytes' of committed (mapped) space from the current
// commit pointer by mapping new palloc pages as needed. Returns the start VA
// where the space is available, or 0 on failure. Moves the commit pointer.
uint64_t vheap_commit(size_t bytes);

// Query reserved virtual heap range.
void vheap_bounds(uint64_t* base_va, uint64_t* size_bytes);

// Map a single 4KiB page at 'va' if within the reserved heap range.
// Returns 0 on success, non-zero on failure.
int vheap_map_one(uint64_t va);

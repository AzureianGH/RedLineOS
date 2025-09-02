#pragma once

#include <extendedint.h>
#include <stddef.h>
#include <stdbool.h>
#include <stdint.h>

// Page table flags
#define VMM_P_PRESENT   (1ULL << 0)
#define VMM_P_WRITABLE  (1ULL << 1)
#define VMM_P_USER      (1ULL << 2)
#define VMM_P_NX        (1ULL << 63)

// Initialize VMM (grabs current PML4 via CR3 and translates through HHDM)
void vmm_init(void);

// Map a single 4KiB page at virtual address 'va' to physical 'pa' with flags.
// Returns 0 on success, non-zero on failure.
int vmm_map_page(uint64_t va, uint64_t pa, uint64_t flags);

#pragma once

#include <stdint.h>
#include <stddef.h>

// Map a physical MMIO range [phys, phys+size) into kernel virtual space and
// return the mapped virtual base. Returns 0 on failure.
void* ioremap(uint64_t phys, size_t size);

// Optional unmap (not strictly needed yet). Safe to no-op if not implemented.
void iounmap(void* virt, size_t size);

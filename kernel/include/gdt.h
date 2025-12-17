#pragma once

#include <stdint.h>

// Segment selectors
#define GDT_SELECTOR_NULL      0x00
#define GDT_SELECTOR_KERNEL_CS 0x08
#define GDT_SELECTOR_KERNEL_DS 0x10
#define GDT_SELECTOR_USER_DS   (0x18 | 0x3)
#define GDT_SELECTOR_USER_CS   (0x20 | 0x3)
#define GDT_SELECTOR_TSS       0x28

// Initialize and load a 64-bit GDT/TSS for the given CPU index.
// cpu_index should be a small, unique 0-based index per CPU (e.g., BSP=0, APs as enumerated).
void gdt_init(uint32_t cpu_index);

// Optionally update the TSS RSP0 to a new kernel stack top.
void gdt_set_kernel_rsp0(uint64_t rsp0);

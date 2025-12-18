#pragma once

#include <stdint.h>
#include <stdbool.h>

// Initialize SMP using Limine MP response; bring APs online.
void smp_init(uint64_t tsc_hz_hint);

// Number of logical CPUs detected (includes BSP).
uint32_t smp_cpu_count(void);

// Blocking wait until all APs have reported online (bounded by init sequence).
void smp_wait_all_aps(void);

// Broadcast a halt IPI to all other CPUs (used during panic paths).
void smp_halt_others(void);

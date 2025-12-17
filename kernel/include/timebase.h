#pragma once

#include <stdint.h>
#include <stdbool.h>

// Initialize high-resolution timebase using HPET or TSC.
void timebase_init(uint64_t tsc_hz_hint);

// Monotonic time in nanoseconds (best available source: HPET > TSC > coarse ticks).
uint64_t timebase_monotonic_ns(void);

// Busy-wait sleeps; prefer small durations. For long waits, combine with ktime events.
void timebase_sleep_ns(uint64_t ns);
static inline void timebase_sleep_ms(uint64_t ms) { timebase_sleep_ns(ms * 1000000ULL); }

// Query which source is active.
bool timebase_uses_hpet(void);
bool timebase_uses_tsc(void);

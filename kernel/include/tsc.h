#pragma once
#include <stdint.h>

static inline uint64_t rdtsc(void) {
    uint32_t lo, hi; __asm__ __volatile__("rdtsc" : "=a"(lo), "=d"(hi)); return ((uint64_t)hi<<32)|lo;
}

uint64_t tsc_calibrate_hz(uint32_t pit_hz, uint32_t ms);

#pragma once
#include <stdint.h>
#include <stdbool.h>

// Vector used for LAPIC timer interrupts; ensure an IDT entry exists
#define LAPIC_TIMER_VECTOR 0xF0

typedef void (*lapic_timer_cb_t)(void);

bool lapic_supported(void);
void lapic_enable(void);
void lapic_eoi(void);

// Timer API (periodic)
void lapic_timer_init(uint32_t hz, uint64_t tsc_hz_hint);
int  lapic_timer_on_tick(lapic_timer_cb_t cb);
bool lapic_timer_active(void);

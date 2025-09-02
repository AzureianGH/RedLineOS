#pragma once
#include <stdint.h>
#include <stdbool.h>

typedef void (*timer_cb_t)(void);

typedef enum { TIMER_SRC_LAPIC, TIMER_SRC_HPET, TIMER_SRC_PIT } timer_source_t;

bool timer_init(uint32_t hz_hint, uint64_t tsc_hz);
timer_source_t timer_source(void);
int  timer_on_tick(timer_cb_t cb);

#pragma once
#include <stdint.h>
#include <stdbool.h>
#include <ktime.h>

static inline void     ktimer_init(uint32_t hz_hint, uint64_t tsc_hz) { ktime_init(hz_hint, tsc_hz); }
static inline uint64_t ktimer_millis(void) { return ktime_millis(); }

static inline int ktimer_after(uint64_t delay_ms, ktime_cb_t cb, void* user) {
    ktime_event_t ev = { .expires_ms = delay_ms, .period_ms = 0, .cb = cb, .user = user, .active = false };
    return ktime_add_event(&ev);
}

static inline int ktimer_every(uint64_t period_ms, ktime_cb_t cb, void* user) {
    ktime_event_t ev = { .expires_ms = period_ms, .period_ms = period_ms, .cb = cb, .user = user, .active = false };
    return ktime_add_event(&ev);
}

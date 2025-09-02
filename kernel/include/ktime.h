#pragma once
#include <stdint.h>
#include <stdbool.h>

// Generic kernel timer API for modules/drivers
// - One-shot and periodic callbacks
// - Callbacks run in interrupt context; keep them short
// - Thread-safe for multicore with simple spinlock

typedef void (*ktime_cb_t)(void* user);

typedef struct {
    uint64_t    expires_ms;
    uint64_t    period_ms;     // 0 for one-shot
    ktime_cb_t  cb;
    void*       user;
    bool        active;
} ktime_event_t;

void     ktime_init(uint32_t tick_hz_hint, uint64_t tsc_hz);
uint64_t ktime_millis(void);            // monotonic ms
int      ktime_add_event(ktime_event_t* ev); // arm event (updates expires from now)
void     ktime_cancel(ktime_event_t* ev);


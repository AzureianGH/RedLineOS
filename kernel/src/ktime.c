#include <ktime.h>
#include <timer.h>
#include <lprintf.h>
#include <stddef.h>
#include <stdatomic.h>
#include <spinlock.h>

// Simple global tick counter (ms) serviced by the system tick
static _Atomic uint64_t g_ms;
static spinlock_t qlock;
#define MAX_EVENTS 64
static ktime_event_t* q[MAX_EVENTS];

static void tick_cb(void) {
    atomic_fetch_add_explicit(&g_ms, 1, memory_order_relaxed);
    // Service timer queue
    uint64_t now = atomic_load_explicit(&g_ms, memory_order_relaxed);
    // Fast path: try to take lock; if contended, skip this tick
    if (!spin_trylock(&qlock)) return;
    for (int i = 0; i < MAX_EVENTS; ++i) {
        ktime_event_t* ev = q[i];
        if (!ev || !ev->active) continue;
        if (now >= ev->expires_ms) {
            ktime_cb_t cb = ev->cb;
            void* user = ev->user;
            if (ev->period_ms) {
                ev->expires_ms = now + ev->period_ms;
            } else {
                ev->active = false;
                q[i] = NULL;
            }
            // Call outside of list mutation to reduce time under lock
            spin_unlock(&qlock);
            cb(user);
            if (!spin_trylock(&qlock)) return; // avoid long lock attempts
        }
    }
    spin_unlock(&qlock);
}

void ktime_init(uint32_t tick_hz_hint, uint64_t tsc_hz) {
    // Ensure system timer is initialized; this is a thin wrapper over timer_init
    if (!timer_init(tick_hz_hint ? tick_hz_hint : 1000, tsc_hz)) {
        error_printf("ktime: timer_init failed\n");
        return;
    }
    spinlock_init(&qlock);
    if (timer_on_tick(tick_cb) != 0) {
        // On HPET-only (no IRQ tick), this will fail; we still track ms via other sources later
        info_printf("ktime: no IRQ tick source; ms counter won't advance\n");
    }
}

uint64_t ktime_millis(void) {
    return atomic_load_explicit(&g_ms, memory_order_relaxed);
}

// Minimal event wheel: single-threaded check in tick callback would be ideal, but
// keep it simple for now: drivers can poll ktime_millis for timeouts.
int ktime_add_event(ktime_event_t* ev) {
    if (!ev || !ev->cb) return -1;
    uint64_t now = ktime_millis();
    ev->expires_ms = now + (ev->expires_ms ? ev->expires_ms : ev->period_ms);
    ev->active = true;
    spin_lock(&qlock);
    for (int i = 0; i < MAX_EVENTS; ++i) {
        if (!q[i]) { q[i] = ev; spin_unlock(&qlock); return 0; }
    }
    spin_unlock(&qlock);
    return -1;
}

void ktime_cancel(ktime_event_t* ev) {
    if (!ev) return;
    spin_lock(&qlock);
    for (int i = 0; i < MAX_EVENTS; ++i) if (q[i] == ev) { q[i] = NULL; break; }
    ev->active = false;
    spin_unlock(&qlock);
}

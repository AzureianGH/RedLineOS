#include <timer.h>
#include <lapic.h>
#include <hpet.h>
#include <pit.h>
#include <lprintf.h>

static timer_source_t g_src;

bool timer_init(uint32_t hz_hint, uint64_t tsc_hz) {
    // Try LAPIC first
    if (lapic_supported()) {
        info_printf("Timer: LAPIC supported\n");
        lapic_enable();
        lapic_timer_init(hz_hint ? hz_hint : 1000, tsc_hz);
        if (lapic_timer_active()) { g_src = TIMER_SRC_LAPIC; info_printf("Timer: using LAPIC\n"); return true; }
    }
    // Then HPET in periodic mode (no IRQ routing yet, use it as a sleeper for now)
    if (hpet_supported()) {
        info_printf("Timer: HPET supported\n");
        hpet_init();
        g_src = TIMER_SRC_HPET; // Tick callbacks unsupported yet
        info_printf("Timer: using HPET (sleep only)\n");
        return true;
    }
    // Fallback: PIT
    info_printf("Timer: falling back to PIT\n");
    pit_init(hz_hint ? hz_hint : 1000);
    g_src = TIMER_SRC_PIT;
    return true;
}

timer_source_t timer_source(void) { return g_src; }

int timer_on_tick(timer_cb_t cb) {
    switch (g_src) {
        case TIMER_SRC_LAPIC: return lapic_timer_on_tick(cb);
        case TIMER_SRC_PIT:   return pit_on_tick(cb);
        case TIMER_SRC_HPET:  return -1; // not wired to IRQ yet
    }
    return -1;
}

#include <timer.h>
#include <lapic.h>
#include <hpet.h>
#include <ioapic.h>
#include <pit.h>
#include <lprintf.h>

// General tick
static clock_t g_ticks = 0;
static uint32_t g_hz = 1000;

static void general_tick_cb(void) {
    ++g_ticks;
}

static timer_source_t g_src;

bool timer_init(uint32_t hz_hint, uint64_t tsc_hz) {
    g_hz = (hz_hint ? hz_hint : 1000);
    // Try LAPIC first
    if (lapic_supported()) {
        info_printf("Timer: LAPIC supported\n");
        lapic_enable();
    lapic_timer_init(g_hz, tsc_hz);
        if (lapic_timer_active()) {
            g_src = TIMER_SRC_LAPIC; 
            info_printf("Timer: using LAPIC\n");
            lapic_timer_on_tick(general_tick_cb);
            return true; 
        }
    }
    // Then HPET in periodic mode with IRQ via IOAPIC if possible
    if (hpet_supported()) {
        info_printf("Timer: HPET supported\n");
        hpet_init();
        if (ioapic_supported()) {
            uint64_t ns = (uint64_t)1000000000ULL / g_hz;
            if (hpet_enable_and_route_irq(0, ns, 242)) {
                g_src = TIMER_SRC_HPET;
                info_printf("Timer: using HPET (periodic IRQ)\n");
                hpet_on_tick(general_tick_cb);
                return true;
            } else {
                info_printf("Timer: HPET IRQ routing failed; falling back\n");
            }
        } else {
            info_printf("Timer: IOAPIC not present; HPET IRQ unavailable\n");
        }
    }
    // Fallback: PIT
    info_printf("Timer: falling back to PIT\n");
    pit_init(g_hz);
    g_src = TIMER_SRC_PIT;
    pit_on_tick(general_tick_cb);
    return true;
}

timer_source_t timer_source(void) { return g_src; }

int timer_on_tick(timer_cb_t cb) {
    switch (g_src) {
        case TIMER_SRC_LAPIC: return lapic_timer_on_tick(cb);
        case TIMER_SRC_PIT:   return pit_on_tick(cb);
        case TIMER_SRC_HPET:  return hpet_on_tick(cb);
    }
    return -1;
}

clock_t timer_get_ticks(void)
{
    return g_ticks;
}

uint32_t timer_hz(void) { return g_hz; }

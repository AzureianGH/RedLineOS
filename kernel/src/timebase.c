#include <timebase.h>
#include <hpet.h>
#include <tsc.h>
#include <ktime.h>
#include <lprintf.h>

static uint64_t g_hpet_hz;
static uint64_t g_tsc_hz;
static bool g_use_hpet;
static bool g_use_tsc;

// Simple 128/64 division without libgcc runtime (__udivti3).
static uint64_t udiv128_64(uint64_t hi, uint64_t lo, uint64_t d) {
    if (d == 0) return 0;
    uint64_t rem_hi = hi;
    uint64_t rem_lo = lo;
    uint64_t q = 0;
    for (int i = 0; i < 64; ++i) {
        rem_hi = (rem_hi << 1) | (rem_lo >> 63);
        rem_lo <<= 1;
        q <<= 1;
        if (rem_hi || rem_lo >= d) {
            uint64_t borrow = (rem_lo < d);
            rem_lo -= d;
            rem_hi -= borrow;
            q |= 1;
        }
    }
    return q;
}

static uint64_t mul_div_u64(uint64_t a, uint64_t b, uint64_t d) {
    __uint128_t num = (__uint128_t)a * b;
    return udiv128_64((uint64_t)(num >> 64), (uint64_t)num, d);
}

void timebase_init(uint64_t tsc_hz_hint) {
    g_use_hpet = false;
    g_use_tsc = false;
    g_hpet_hz = 0;
    g_tsc_hz = tsc_hz_hint;

    if (hpet_supported()) {
        hpet_init();
        g_hpet_hz = hpet_counter_hz();
        if (g_hpet_hz) {
            g_use_hpet = true;
            info_printf("timebase: using HPET (%llu Hz)\n", (unsigned long long)g_hpet_hz);
            return;
        }
    }
    if (g_tsc_hz == 0) {
        g_tsc_hz = tsc_calibrate_hz(1193182u, 10);
    }
    if (g_tsc_hz) {
        g_use_tsc = true;
        info_printf("timebase: using TSC (%llu Hz)\n", (unsigned long long)g_tsc_hz);
    } else {
        info_printf("timebase: falling back to coarse tick\n");
    }
}

static uint64_t ns_from_ticks(uint64_t ticks, uint64_t hz) {
    if (!hz) return 0;
    return mul_div_u64(ticks, 1000000000ULL, hz);
}

uint64_t timebase_monotonic_ns(void) {
    if (g_use_hpet && g_hpet_hz) {
        uint64_t c = hpet_counter();
        return ns_from_ticks(c, g_hpet_hz);
    }
    if (g_use_tsc && g_tsc_hz) {
        uint64_t c = rdtsc();
        return ns_from_ticks(c, g_tsc_hz);
    }
    // Coarse fallback: ktime milliseconds
    return ktime_millis() * 1000000ULL;
}

void timebase_sleep_ns(uint64_t ns) {
    if (ns == 0) return;
    if (g_use_hpet && g_hpet_hz) {
        uint64_t start = hpet_counter();
        uint64_t delta_ticks = ns_from_ticks(1, g_hpet_hz); // inverse for guard; reuse below
        (void)delta_ticks;
        uint64_t ticks = mul_div_u64((uint64_t)ns, g_hpet_hz, 1000000000ULL);
        uint64_t target = start + ticks;
        while (hpet_counter() < target) { __asm__ __volatile__("pause"); }
        return;
    }
    if (g_use_tsc && g_tsc_hz) {
        uint64_t start = rdtsc();
        uint64_t cycles = mul_div_u64((uint64_t)ns, g_tsc_hz, 1000000000ULL);
        uint64_t target = start + cycles;
        while (rdtsc() < target) { __asm__ __volatile__("pause"); }
        return;
    }
    // Coarse: spin on ms ticks
    uint64_t start_ms = ktime_millis();
    uint64_t wait_ms = (ns + 999999ULL) / 1000000ULL;
    while (ktime_millis() - start_ms < wait_ms) { __asm__ __volatile__("pause"); }
}

bool timebase_uses_hpet(void) { return g_use_hpet; }
bool timebase_uses_tsc(void) { return g_use_tsc; }

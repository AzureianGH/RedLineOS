#include <lapic.h>
#include <acpi.h>
#include <limine.h>
#include <ioremap.h>
#include <isr.h>
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <lprintf.h>
#include <tsc.h>
#include <hpet.h>
#include <sched.h>

extern volatile struct limine_hhdm_request hhdm_request;

static volatile uint32_t* lapic_base;
static lapic_timer_cb_t timer_cbs[256]; // max 256 callbacks (one per vector)
static bool timer_on;

static inline volatile uint32_t* lapic_reg(uint32_t off) {
    return (volatile uint32_t*)((uintptr_t)lapic_base + off);
}

static inline void lapic_write(uint32_t off, uint32_t val) { *lapic_reg(off) = val; (void)*lapic_reg(off); }
static inline uint32_t lapic_read(uint32_t off) { return *lapic_reg(off); }

#define LAPIC_REG_ID          0x020
#define LAPIC_REG_EOI         0x0B0
#define LAPIC_REG_TPR         0x080
#define LAPIC_REG_SVR         0x0F0
#define LAPIC_REG_LVT_TIMER   0x320
#define LAPIC_REG_TIMER_INIT  0x380
#define LAPIC_REG_TIMER_CURR  0x390
#define LAPIC_REG_TIMER_DIV   0x3E0

#define SVR_ENABLE            0x100

#define LVT_TIMER_MODE_ONE_SHOT 0x00000
#define LVT_TIMER_MODE_PERIODIC 0x20000

static void lapic_timer_isr(isr_frame_t* f) {
    (void)f;
    for (int i = 0; i < 256; ++i) if (timer_cbs[i]) timer_cbs[i]();
    // Preemptive scheduling hook
    sched_on_timer_tick(f);
    lapic_write(LAPIC_REG_EOI, 0);
}

bool lapic_supported(void) {
    uint64_t phys = acpi_lapic_phys();
    if (!phys) return false;
    // Map MMIO with ioremap to ensure page tables cover the region
    lapic_base = (volatile uint32_t*)ioremap(phys, 0x1000);
    if (!lapic_base) { error_printf("LAPIC: ioremap failed for phys=%#016llx\n", (unsigned long long)phys); return false; }
    debug_printf("LAPIC: phys=%#016llx -> base=%p (ioremap)\n", (unsigned long long)phys, lapic_base);
    // Light sanity: read ID or SVR
    uint32_t svr = lapic_read(LAPIC_REG_SVR);
    debug_printf("LAPIC: SVR=%#08x\n", svr);
    return true;
}

static inline uint64_t rdmsr(uint32_t msr) {
    uint32_t lo, hi;
    __asm__ volatile ("rdmsr" : "=a"(lo), "=d"(hi) : "c"(msr));
    return ((uint64_t)hi << 32) | lo;
}

static inline void wrmsr(uint32_t msr, uint64_t val) {
    uint32_t lo = (uint32_t)val;
    uint32_t hi = (uint32_t)(val >> 32);
    __asm__ volatile ("wrmsr" :: "c"(msr), "a"(lo), "d"(hi));
}

// IA32_APIC_BASE MSR (0x1B)
#define MSR_IA32_APIC_BASE  0x1B
#define APIC_BASE_ENABLE    (1ULL << 11)

void lapic_enable(void) {
    // Ensure xAPIC is enabled in MSR
    uint64_t apic_base = rdmsr(MSR_IA32_APIC_BASE);
    if (!(apic_base & APIC_BASE_ENABLE)) {
        debug_printf("LAPIC: enabling xAPIC in MSR, base MSR=%#016llx\n", (unsigned long long)apic_base);
        apic_base |= APIC_BASE_ENABLE;
        wrmsr(MSR_IA32_APIC_BASE, apic_base);
    } else {
        debug_printf("LAPIC: xAPIC already enabled, MSR=%#016llx\n", (unsigned long long)apic_base);
    }
    uint32_t svr = lapic_read(LAPIC_REG_SVR);
    debug_printf("LAPIC: enabling, SVR old=%#08x new=%#08x\n", svr, (svr | SVR_ENABLE | 0xFF));
    lapic_write(LAPIC_REG_SVR, svr | SVR_ENABLE | 0xFF); // spurious vector 0xFF
    // Ensure Task Priority Register allows all interrupts
    lapic_write(LAPIC_REG_TPR, 0x00);
}

void lapic_eoi(void) { lapic_write(LAPIC_REG_EOI, 0); }

void lapic_timer_init(uint32_t hz, uint64_t tsc_hz_hint) {
    if (!lapic_base || hz == 0) { timer_on = false; return; }
    // Register ISR vector
    extern void isr_stub_240(void); // we'll add stub for 0xF0
    isr_register(LAPIC_TIMER_VECTOR, lapic_timer_isr);
    // Divide by 16
    lapic_write(LAPIC_REG_TIMER_DIV, 0x3);
    debug_printf("LAPIC: timer div=16, target hz=%u\n", (unsigned)hz);

    // Calibrate APIC timer frequency using HPET if available (stable), else TSC over ~10ms window
    uint64_t apic_hz = 0;
    bool used_hpet = false;
    if (hpet_supported()) {
        hpet_init(); // ensure main counter is running
        uint64_t hpet_hz = hpet_counter_hz();
        if (hpet_hz) {
            // One-shot with max initial
            lapic_write(LAPIC_REG_LVT_TIMER, LAPIC_TIMER_VECTOR | LVT_TIMER_MODE_ONE_SHOT);
            lapic_write(LAPIC_REG_TIMER_INIT, 0xFFFFFFFFu);
            // Measure APIC elapsed over ~10ms using HPET as time base (sleep)
            uint64_t interval_ns = 10ULL * 1000ULL * 1000ULL; // 10ms
            hpet_sleep_ns(interval_ns);
            uint32_t curr = lapic_read(LAPIC_REG_TIMER_CURR);
            uint32_t elapsed = 0xFFFFFFFFu - curr;
            // apic_hz = elapsed ticks / 0.01s
            apic_hz = (elapsed * 1000000000ULL) / interval_ns;
            used_hpet = (apic_hz != 0);
        }
    }
    if (!used_hpet) {
        uint64_t tsc_hz = tsc_hz_hint;
        if (!tsc_hz) {
            tsc_hz = tsc_calibrate_hz(1193182u, 10);
        }
        // One-shot with max initial
        lapic_write(LAPIC_REG_LVT_TIMER, LAPIC_TIMER_VECTOR | LVT_TIMER_MODE_ONE_SHOT);
        lapic_write(LAPIC_REG_TIMER_INIT, 0xFFFFFFFFu);
        uint64_t t0 = rdtsc();
        uint64_t wait_cycles = tsc_hz / 100; // ~10ms
        while (rdtsc() - t0 < wait_cycles) { __asm__ __volatile__("pause"); }
        uint32_t curr = lapic_read(LAPIC_REG_TIMER_CURR);
        uint64_t t1 = rdtsc();
        uint32_t elapsed = 0xFFFFFFFFu - curr;
        uint64_t tsc_delta = t1 - t0;
        // apic_ticks_per_sec = elapsed / (tsc_delta / tsc_hz)
        apic_hz = (tsc_delta ? (elapsed * tsc_hz) / tsc_delta : 0);
    }
    if (apic_hz == 0) {
        // Fallback guess: common LAPIC clock ~ 100MHz / 16 div
        apic_hz = 100000000ULL / 16ULL;
    }
    uint32_t initial = (uint32_t)(apic_hz / (hz ? hz : 1000u));
    if (initial == 0) initial = 1;
    debug_printf("LAPIC: calib %s apic_hz~%llu, initial=%u vector=%u\n",
                 used_hpet ? "HPET" : "TSC",
                 (unsigned long long)apic_hz, (unsigned)initial, (unsigned)LAPIC_TIMER_VECTOR);
    // Program periodic mode
    lapic_write(LAPIC_REG_LVT_TIMER, LAPIC_TIMER_VECTOR | LVT_TIMER_MODE_PERIODIC);
    lapic_write(LAPIC_REG_TIMER_INIT, initial);
    timer_on = true;
}

int lapic_timer_on_tick(lapic_timer_cb_t cb) {
    for (int i = 0; i < 256; ++i) if (!timer_cbs[i]) { timer_cbs[i]=cb; return 0; }
    return -1;
}

bool lapic_timer_active(void) { return timer_on; }

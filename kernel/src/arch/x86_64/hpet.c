#include <hpet.h>
#include <acpi.h>
#include <limine.h>
#include <ioremap.h>
#include <stdint.h>
#include <stdbool.h>
#include <lprintf.h>
#include <isr.h>
#include <ioapic.h>
#include <lapic.h>

extern volatile struct limine_hhdm_request hhdm_request;

static volatile uint64_t* hpet_regs;
static uint64_t period_fs;
static uint8_t hpet_irq_vector = 242;
static uint8_t hpet_irq_timer_index = 0;
static uint64_t hpet_irq_count = 0;
static hpet_irq_cb_t hpet_cbs[256];
static bool hpet_irq_routed = false;

static inline uint64_t mmio_read64(volatile uint64_t* p) { return *p; }
static inline void mmio_write64(volatile uint64_t* p, uint64_t v) { *p = v; (void)*p; }

bool hpet_supported(void) {
    const acpi_hpet_t* t = acpi_hpet();
    if (!t) return false;
    if (t->address.address_space_id != 0) return false; // must be MMIO
    hpet_regs = (volatile uint64_t*)ioremap(t->address.address, 0x400);
    if (!hpet_regs) { error_printf("HPET: ioremap failed for phys=%#016llx\n", (unsigned long long)t->address.address); return false; }
    debug_printf("HPET: phys=%#016llx -> base=%p\n", (unsigned long long)t->address.address, hpet_regs);
    uint64_t caps = mmio_read64(hpet_regs + 0); // GCAP_ID
    period_fs = (caps >> 32);
    debug_printf("HPET: GCAP_ID=%#016llx period_fs=%llu\n", (unsigned long long)caps, (unsigned long long)period_fs);
    return period_fs != 0;
}

void hpet_init(void) {
    if (!hpet_regs) return;
    // Disable
    uint64_t conf = mmio_read64(hpet_regs + 2);
    conf &= ~1ULL;
    mmio_write64(hpet_regs + 2, conf);
    debug_printf("HPET: disabled, conf=%#016llx\n", (unsigned long long)conf);
    // Enable legacy replacement route off by default
    // Reset main counter (MAIN_CNT at 0xF0 -> index 0x1E)
    mmio_write64(hpet_regs + 0x1E, 0);
    // Enable
    conf |= 1ULL;
    mmio_write64(hpet_regs + 2, conf);
    debug_printf("HPET: enabled, conf=%#016llx\n", (unsigned long long)conf);
    // Sanity: ensure main counter advances
    uint64_t c0 = mmio_read64(hpet_regs + 0x1E);
    for (volatile int i = 0; i < 10000; ++i) __asm__ __volatile__("pause");
    uint64_t c1 = mmio_read64(hpet_regs + 0x1E);
    debug_printf("HPET: counter %llu -> %llu (+%llu)\n",
                 (unsigned long long)c0, (unsigned long long)c1, (unsigned long long)(c1 - c0));
}

uint64_t hpet_counter_hz(void) {
    if (!period_fs) return 0;
    // 1e15 fs per second / period_fs
    return (uint64_t)(1000000000000000ULL / period_fs);
}

void hpet_sleep_ns(uint64_t ns) {
    if (!hpet_regs || period_fs == 0) return;
    uint64_t start = mmio_read64(hpet_regs + 0x1E);
    uint64_t ticks = (ns * 1000000ULL) / period_fs; // ns to ticks
    debug_printf("HPET: sleep %lluns -> %lluticks start=%llu\n", (unsigned long long)ns, (unsigned long long)ticks, (unsigned long long)start);
    while (mmio_read64(hpet_regs + 0x1E) - start < ticks) {
        __asm__ __volatile__("pause");
    }
}

void hpet_enable_periodic_irq(uint8_t comparator, uint64_t ns_interval, uint8_t vector) {
    hpet_irq_vector = vector;
    hpet_irq_timer_index = comparator;
    if (!hpet_regs || period_fs == 0) return;
    // Disable
    uint64_t conf = mmio_read64(hpet_regs + 2);
    conf &= ~1ULL;
    mmio_write64(hpet_regs + 2, conf);

    // Configure comparator N
    volatile uint64_t* tn_cfg = hpet_regs + (0x100/8) + comparator*0x20/8;
    volatile uint64_t* tn_cmp = hpet_regs + (0x108/8) + comparator*0x20/8;
    // Clear any pending interrupt for this timer
    mmio_write64(hpet_regs + (0x20/8), (1ull << comparator));
    uint64_t cfg = mmio_read64(tn_cfg);
    // TYPE_CNF (bit 3) = periodic, INT_TYPE_CNF (bit 1) = edge, INT_ENB_CNF (bit 2) stays 0 for now
    cfg |= (1ull << 3);   // periodic mode
    cfg &= ~(1ull << 1);  // edge triggered
    cfg &= ~(1ull << 2);  // leave interrupts disabled until routed
    mmio_write64(tn_cfg, cfg);

    uint64_t ticks = (ns_interval * 1000000ULL) / period_fs;
    // Program periodic comparator with VAL_SET (bit 6) sequence
    mmio_write64(tn_cmp, ticks);
    mmio_write64(tn_cfg, cfg | (1ull << 6)); // VAL_SET_CNF
    mmio_write64(tn_cmp, ticks);
    debug_printf("HPET: comparator%u periodic ticks=%llu cfg=%#016llx\n", (unsigned)comparator, (unsigned long long)ticks, (unsigned long long)cfg);

    // Reset main counter and enable
    mmio_write64(hpet_regs + 0x1E, 0);
    conf |= 1ULL;
    mmio_write64(hpet_regs + 2, conf);
}

static void hpet_isr(isr_frame_t* f) {
    (void)f;
    // Acknowledge by writing to General Interrupt Status for our timer bit
    mmio_write64(hpet_regs + (0x20/8), (1ull << hpet_irq_timer_index));
    // Invoke registered callbacks
    for (int i = 0; i < 256; ++i) if (hpet_cbs[i]) hpet_cbs[i]();
    ++hpet_irq_count;
    lapic_eoi();
}

// Wire HPET comparator 0 through IOAPIC to LAPIC vector
bool hpet_route_irq_via_ioapic(uint8_t gsi, uint8_t vector) {
    if (hpet_irq_routed) return true;
    if (!ioapic_supported()) return false;
    // Program HPET comparator route field to selected GSI (common implementations support 0..31)
    volatile uint64_t* tn_cfg = hpet_regs + (0x100/8) + hpet_irq_timer_index*0x20/8;
    uint64_t cfg = mmio_read64(tn_cfg);
    cfg &= ~(0x1FULL << 9);               // clear INT_ROUTE_CNF field (5 bits typical)
    cfg |= ((uint64_t)(gsi & 0x1F) << 9); // set route to desired GSI
    mmio_write64(tn_cfg, cfg);
    // Mask, route, unmask
    ioapic_mask_irq(gsi);
    ioapic_route_irq(gsi, vector);
    isr_register(vector, hpet_isr);
    ioapic_unmask_irq(gsi);
    debug_printf("HPET: routed GSI %u to vector %u\n", (unsigned)gsi, (unsigned)vector);
    return true;
}

int hpet_on_tick(hpet_irq_cb_t cb) {
    for (int i = 0; i < 256; ++i) {
        if (!hpet_cbs[i]) { hpet_cbs[i] = cb; return 0; }
    }
    return -1;
}

bool hpet_enable_and_route_irq(uint8_t comparator, uint64_t ns_interval, uint8_t vector) {
    if (hpet_irq_routed) return true;
    // Program HPET periodic comparator and enable INT
    hpet_enable_periodic_irq(comparator, ns_interval, vector);
    // Choose a valid GSI from Timer N route capability mask (bits 32..63)
    volatile uint64_t* tn_cfg = hpet_regs + (0x100/8) + comparator*0x20/8;
    uint64_t cfg = mmio_read64(tn_cfg);
    uint32_t route_cap = (uint32_t)(cfg >> 32);
    if (route_cap == 0) {
        error_printf("HPET: no INT_ROUTE_CAP bits set; cannot route IRQ\n");
        return false;
    }
    uint32_t gsi_base = ioapic_get_gsi_base();
    // route_cap bits are IOAPIC input pin numbers; absolute GSI = base + pin
    // Prefer pins that map to non-ISA GSIs or not 0,1,2,8.
    // Pass 1: pins yielding GSI >= 16
    for (uint8_t pin = 0; pin < 32; ++pin) {
        if (route_cap & (1u << pin)) {
            uint32_t abs_gsi = gsi_base + pin;
            if (abs_gsi < 16) continue;
            if (hpet_route_irq_via_ioapic((uint8_t)abs_gsi, vector)) {
                // Now enable interrupts on the comparator
                volatile uint64_t* tn_cfg2 = hpet_regs + (0x100/8) + comparator*0x20/8;
                uint64_t cfg2 = mmio_read64(tn_cfg2);
                cfg2 |= (1ull << 2); // INT_ENB_CNF
                mmio_write64(tn_cfg2, cfg2);
                // Clear pending one more time to avoid spurious
                mmio_write64(hpet_regs + (0x20/8), (1ull << comparator));
                hpet_irq_routed = true;
                debug_printf("HPET: enable+route pin %u -> GSI %u OK (cap=%#08x base=%u)\n", pin, abs_gsi, route_cap, gsi_base);
                return true;
            }
        }
    }
    // Pass 2: pins yielding ISA GSIs but avoid 0,1,2,8
    for (uint8_t pin = 0; pin < 16; ++pin) {
        if (route_cap & (1u << pin)) {
            uint32_t abs_gsi = gsi_base + pin;
            if (abs_gsi == 0 || abs_gsi == 1 || abs_gsi == 2 || abs_gsi == 8) continue;
            if (hpet_route_irq_via_ioapic((uint8_t)abs_gsi, vector)) {
                // Now enable interrupts on the comparator
                volatile uint64_t* tn_cfg2 = hpet_regs + (0x100/8) + comparator*0x20/8;
                uint64_t cfg2 = mmio_read64(tn_cfg2);
                cfg2 |= (1ull << 2); // INT_ENB_CNF
                mmio_write64(tn_cfg2, cfg2);
                // Clear pending one more time to avoid spurious
                mmio_write64(hpet_regs + (0x20/8), (1ull << comparator));
                hpet_irq_routed = true;
                debug_printf("HPET: enable+route pin %u -> GSI %u OK (cap=%#08x base=%u)\n", pin, abs_gsi, route_cap, gsi_base);
                return true;
            }
        }
    }
    info_printf("HPET: skipping IRQ routing (only colliding GSIs available: cap=%#08x base=%u)\n", route_cap, gsi_base);
    return false;
}

uint64_t hpet_get_irq_count(void) { return hpet_irq_count; }

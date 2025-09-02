#include <ioapic.h>
#include <acpi.h>
#include <limine.h>
#include <ioremap.h>
#include <stdint.h>
#include <stdbool.h>
#include <lprintf.h>

extern volatile struct limine_hhdm_request hhdm_request;

static volatile uint32_t* ioapic_base;
static uint32_t gsi_base;

static inline void mmio_write32(volatile uint32_t* p, uint32_t v) { *p = v; (void)*p; }
static inline uint32_t mmio_read32(volatile uint32_t* p) { return *p; }

static inline void ioapic_write(uint8_t reg, uint32_t val) {
    volatile uint32_t* sel = ioapic_base;
    volatile uint32_t* win = ioapic_base + 4;
    mmio_write32(sel, reg);
    mmio_write32(win, val);
}

static inline uint32_t ioapic_read(uint8_t reg) {
    volatile uint32_t* sel = ioapic_base;
    volatile uint32_t* win = ioapic_base + 4;
    mmio_write32(sel, reg);
    return mmio_read32(win);
}

bool ioapic_supported(void) {
    ioapic_info_t info;
    if (!acpi_get_first_ioapic(&info)) return false;
    ioapic_base = (volatile uint32_t*)ioremap(info.phys_base, 0x20);
    if (!ioapic_base) { error_printf("IOAPIC: ioremap failed for phys=%#010x\n", (unsigned)info.phys_base); return false; }
    gsi_base = info.gsi_base;
    debug_printf("IOAPIC: phys=%#010x -> base=%p gsi_base=%u\n", (unsigned)info.phys_base, ioapic_base, (unsigned)gsi_base);
    uint32_t ver = ioapic_read(0x01);
    debug_printf("IOAPIC: VER=%#08x\n", ver);
    return true;
}

void ioapic_init(void) {
    // nothing to do yet
}

uint32_t ioapic_get_gsi_base(void) {
    return gsi_base;
}

static void ioapic_write_redir(uint8_t index, uint64_t value) {
    // redirection table starts at 0x10, each entry 2 regs
    ioapic_write(0x10 + index * 2, (uint32_t)(value & 0xFFFFFFFF));
    ioapic_write(0x11 + index * 2, (uint32_t)(value >> 32));
}

static uint64_t ioapic_read_redir(uint8_t index) {
    uint32_t lo, hi;
    lo = ioapic_read(0x10 + index * 2);
    hi = ioapic_read(0x11 + index * 2);
    return ((uint64_t)hi << 32) | lo;
}

void ioapic_mask_irq(uint8_t gsi) {
    uint8_t idx = (uint8_t)(gsi - gsi_base);
    uint64_t red = ioapic_read_redir(idx);
    red |= (1ULL << 16); // mask
    debug_printf("IOAPIC: mask gsi=%u idx=%u before=%#016llx after=%#016llx\n", (unsigned)gsi, (unsigned)idx, (unsigned long long)ioapic_read_redir(idx), (unsigned long long)red);
    ioapic_write_redir(idx, red);
}

void ioapic_unmask_irq(uint8_t gsi) {
    uint8_t idx = (uint8_t)(gsi - gsi_base);
    uint64_t red = ioapic_read_redir(idx);
    red &= ~(1ULL << 16);
    debug_printf("IOAPIC: unmask gsi=%u idx=%u before=%#016llx after=%#016llx\n", (unsigned)gsi, (unsigned)idx, (unsigned long long)ioapic_read_redir(idx), (unsigned long long)red);
    ioapic_write_redir(idx, red);
}

void ioapic_route_irq(uint8_t gsi, uint8_t vector) {
    uint8_t idx = (uint8_t)(gsi - gsi_base);
    uint64_t red = ioapic_read_redir(idx);
    red &= ~0xFFULL;       // clear vector
    red |= vector;         // set vector
    red &= ~(1ULL << 11);  // physical delivery
    red &= ~(1ULL << 13);  // active high
    red &= ~(1ULL << 15);  // edge triggered
    debug_printf("IOAPIC: route gsi=%u idx=%u vec=%u red=%#016llx\n", (unsigned)gsi, (unsigned)idx, (unsigned)vector, (unsigned long long)red);
    ioapic_write_redir(idx, red);
}

uint32_t ioapic_gsi_base(void) {
    return gsi_base;
}

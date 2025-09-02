#include <rtc.h>
#include <io.h>
#include <isr.h>
#include <pic.h>

#define CMOS_ADDR 0x70
#define CMOS_DATA 0x71

static inline uint8_t cmos_read(uint8_t reg) {
    outb(CMOS_ADDR, reg); return inb(CMOS_DATA);
}
static inline void cmos_write(uint8_t reg, uint8_t val) {
    outb(CMOS_ADDR, reg); outb(CMOS_DATA, val);
}

static rtc_callback_t rcbs[8];

static void rtc_irq(isr_frame_t* f) {
    (void)f;
    // Read C to clear the interrupt
    outb(CMOS_ADDR, 0x0C); (void)inb(CMOS_DATA);
    for (int i=0;i<8;++i) if (rcbs[i]) rcbs[i]();
    pic_send_eoi(40); // IRQ8 = vector 40
}

void rtc_init_periodic(int rate_power2) {
    // Clamp rate to valid RTC periodic range [3, 15]
    if (rate_power2 < 3) {
        rate_power2 = 3;
    }
    if (rate_power2 > 15) {
        rate_power2 = 15;
    }
    // Disable NMI
    uint8_t prev = cmos_read(0x0B);
    outb(CMOS_ADDR, 0x8B); outb(CMOS_DATA, (prev | 0x40)); // set PIE (bit 6)
    prev = cmos_read(0x0A);
    outb(CMOS_ADDR, 0x8A); outb(CMOS_DATA, (prev & 0xF0) | (rate_power2 & 0x0F));
    // Read C to reset
    (void)cmos_read(0x0C);
    isr_register(40, rtc_irq);
    // Unmask cascade (IRQ2) and IRQ8 (RTC)
    pic_clear_mask(2);
    pic_clear_mask(8);
}

int rtc_on_tick(rtc_callback_t cb) {
    for (int i=0;i<8;++i) if (!rcbs[i]) { rcbs[i]=cb; return 0; }
    return -1;
}

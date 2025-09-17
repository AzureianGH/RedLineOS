#include <rtc.h>
#include <io.h>
#include <isr.h>
#include <pic.h>
#include <lprintf.h>

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

static inline int is_update_in_progress(void) { return (cmos_read(0x0A) & 0x80) != 0; }

static inline uint8_t bcd_to_bin(uint8_t b) { return (uint8_t)((b & 0x0F) + ((b >> 4) * 10)); }

static void read_rtc_regs(uint8_t* sec, uint8_t* min, uint8_t* hour, uint8_t* day, uint8_t* mon, uint16_t* year, int* century) {
    // Wait for update to complete
    while (is_update_in_progress()) { /* spin */ }
    *sec  = cmos_read(0x00);
    *min  = cmos_read(0x02);
    *hour = cmos_read(0x04);
    *day  = cmos_read(0x07);
    *mon  = cmos_read(0x08);
    uint8_t y = cmos_read(0x09);
    *year = y;
    // Century register is platform dependent; not using by default
    if (century) *century = -1;
}

static void normalize_rtc_values(uint8_t* sec, uint8_t* min, uint8_t* hour, uint8_t* day, uint8_t* mon, uint16_t* year) {
    uint8_t regB = cmos_read(0x0B);
    // Convert BCD to binary if needed
    if (!(regB & 0x04)) {
        *sec  = bcd_to_bin(*sec);
        *min  = bcd_to_bin(*min);
        *hour = bcd_to_bin(*hour);
        *day  = bcd_to_bin(*day);
        *mon  = bcd_to_bin(*mon);
        *year = bcd_to_bin((uint8_t)*year);
    }
    // 12/24 hour
    if (!(regB & 0x02)) {
        // 12-hour format
        uint8_t h = *hour & 0x7F;
        uint8_t pm = *hour & 0x80;
        if (pm && h < 12) h += 12;
        if (!pm && h == 12) h = 0;
        *hour = h;
    }
    // Year expansion to full year (assume >= 2000 if <70, else 1900+)
    uint16_t y = *year;
    *year = (y < 70) ? (2000 + y) : (1900 + y);
}

static int is_leap(int y) { return (y % 4 == 0) && ((y % 100 != 0) || (y % 400 == 0)); }

static uint64_t ymd_hms_to_epoch(int year, int mon, int day, int hour, int min, int sec) {
    static const int mdays[12] = {31,28,31,30,31,30,31,31,30,31,30,31};
    // Days from 1970-01-01 to 00:00 of given date
    int y = 1970;
    uint64_t days = 0;
    while (y < year) { days += 365 + is_leap(y); ++y; }
    for (int m = 1; m < mon; ++m) { days += mdays[m-1] + (m==2 && is_leap(year)); }
    days += (day - 1);
    return days*86400ULL + (uint64_t)hour*3600ULL + (uint64_t)min*60ULL + (uint64_t)sec;
}

int rtc_read_tm(struct tm* out) {
    if (!out) return -1;
    uint8_t sec,min,hour,day,mon; uint16_t year;
    read_rtc_regs(&sec,&min,&hour,&day,&mon,&year,NULL);
    normalize_rtc_values(&sec,&min,&hour,&day,&mon,&year);
    out->tm_sec = sec; out->tm_min = min; out->tm_hour = hour;
    out->tm_mday = day; out->tm_mon = mon-1; out->tm_year = year - 1900;
    // Compute wday/yday if needed later; leave defaults for now
    out->tm_wday = 0; out->tm_yday = 0; out->tm_isdst = 0;
    return 0;
}

uint64_t rtc_read_epoch(void) {
    uint8_t sec1,min1,hour1,day1,mon1; uint16_t year1;
    uint8_t sec2,min2,hour2,day2,mon2; uint16_t year2;
    // Read twice until consistent (RTC updates can change values mid-read)
    do {
        read_rtc_regs(&sec1,&min1,&hour1,&day1,&mon1,&year1,NULL);
        normalize_rtc_values(&sec1,&min1,&hour1,&day1,&mon1,&year1);
        read_rtc_regs(&sec2,&min2,&hour2,&day2,&mon2,&year2,NULL);
        normalize_rtc_values(&sec2,&min2,&hour2,&day2,&mon2,&year2);
    } while (sec1!=sec2 || min1!=min2 || hour1!=hour2 || day1!=day2 || mon1!=mon2 || year1!=year2);

    return ymd_hms_to_epoch(year1, mon1, day1, hour1, min1, sec1);
}

#include <tsc.h>
#include <io.h>

#define PIT_CH0 0x40
#define PIT_CMD 0x43

uint64_t tsc_calibrate_hz(uint32_t pit_hz, uint32_t ms) {
    if (pit_hz == 0) pit_hz = 1193182u; // PIT input frequency
    if (ms == 0) ms = 10;
    // Program PIT one-shot to ms
    uint32_t ticks = (pit_hz / 1000u) * ms;
    outb(PIT_CMD, 0x30); // ch0, lobyte/hibyte, mode 0 (interrupt on terminal count)
    outb(PIT_CH0, (uint8_t)(ticks & 0xFF));
    outb(PIT_CH0, (uint8_t)((ticks >> 8) & 0xFF));
    uint64_t t0 = rdtsc();
    // Poll PIT gate by reading counter until wraps to zero (approx)
    uint16_t last = 0xFFFF, cur;
    do {
        outb(PIT_CMD, 0x00); // latch
        uint8_t lo = inb(PIT_CH0); uint8_t hi = inb(PIT_CH0); cur = (uint16_t)(lo | (hi<<8));
        if (cur > last) break; // wrapped
        last = cur;
    } while (1);
    uint64_t t1 = rdtsc();
    uint64_t cycles = t1 - t0;
    return (cycles * 1000u) / ms;
}

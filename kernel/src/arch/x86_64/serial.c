#include <serial.h>
#include <io.h>

#define COM1 0x3F8

static inline int serial_tx_empty(void) {
    return inb(COM1 + 5) & 0x20; // LSR THR empty
}

void serial_init(void) {
    outb(COM1 + 1, 0x00); // Disable all interrupts
    outb(COM1 + 3, 0x80); // Enable DLAB
    outb(COM1 + 0, 0x01); // Divisor low (115200/1=115200)
    outb(COM1 + 1, 0x00); // Divisor high
    outb(COM1 + 3, 0x03); // 8 bits, no parity, one stop
    outb(COM1 + 2, 0xC7); // Enable FIFO, clear, 14-byte threshold
    outb(COM1 + 4, 0x0B); // IRQs enabled, RTS/DSR set
}

void serial_putc(char c) {
    if (c == '\n') {
        // Convert to CRLF for typical terminals
        while (!serial_tx_empty()) { }
        outb(COM1, '\r');
    }
    while (!serial_tx_empty()) { }
    outb(COM1, (uint8_t)c);
}

void serial_write(const char* buf, size_t len) {
    for (size_t i = 0; i < len; ++i) serial_putc(buf[i]);
}

void serial_puts(const char* s) {
    while (*s) serial_putc(*s++);
}

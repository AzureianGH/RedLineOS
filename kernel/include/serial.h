#pragma once
#include <stddef.h>
#include <stdint.h>

// Initialize COM1 16550-compatible UART at 115200 8N1
void serial_init(void);

// Write a single character (blocking)
void serial_putc(char c);

// Write a buffer (blocking)
void serial_write(const char* buf, size_t len);

// Convenience write of NUL-terminated string
void serial_puts(const char* s);

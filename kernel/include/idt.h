#pragma once

#include <stdint.h>

// Initialize the IDT and install exception stubs for vectors 0..31.
void idt_init(void);

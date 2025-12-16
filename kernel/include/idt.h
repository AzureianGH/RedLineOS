#pragma once

#include <stdint.h>

// Initialize the IDT and install exception stubs for vectors 0..31.
void idt_init(void);

// Enable interrupts after IDT/GDT/PIC are fully configured.
void idt_enable_interrupts(void);

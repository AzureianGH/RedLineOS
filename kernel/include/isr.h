#pragma once

#include <stdint.h>
#include <stddef.h>

typedef struct isr_frame {
    uint64_t r15, r14, r13, r12, r11, r10, r9, r8;
    uint64_t rsi, rdi, rbp, rdx, rcx, rbx, rax;
    uint64_t int_no, err_code;
    uint64_t rip, cs, rflags, rsp, ss;
} isr_frame_t;

typedef void (*isr_handler_t)(isr_frame_t*);

// Register an ISR handler for a vector (0..255). Multiple handlers per vector.
int isr_register(uint8_t vector, isr_handler_t handler);
int isr_unregister(uint8_t vector, isr_handler_t handler);

// Default exception dispatchers
void exceptions_install_defaults(void);

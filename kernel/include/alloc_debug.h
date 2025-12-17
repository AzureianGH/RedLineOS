#pragma once

#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

// Set ALLOC_DEBUG to 0 at compile time to disable poisoning/redzones.
#ifndef ALLOC_DEBUG
#define ALLOC_DEBUG 1
#endif

#define ALLOC_POISON_ALLOC 0xAF
#define ALLOC_POISON_FREE  0xDD
#define ALLOC_REDZONE_BYTE 0xFA
#define ALLOC_REDZONE_SIZE 16U
#define ALLOC_DBG_MAGIC    0xFADEFADEu

static inline void alloc_dbg_fill(void *ptr, size_t len, uint8_t byte) {
    uint8_t *p = (uint8_t *)ptr;
    for (size_t i = 0; i < len; i++) p[i] = byte;
}

static inline int alloc_dbg_check(const void *ptr, size_t len, uint8_t byte) {
    const uint8_t *p = (const uint8_t *)ptr;
    for (size_t i = 0; i < len; i++) {
        if (p[i] != byte) return 0;
    }
    return 1;
}

static inline void alloc_debug_fail(const char *msg, const void *ptr) {
    printf("ALLOC DEBUG: %s %p\n", msg, ptr);
    __asm__ __volatile__("cli; hlt");
    for (;;) { __asm__ __volatile__("hlt"); }
}

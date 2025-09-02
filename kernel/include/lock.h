#pragma once

#include <stdint.h>

typedef struct {
    volatile uint32_t v;
} spinlock_t;

static inline void spinlock_init(spinlock_t *l) { l->v = 0; }

static inline void cpu_relax(void) { __asm__ __volatile__("pause"); }

static inline void spin_lock(spinlock_t *l) {
    for (;;) {
        if (__atomic_exchange_n(&l->v, 1, __ATOMIC_ACQUIRE) == 0) return;
        while (__atomic_load_n(&l->v, __ATOMIC_RELAXED)) cpu_relax();
    }
}

// Try to acquire the lock. Returns non-zero on success, 0 on failure.
static inline int spin_trylock(spinlock_t *l) {
    return __atomic_exchange_n(&l->v, 1, __ATOMIC_ACQUIRE) == 0;
}

static inline void spin_unlock(spinlock_t *l) {
    __atomic_store_n(&l->v, 0, __ATOMIC_RELEASE);
}

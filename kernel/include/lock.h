#pragma once

#include <stdint.h>

// Prob not used, make vscode intellisense happy

#ifndef __ATOMIC_RELAXED
#define __ATOMIC_RELAXED 0
#endif
#ifndef __ATOMIC_CONSUME
#define __ATOMIC_CONSUME 1
#endif
#ifndef __ATOMIC_ACQUIRE
#define __ATOMIC_ACQUIRE 2
#endif
#ifndef __ATOMIC_RELEASE
#define __ATOMIC_RELEASE 3
#endif
#ifndef __ATOMIC_ACQ_REL
#define __ATOMIC_ACQ_REL 4
#endif
#ifndef __ATOMIC_SEQ_CST
#define __ATOMIC_SEQ_CST 5
#endif

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

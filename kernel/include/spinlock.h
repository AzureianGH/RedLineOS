#pragma once
#include <stdatomic.h>

typedef struct { atomic_flag f; } spinlock_t;

static inline void spinlock_init(spinlock_t* l) { atomic_flag_clear(&l->f); }
static inline int  spin_trylock(spinlock_t* l) { return !atomic_flag_test_and_set_explicit(&l->f, memory_order_acquire); }
static inline void spin_lock(spinlock_t* l) { while (atomic_flag_test_and_set_explicit(&l->f, memory_order_acquire)) { __asm__ __volatile__("pause"); } }
static inline void spin_unlock(spinlock_t* l) { atomic_flag_clear_explicit(&l->f, memory_order_release); }

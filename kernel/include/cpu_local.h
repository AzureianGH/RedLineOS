#pragma once

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

typedef struct cpu_local {
    uint32_t cpu_index;    // 0-based logical CPU index (BSP=0)
    uint32_t lapic_id;     // APIC ID from firmware/MP table
    void    *tss_base;     // Optional: per-CPU TSS base (if assigned)
    void    *current_task; // Scheduler-owned pointer to current task on this CPU
    void    *idle_task;    // Scheduler-owned pointer to idle task on this CPU
    uint64_t tick_count;   // Per-CPU timer ticks
    bool     online;       // Set true once CPU is fully up
} cpu_local_t;

// Install the per-CPU pointer for this core.
void cpu_local_set(cpu_local_t* ptr);

// Retrieve the current CPU's local data pointer (may be NULL early).
cpu_local_t* cpu_local_get(void);

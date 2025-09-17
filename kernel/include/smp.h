#pragma once
#include <stdint.h>
#include <stdbool.h>

// Basic SMP info & init using Limine MP interface

typedef struct cpu_info {
    uint32_t lapic_id;
    uint32_t index;      // 0..n-1
    void*    stack_top;  // per-CPU scheduler/idle stack
} cpu_info_t;

bool     smp_init(void);
uint32_t smp_cpu_count(void);
uint32_t smp_bsp_lapic_id(void);
uint32_t smp_this_cpu_index(void);
const cpu_info_t* smp_get_cpu(uint32_t idx);

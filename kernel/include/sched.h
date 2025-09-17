#pragma once
#include <stdint.h>
#include <stdbool.h>
#include <stdatomic.h>
#include <isr.h>

typedef enum { THREAD_UNUSED=0, THREAD_RUNNABLE, THREAD_RUNNING, THREAD_SLEEPING, THREAD_ZOMBIE } thread_state_t;

typedef struct thread thread_t;
typedef void (*thread_entry_t)(void*);

struct cpu_runq {
    thread_t* head;
    thread_t* tail;
};

struct thread {
    // Saved context (minimal: callee-saved + rip/rsp) captured/restored in asm
    uint64_t rsp;
    uint64_t rip;
    uint64_t rbx, rbp, r12, r13, r14, r15;
    uint64_t fx_state; // reserved for future SSE state save

    uint32_t tid;
    uint32_t cpu;
    thread_state_t state;
    uint64_t wakeup_ms; // for sleep

    // Stack and linkage
    void*    stack_base;
    uint64_t stack_size;
    thread_t* next;

    // Accounting
    uint32_t time_slice;
};

void sched_init(void);
thread_t* sched_create(thread_entry_t entry, void* arg, const char* name);
void sched_yield(void);
void sched_sleep_ms(uint64_t ms);
void sched_on_timer_tick(isr_frame_t* f);
void sched_start(void); // enter scheduler loop on each CPU

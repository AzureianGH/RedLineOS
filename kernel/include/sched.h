#pragma once

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <isr.h>

#define TASK_NAME_MAX 32

typedef void (*task_entry_t)(void *arg);

typedef enum {
    TASK_RUNNABLE = 0,
    TASK_BLOCKED,
    TASK_ZOMBIE,
} task_state_t;

typedef struct task_context {
    uint64_t r15, r14, r13, r12;
    uint64_t rbx, rbp;
    uint64_t r11, r10, r9, r8;
    uint64_t rax, rcx, rdx, rsi, rdi;
    uint64_t rsp;
    uint64_t rip;
    uint64_t rflags;
} task_context_t;

typedef struct task {
    struct task *next;
    uint64_t id;
    char name[TASK_NAME_MAX];
    task_state_t state;
    task_context_t ctx;
    void *stack_base;
    size_t stack_size;
    uint64_t stack_highwater;
    uint8_t stack_warn_bucket;
    task_entry_t entry;
    void *arg;
} task_t;

void scheduler_init(uint32_t tick_hz_hint, uint64_t tsc_hz_hint);
void scheduler_start(void);
int task_create(const char *name, task_entry_t entry, void *arg, size_t stack_pages);
void task_yield(void);
__attribute__((noreturn)) void task_exit(void);
void task_block(void);
int task_wake(task_t *t);

// Called from timer ISRs to drive preemption.
void scheduler_tick(isr_frame_t *frame);

// Current task accessor.
task_t *scheduler_current(void);

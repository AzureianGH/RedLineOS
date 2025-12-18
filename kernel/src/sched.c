#include <sched.h>
#include <spinlock.h>
#include <stdlib.h>
#include <string.h>
#include <lprintf.h>
#include <timer.h>
#include <palloc.h>
#include <vmm.h>
#include <vheap.h>

extern void context_switch(task_context_t *prev, task_context_t *next);

static spinlock_t sched_lock;
static task_t *current_task;
static task_t *rq_head, *rq_tail;
static task_t *sleep_head;
static task_t bootstrap_task;
static task_t *idle_task;
static uint64_t next_tid = 1;
static uint32_t timeslice_ticks = 10;
static uint32_t tick_log_div = 100;
static uint64_t tick_counter;
static bool sched_started;

#define MIN_STACK_PAGES 16ULL

static inline void irq_disable(void) { __asm__ __volatile__("cli" ::: "memory"); }
static inline void irq_enable(void) { __asm__ __volatile__("sti" ::: "memory"); }
static inline uint64_t read_rsp(void) { uint64_t v; __asm__ __volatile__("mov %%rsp,%0" : "=r"(v)); return v; }

#define PAGE_SIZE 0x1000ULL
#define STACK_CANARY 0xCAFEBABEDEADBEEFULL

static inline uint64_t *stack_canary_slot(task_t *t) {
    if (!t || !t->stack_base) return NULL;
    return (uint64_t *)((uint8_t *)t->stack_base - PAGE_SIZE);
}

static inline bool stack_canary_ok(task_t *t) {
    uint64_t *slot = stack_canary_slot(t);
    return slot && *slot == STACK_CANARY;
}

static void record_stack_usage(task_t *t, uint64_t rsp) {
    if (!t || !t->stack_base) return;
    uint64_t top = (uint64_t)(uintptr_t)t->stack_base + t->stack_size;
    if (rsp > top) return;
    uint64_t used = top - rsp;
    if (used > t->stack_highwater) {
        t->stack_highwater = used;
        uint64_t pct = (used * 100ULL) / (t->stack_size ? t->stack_size : 1ULL);
        uint8_t bucket = (uint8_t)(pct / 5ULL);
        if (pct >= 75 && bucket > t->stack_warn_bucket) {
            t->stack_warn_bucket = bucket;
            info_printf("sched: task %s stack highwater %llu/%zu (%llu%%)\n",
                        t->name, (unsigned long long)used,
                        t->stack_size, (unsigned long long)pct);
        }
    }
}

static __attribute__((noreturn)) void stack_overflow(task_t *t) {
    error_printf("sched: stack overflow detected in task %s (id=%llu)\n",
                 t ? t->name : "?", (unsigned long long)(t ? t->id : 0ULL));
    for (;;) { __asm__ __volatile__("cli; hlt"); }
}

static void enqueue(task_t *t) {
    if (!t || t->state != TASK_RUNNABLE) return;
    if (t->stack_base && !stack_canary_ok(t)) stack_overflow(t);
    t->next = NULL;
    if (!rq_head) { rq_head = rq_tail = t; }
    else { rq_tail->next = t; rq_tail = t; }
}

static task_t *dequeue(void) {
    task_t *t = rq_head;
    if (t) {
        rq_head = t->next;
        if (!rq_head) rq_tail = NULL;
        t->next = NULL;
    }
    return t;
}

static void sleep_insert(task_t *t) {
    if (!t) return;
    t->next = NULL;
    if (!sleep_head || t->wake_tick < sleep_head->wake_tick) {
        t->next = sleep_head;
        sleep_head = t;
        return;
    }
    task_t *cur = sleep_head;
    while (cur->next && cur->next->wake_tick <= t->wake_tick) {
        cur = cur->next;
    }
    t->next = cur->next;
    cur->next = t;
}

static bool sleep_remove(task_t *t) {
    if (!t || !sleep_head) return false;
    if (sleep_head == t) {
        sleep_head = t->next;
        t->next = NULL;
        return true;
    }
    task_t *cur = sleep_head;
    while (cur->next && cur->next != t) {
        cur = cur->next;
    }
    if (cur->next == t) {
        cur->next = t->next;
        t->next = NULL;
        return true;
    }
    return false;
}

static void idle_entry(void *arg) {
    (void)arg;
    for (;;) {
        __asm__ __volatile__("hlt");
    }
}

static void task_bootstrap(void) {
    task_t *t = current_task;
    if (t && t->entry) {
        t->entry(t->arg);
    }
    task_exit();
}

static void setup_stack(task_t *t, size_t stack_pages) {
    size_t usable_pages = stack_pages ? (stack_pages < MIN_STACK_PAGES ? MIN_STACK_PAGES : stack_pages) : MIN_STACK_PAGES; // 64 KiB min default
    size_t guard_pages = 1; // lightweight guard with canary
    size_t total_pages = usable_pages + guard_pages;
    size_t bytes = total_pages * PAGE_SIZE;
    uint64_t base = vheap_commit(bytes);
    if (!base) {
        error_printf("sched: failed to allocate stack for task %s\n", t->name);
        t->stack_base = NULL;
        t->stack_size = 0;
        return;
    }
    // Place a canary in the guard page; stack grows down from the top of the usable region.
    uint64_t guard_base = base;
    *((uint64_t *)guard_base) = STACK_CANARY;

    uint64_t usable_base = base + guard_pages * PAGE_SIZE;
    t->stack_base = (void *)(uintptr_t)usable_base;
    t->stack_size = usable_pages * PAGE_SIZE;
    t->stack_highwater = 0;
    t->stack_warn_bucket = 0;

    uint64_t top = usable_base + t->stack_size;
    top &= ~0xFULL; // align 16
    t->ctx.rsp = top;
    t->ctx.rip = (uint64_t)task_bootstrap;
    t->ctx.rflags = 0x202ULL;
    t->ctx.r15 = t->ctx.r14 = t->ctx.r13 = t->ctx.r12 = 0;
    t->ctx.rbx = t->ctx.rbp = 0;
    t->ctx.r11 = t->ctx.r10 = t->ctx.r9 = t->ctx.r8 = 0;
    t->ctx.rax = t->ctx.rcx = t->ctx.rdx = t->ctx.rsi = t->ctx.rdi = 0;
}

static task_t *task_alloc(const char *name, task_entry_t entry, void *arg, size_t stack_pages) {
    task_t *t = calloc(1, sizeof(task_t));
    if (!t) return NULL;
    t->id = next_tid++;
    t->state = TASK_RUNNABLE;
    t->entry = entry;
    t->arg = arg;
    if (name) {
        strncpy(t->name, name, TASK_NAME_MAX - 1);
    } else {
        strncpy(t->name, "task", TASK_NAME_MAX - 1);
    }
    setup_stack(t, stack_pages);
    if (!t->stack_base) { free(t); return NULL; }
    return t;
}

void scheduler_init(uint32_t tick_hz_hint, uint64_t tsc_hz_hint) {
    (void)tsc_hz_hint;
    spinlock_init(&sched_lock);
    sched_started = false;
    rq_head = rq_tail = sleep_head = NULL;
    tick_counter = 0;
    if (tick_hz_hint >= 1000) {
        timeslice_ticks = tick_hz_hint / 200; // ~5ms
        tick_log_div = tick_hz_hint; // log about once per second
    } else if (tick_hz_hint >= 100) {
        timeslice_ticks = tick_hz_hint / 100; // ~10ms
        tick_log_div = tick_hz_hint; // ~1s
    } else {
        timeslice_ticks = 10;
        tick_log_div = 100;
    }
    if (timeslice_ticks == 0) timeslice_ticks = 1;

    memset(&bootstrap_task, 0, sizeof(bootstrap_task));
    bootstrap_task.id = 0;
    strncpy(bootstrap_task.name, "bootstrap", TASK_NAME_MAX - 1);
    bootstrap_task.state = TASK_RUNNABLE;
    bootstrap_task.ctx.rsp = read_rsp();
    bootstrap_task.ctx.rip = 0; // will be set on first tick save
    bootstrap_task.ctx.rflags = 0x202ULL;
    bootstrap_task.ctx.r11 = bootstrap_task.ctx.r10 = bootstrap_task.ctx.r9 = bootstrap_task.ctx.r8 = 0;
    bootstrap_task.ctx.rax = bootstrap_task.ctx.rcx = bootstrap_task.ctx.rdx = bootstrap_task.ctx.rsi = bootstrap_task.ctx.rdi = 0;
    bootstrap_task.stack_highwater = 0;
    bootstrap_task.stack_warn_bucket = 0;
    current_task = &bootstrap_task;

    idle_task = task_alloc("idle", idle_entry, NULL, 2);
    if (idle_task) {
        enqueue(idle_task);
    } else {
        error_printf("sched: failed to create idle task\n");
    }
}

void scheduler_start(void) {
    sched_started = true;
    // If there is any runnable task, yield into it to start scheduling
    task_yield();
}

bool scheduler_is_started(void) {
    return sched_started;
}

task_t *scheduler_current(void) { return current_task; }

int task_create(const char *name, task_entry_t entry, void *arg, size_t stack_pages) {
    task_t *t = task_alloc(name, entry, arg, stack_pages);
    if (!t) return -1;
    irq_disable();
    spin_lock(&sched_lock);
    enqueue(t);
    spin_unlock(&sched_lock);
    irq_enable();
    return (int)t->id;
}

__attribute__((noreturn)) void task_exit(void) {
    irq_disable();
    spin_lock(&sched_lock);
    task_t *prev = current_task;
    record_stack_usage(prev, read_rsp());
    if (prev->stack_base && !stack_canary_ok(prev)) stack_overflow(prev);
    prev->state = TASK_ZOMBIE;
    task_t *next = dequeue();
    if (!next) {
        spin_unlock(&sched_lock);
        error_printf("sched: no runnable tasks, halting\n");
        for (;;) { __asm__ __volatile__("cli; hlt"); }
    }
    current_task = next;
    spin_unlock(&sched_lock);
    irq_enable();
    context_switch(&prev->ctx, &next->ctx);
    __builtin_unreachable();
}

void task_yield(void) {
    if (!sched_started) return;
    irq_disable();
    spin_lock(&sched_lock);
    task_t *prev = current_task;
    record_stack_usage(prev, read_rsp());
    task_t *next = dequeue();
    if (!next) {
        spin_unlock(&sched_lock);
        irq_enable();
        return;
    }
    if (prev->stack_base && !stack_canary_ok(prev)) stack_overflow(prev);
    enqueue(prev);
    current_task = next;
    spin_unlock(&sched_lock);
    irq_enable();
    context_switch(&prev->ctx, &next->ctx);
}

void task_block(void) {
    if (!sched_started) return;
    irq_disable();
    spin_lock(&sched_lock);
    task_t *prev = current_task;
    prev->state = TASK_BLOCKED;
    task_t *next = dequeue();
    if (!next) {
        spin_unlock(&sched_lock);
        error_printf("sched: all tasks blocked, halting\n");
        for (;;) { __asm__ __volatile__("cli; hlt"); }
    }
    current_task = next;
    spin_unlock(&sched_lock);
    irq_enable();
    context_switch(&prev->ctx, &next->ctx);
}

void task_sleep_ticks(uint64_t ticks) {
    if (ticks == 0) { task_yield(); return; }
    if (!sched_started) {
        uint64_t start = (uint64_t)timer_get_ticks();
        while (((uint64_t)timer_get_ticks() - start) < ticks) {
            __asm__ __volatile__("pause");
        }
        return;
    }
    irq_disable();
    spin_lock(&sched_lock);
    task_t *prev = current_task;
    prev->state = TASK_BLOCKED;
    prev->wake_tick = tick_counter + ticks;
    sleep_insert(prev);
    task_t *next = dequeue();
    if (!next) {
        spin_unlock(&sched_lock);
        error_printf("sched: all tasks sleeping, halting\n");
        for (;;) { __asm__ __volatile__("cli; hlt"); }
    }
    current_task = next;
    spin_unlock(&sched_lock);
    irq_enable();
    context_switch(&prev->ctx, &next->ctx);
}

int task_wake(task_t *t) {
    if (!t || t->state != TASK_BLOCKED) return -1;
    irq_disable();
    spin_lock(&sched_lock);
    sleep_remove(t);
    t->state = TASK_RUNNABLE;
    t->wake_tick = 0;
    enqueue(t);
    spin_unlock(&sched_lock);
    irq_enable();
    return 0;
}

void scheduler_tick(isr_frame_t *frame) {
    if (!sched_started) return;
    ++tick_counter;
    // Wake any sleepers whose deadlines have passed
    if (sleep_head && sleep_head->wake_tick <= tick_counter) {
        spin_lock(&sched_lock);
        while (sleep_head && sleep_head->wake_tick <= tick_counter) {
            task_t *t = sleep_head;
            sleep_head = t->next;
            t->next = NULL;
            t->state = TASK_RUNNABLE;
            t->wake_tick = 0;
            enqueue(t);
        }
        spin_unlock(&sched_lock);
    }
    if (tick_log_div && (tick_counter % tick_log_div) == 0) {
        debug_printf("sched: tick=%llu current=%s\n",
                     (unsigned long long)tick_counter,
                     current_task ? current_task->name : "?");
    }
    if (timeslice_ticks == 0 || (tick_counter % timeslice_ticks)) return;

    spin_lock(&sched_lock);
    task_t *prev = current_task;
    task_t *next = dequeue();
    if (!next || next == prev) {
        if (next && next != prev) enqueue(next);
        spin_unlock(&sched_lock);
        return;
    }
    record_stack_usage(prev, frame->rsp);
    if (prev->stack_base && !stack_canary_ok(prev)) stack_overflow(prev);
    if (next->stack_base && !stack_canary_ok(next)) stack_overflow(next);
    // Save current registers into prev context
    prev->ctx.r15 = frame->r15; prev->ctx.r14 = frame->r14; prev->ctx.r13 = frame->r13; prev->ctx.r12 = frame->r12;
    prev->ctx.rbx = frame->rbx; prev->ctx.rbp = frame->rbp; prev->ctx.rsp = frame->rsp;
    prev->ctx.r11 = frame->r11; prev->ctx.r10 = frame->r10; prev->ctx.r9 = frame->r9; prev->ctx.r8 = frame->r8;
    prev->ctx.rax = frame->rax; prev->ctx.rcx = frame->rcx; prev->ctx.rdx = frame->rdx; prev->ctx.rsi = frame->rsi; prev->ctx.rdi = frame->rdi;
    prev->ctx.rip = frame->rip; prev->ctx.rflags = frame->rflags;
    enqueue(prev);
    current_task = next;

    // Load next context into frame
    frame->r15 = next->ctx.r15; frame->r14 = next->ctx.r14; frame->r13 = next->ctx.r13; frame->r12 = next->ctx.r12;
    frame->rbx = next->ctx.rbx; frame->rbp = next->ctx.rbp; frame->rsp = next->ctx.rsp;
    frame->r11 = next->ctx.r11; frame->r10 = next->ctx.r10; frame->r9 = next->ctx.r9; frame->r8 = next->ctx.r8;
    frame->rax = next->ctx.rax; frame->rcx = next->ctx.rcx; frame->rdx = next->ctx.rdx; frame->rsi = next->ctx.rsi; frame->rdi = next->ctx.rdi;
    frame->rip = next->ctx.rip; frame->rflags = next->ctx.rflags;

    spin_unlock(&sched_lock);
}

#include <sched.h>
#include <smp.h>
#include <ktime.h>
#include <lprintf.h>
#include <stdlib.h>
#include <string.h>
#include <spinlock.h>

void sched_isr_install(void);

// Simple global thread table
#define MAX_THREADS 128
static thread_t g_threads[MAX_THREADS];
static atomic_uint g_next_tid;
static spinlock_t g_threads_lock;

// Per-CPU current thread and run queue
static thread_t* g_current[64];
static struct cpu_runq g_runqueues[64];
static spinlock_t g_rqlock[64];

// Quantum in ticks
static const uint32_t TIME_SLICE_TICKS = 10; // ~10ms at 1kHz

extern void __thread_trampoline(void);
extern void __ctx_switch(thread_t* prev, thread_t* next);

static void runq_push(uint32_t cpu, thread_t* t) {
    spin_lock(&g_rqlock[cpu]);
    t->next = NULL;
    if (!g_runqueues[cpu].tail) {
        g_runqueues[cpu].head = g_runqueues[cpu].tail = t;
    } else {
        g_runqueues[cpu].tail->next = t;
        g_runqueues[cpu].tail = t;
    }
    spin_unlock(&g_rqlock[cpu]);
}

static thread_t* runq_pop(uint32_t cpu) {
    spin_lock(&g_rqlock[cpu]);
    thread_t* t = g_runqueues[cpu].head;
    if (t) {
        g_runqueues[cpu].head = t->next;
        if (!g_runqueues[cpu].head) g_runqueues[cpu].tail = NULL;
        t->next = NULL;
    }
    spin_unlock(&g_rqlock[cpu]);
    return t;
}

static thread_t* alloc_thread(void) {
    spin_lock(&g_threads_lock);
    for (int i = 0; i < MAX_THREADS; ++i) {
        if (g_threads[i].state == THREAD_UNUSED) {
            memset(&g_threads[i], 0, sizeof(thread_t));
            g_threads[i].state = THREAD_ZOMBIE; // temp
            spin_unlock(&g_threads_lock);
            return &g_threads[i];
        }
    }
    spin_unlock(&g_threads_lock);
    return NULL;
}

void sched_init(void) {
    spinlock_init(&g_threads_lock);
    for (int i = 0; i < 64; ++i) { spinlock_init(&g_rqlock[i]); g_runqueues[i].head=g_runqueues[i].tail=NULL; g_current[i]=NULL; }
    sched_isr_install();
}

// Set up a new kernel thread stack such that it returns into __thread_trampoline(entry,arg)
static void thread_setup_stack(thread_t* t, thread_entry_t entry, void* arg) {
    const uint64_t stack_size = 16384; // 16 KiB
    void* stack = malloc(stack_size);
    t->stack_base = stack;
    t->stack_size = stack_size;
    // Stack grows down; place [retaddr][entry][arg] for trampoline
    uint64_t* sp = (uint64_t*)((uint8_t*)stack + stack_size);
    *(--sp) = (uint64_t)0;           // return address (unused)
    *(--sp) = (uint64_t)entry;       // entry pointer
    *(--sp) = (uint64_t)arg;         // argument
    t->rsp = (uint64_t)sp;
    t->rip = (uint64_t)__thread_trampoline;
}

thread_t* sched_create(thread_entry_t entry, void* arg, const char* name) {
    (void)name;
    thread_t* t = alloc_thread();
    if (!t) return NULL;
    t->tid = atomic_fetch_add(&g_next_tid, 1);
    t->cpu = smp_this_cpu_index();
    t->state = THREAD_RUNNABLE;
    t->time_slice = TIME_SLICE_TICKS;
    thread_setup_stack(t, entry, arg);
    runq_push(t->cpu, t);
    return t;
}

static void schedule(isr_frame_t* f) {
    uint32_t cpu = smp_this_cpu_index();
    thread_t* cur = g_current[cpu];

    // Wake sleeping threads whose time has come (linear scan; small counts assumed)
    uint64_t now = ktime_millis();
    for (int i = 0; i < MAX_THREADS; ++i) {
        thread_t* th = &g_threads[i];
        if (th->state == THREAD_SLEEPING && th->wakeup_ms <= now) {
            th->state = THREAD_RUNNABLE; runq_push(cpu, th);
        }
    }

    if (cur && cur->state == THREAD_RUNNING) {
        if (cur->time_slice > 0) cur->time_slice--;
        if (cur->time_slice > 0) return; // keep running
        // time slice expired; enqueue and pick next
        cur->state = THREAD_RUNNABLE; cur->time_slice = TIME_SLICE_TICKS;
        runq_push(cpu, cur);
    }

    thread_t* next = runq_pop(cpu);
    if (!next) return; // nothing to run

    if (cur == NULL) {
        // First time scheduling on this CPU: just jump to next by editing frame
        g_current[cpu] = next; next->state = THREAD_RUNNING; next->time_slice = TIME_SLICE_TICKS;
        // Modify interrupt frame to context-switch when returning from ISR
        f->rip = next->rip;
        f->rsp = next->rsp;
        return;
    }

    if (cur == next) { next->state = THREAD_RUNNING; return; }

    // Save current thread callee-saved regs from frame and switch
    g_current[cpu] = next;
    next->state = THREAD_RUNNING; next->time_slice = TIME_SLICE_TICKS;
    // Save current context from frame
    cur->rsp = f->rsp; cur->rip = f->rip;
    // Load next context into frame
    f->rip = next->rip; f->rsp = next->rsp;
}

void sched_on_timer_tick(isr_frame_t* f) {
    schedule(f);
}

void sched_yield(void) {
    // Trigger a software interrupt to force schedule; use vector 242 stub defined
    extern void isr_stub_242(void);
    __asm__ __volatile__("int $242" ::: "memory");
}

void sched_sleep_ms(uint64_t ms) {
    uint32_t cpu = smp_this_cpu_index();
    thread_t* cur = g_current[cpu];
    if (!cur) return;
    cur->wakeup_ms = ktime_millis() + ms;
    cur->state = THREAD_SLEEPING;
    // immediate yield
    sched_yield();
}

void sched_start(void) {
    uint32_t cpu = smp_this_cpu_index();
    info_printf("CPU%u entering scheduler\n", (unsigned)cpu);
    // Pick first runnable
    thread_t* next = runq_pop(cpu);
    if (!next) {
        // idle loop
        for (;;) { __asm__ __volatile__("hlt"); }
    }
    g_current[cpu] = next; next->state = THREAD_RUNNING;
    // Debug: dump initial context
    uint64_t* sp64 = (uint64_t*)next->rsp;
    info_printf("[sched] next rip=%p rsp=%p\n", (void*)next->rip, (void*)next->rsp);
    info_printf("[sched] stack[0]=%p stack[1]=%p\n", (void*)sp64[0], (void*)sp64[1]);
    // Sanity: ensure canonical addresses
    if (((sp64[0] >> 48) & 0xffff) != 0xffff && ((sp64[0] >> 48) & 0xffff) != 0x0000) {
    error_printf("[sched] Non-canonical entry pointer on stack: %p\n", (void*)sp64[0]);
    }
    if (((next->rip >> 48) & 0xffff) != 0xffff && ((next->rip >> 48) & 0xffff) != 0x0000) {
    error_printf("[sched] Non-canonical next->rip: %p\n", (void*)next->rip);
    }
    // Safety: ensure we start in our trampoline even if rip was clobbered
    next->rip = (uint64_t)__thread_trampoline;
    // Jump to thread
    __asm__ __volatile__(
        "mov %0, %%rsp\n\t"
        "jmp *%1\n\t"
        :: "r"(next->rsp), "r"(next->rip) : "memory");
}

#include <isr.h>
#include <sched.h>
#include <lprintf.h>

static void sw_yield_isr(isr_frame_t* f) {
    (void)f;
    // Let the scheduler pick next immediately
    sched_on_timer_tick(f);
}

void sched_isr_install(void) {
    // Vector 242 is free; stubs exist in isr_stubs.asm
    isr_register(242, sw_yield_isr);
}

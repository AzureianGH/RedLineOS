#include <pit.h>
#include <io.h>
#include <isr.h>
#include <pic.h>

#define PIT_CH0 0x40
#define PIT_CMD 0x43

static pit_callback_t cbs[8];

static void pit_irq(isr_frame_t* f) {
    (void)f;
    for (int i = 0; i < 8; ++i) if (cbs[i]) cbs[i]();
    pic_send_eoi(32);
}

void pit_init(uint32_t hz) {
    if (hz == 0) hz = 1000;
    uint32_t divisor = 1193182u / hz;
    outb(PIT_CMD, 0x36); // channel 0, lobyte/hibyte, mode 3, binary
    outb(PIT_CH0, (uint8_t)(divisor & 0xFF));
    outb(PIT_CH0, (uint8_t)((divisor >> 8) & 0xFF));
    isr_register(32, pit_irq);
    // Unmask IRQ0 (PIT)
    pic_clear_mask(0);
}

int pit_on_tick(pit_callback_t cb) {
    for (int i = 0; i < 8; ++i) { if (!cbs[i]) { cbs[i]=cb; return 0; } }
    return -1;
}

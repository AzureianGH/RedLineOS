#include <io.h>
#include <stdint.h>

#define PIC1_CMD 0x20
#define PIC1_DATA 0x21
#define PIC2_CMD 0xA0
#define PIC2_DATA 0xA1

#define ICW1_INIT 0x10
#define ICW1_ICW4 0x01
#define ICW4_8086 0x01

void pic_remap(void) {
    uint8_t a1 = inb(PIC1_DATA);
    uint8_t a2 = inb(PIC2_DATA);

    outb(PIC1_CMD, ICW1_INIT | ICW1_ICW4);
    io_wait();
    outb(PIC2_CMD, ICW1_INIT | ICW1_ICW4);
    io_wait();

    outb(PIC1_DATA, 0x20); // master offset 32
    io_wait();
    outb(PIC2_DATA, 0x28); // slave offset 40
    io_wait();

    outb(PIC1_DATA, 4); // tell master about slave at IRQ2
    io_wait();
    outb(PIC2_DATA, 2); // tell slave its cascade identity
    io_wait();

    outb(PIC1_DATA, ICW4_8086);
    io_wait();
    outb(PIC2_DATA, ICW4_8086);
    io_wait();

    // restore masks
    outb(PIC1_DATA, a1);
    outb(PIC2_DATA, a2);
}

static inline uint8_t pic_read_mask_master(void) { return inb(PIC1_DATA); }
static inline uint8_t pic_read_mask_slave(void) { return inb(PIC2_DATA); }

void pic_send_eoi(int irq_vector) {
    int irq = irq_vector - 32;
    if (irq >= 8) outb(PIC2_CMD, 0x20);
    outb(PIC1_CMD, 0x20);
}

void pic_set_mask(int irq) {
    if (irq < 8) {
        uint8_t m = pic_read_mask_master();
        m |= (1u << irq);
        outb(PIC1_DATA, m);
    } else {
        irq -= 8;
        uint8_t m = pic_read_mask_slave();
        m |= (1u << irq);
        outb(PIC2_DATA, m);
    }
}

void pic_clear_mask(int irq) {
    if (irq < 8) {
        uint8_t m = pic_read_mask_master();
        m &= (uint8_t)~(1u << irq);
        outb(PIC1_DATA, m);
    } else {
        irq -= 8;
        uint8_t m = pic_read_mask_slave();
        m &= (uint8_t)~(1u << irq);
        outb(PIC2_DATA, m);
    }
}

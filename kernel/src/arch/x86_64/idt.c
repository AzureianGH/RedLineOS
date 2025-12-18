#include <stdint.h>
#include <string.h>
#include <idt.h>
#include <spinlock.h>

extern void idt_load(void* idtr);

typedef struct __attribute__((packed)) {
    uint16_t offset_low;
    uint16_t selector;
    uint8_t  ist;
    uint8_t  type_attr;
    uint16_t offset_mid;
    uint32_t offset_high;
    uint32_t zero;
} idt_entry_t;

typedef struct __attribute__((packed)) {
    uint16_t limit;
    uint64_t base;
} idtr_t;

#define IDT_ENTRIES 256
static idt_entry_t idt[IDT_ENTRIES];
static idtr_t idtr;
static spinlock_t idt_lock = {0};
static int idt_built = 0;

// Stubs (defined in assembly)
extern void isr_stub_0(void);  extern void isr_stub_1(void);  extern void isr_stub_2(void);  extern void isr_stub_3(void);
extern void isr_stub_4(void);  extern void isr_stub_5(void);  extern void isr_stub_6(void);  extern void isr_stub_7(void);
extern void isr_stub_8(void);  extern void isr_stub_9(void);  extern void isr_stub_10(void); extern void isr_stub_11(void);
extern void isr_stub_12(void); extern void isr_stub_13(void); extern void isr_stub_14(void); extern void isr_stub_15(void);
extern void isr_stub_16(void); extern void isr_stub_17(void); extern void isr_stub_18(void); extern void isr_stub_19(void);
extern void isr_stub_20(void); extern void isr_stub_21(void); extern void isr_stub_22(void); extern void isr_stub_23(void);
extern void isr_stub_24(void); extern void isr_stub_25(void); extern void isr_stub_26(void); extern void isr_stub_27(void);
extern void isr_stub_28(void); extern void isr_stub_29(void); extern void isr_stub_30(void); extern void isr_stub_31(void);
// IRQ stubs 32..47
extern void irq_stub_32(void); extern void irq_stub_33(void); extern void irq_stub_34(void); extern void irq_stub_35(void);
extern void irq_stub_36(void); extern void irq_stub_37(void); extern void irq_stub_38(void); extern void irq_stub_39(void);
extern void irq_stub_40(void); extern void irq_stub_41(void); extern void irq_stub_42(void); extern void irq_stub_43(void);
extern void irq_stub_44(void); extern void irq_stub_45(void); extern void irq_stub_46(void); extern void irq_stub_47(void);
// LAPIC extra vectors
extern void isr_stub_240(void);
extern void isr_stub_241(void);
extern void isr_stub_242(void);
extern void isr_stub_255(void);

static void set_idt_gate(int vec, void* handler, uint8_t type_attr, uint8_t ist) {
    uint64_t addr = (uint64_t)handler;
    idt[vec].offset_low  = (uint16_t)(addr & 0xFFFF);
    idt[vec].selector    = 0x08; // kernel CS
    idt[vec].ist         = ist & 0x7;
    idt[vec].type_attr   = type_attr; // present, DPL=0, type=0xE (interrupt gate)
    idt[vec].offset_mid  = (uint16_t)((addr >> 16) & 0xFFFF);
    idt[vec].offset_high = (uint32_t)((addr >> 32) & 0xFFFFFFFF);
    idt[vec].zero        = 0;
}

void idt_init(void) {
    spin_lock(&idt_lock);
    if (!idt_built) {
        memset(idt, 0, sizeof(idt));

        // Interrupt gate: present | type=0xE
        const uint8_t gate = 0x8E;
        set_idt_gate(0,  isr_stub_0,  gate, 0);
        set_idt_gate(1,  isr_stub_1,  gate, 0);
        set_idt_gate(2,  isr_stub_2,  gate, 0);
        set_idt_gate(3,  isr_stub_3,  gate, 0);
        set_idt_gate(4,  isr_stub_4,  gate, 0);
        set_idt_gate(5,  isr_stub_5,  gate, 0);
        set_idt_gate(6,  isr_stub_6,  gate, 0);
        set_idt_gate(7,  isr_stub_7,  gate, 0);
        set_idt_gate(8,  isr_stub_8,  gate, 0);
        set_idt_gate(9,  isr_stub_9,  gate, 0);
        set_idt_gate(10, isr_stub_10, gate, 0);
        set_idt_gate(11, isr_stub_11, gate, 0);
        set_idt_gate(12, isr_stub_12, gate, 0);
        set_idt_gate(13, isr_stub_13, gate, 0);
        set_idt_gate(14, isr_stub_14, gate, 0);
        set_idt_gate(15, isr_stub_15, gate, 0);
        set_idt_gate(16, isr_stub_16, gate, 0);
        set_idt_gate(17, isr_stub_17, gate, 0);
        set_idt_gate(18, isr_stub_18, gate, 0);
        set_idt_gate(19, isr_stub_19, gate, 0);
        set_idt_gate(20, isr_stub_20, gate, 0);
        set_idt_gate(21, isr_stub_21, gate, 0);
        set_idt_gate(22, isr_stub_22, gate, 0);
        set_idt_gate(23, isr_stub_23, gate, 0);
        set_idt_gate(24, isr_stub_24, gate, 0);
        set_idt_gate(25, isr_stub_25, gate, 0);
        set_idt_gate(26, isr_stub_26, gate, 0);
        set_idt_gate(27, isr_stub_27, gate, 0);
        set_idt_gate(28, isr_stub_28, gate, 0);
        set_idt_gate(29, isr_stub_29, gate, 0);
        set_idt_gate(30, isr_stub_30, gate, 0);
        set_idt_gate(31, isr_stub_31, gate, 0);

        // IRQs 32..47
        set_idt_gate(32, irq_stub_32, gate, 0);
        set_idt_gate(33, irq_stub_33, gate, 0);
        set_idt_gate(34, irq_stub_34, gate, 0);
        set_idt_gate(35, irq_stub_35, gate, 0);
        set_idt_gate(36, irq_stub_36, gate, 0);
        set_idt_gate(37, irq_stub_37, gate, 0);
        set_idt_gate(38, irq_stub_38, gate, 0);
        set_idt_gate(39, irq_stub_39, gate, 0);
        set_idt_gate(40, irq_stub_40, gate, 0);
        set_idt_gate(41, irq_stub_41, gate, 0);
        set_idt_gate(42, irq_stub_42, gate, 0);
        set_idt_gate(43, irq_stub_43, gate, 0);
        set_idt_gate(44, irq_stub_44, gate, 0);
        set_idt_gate(45, irq_stub_45, gate, 0);
        set_idt_gate(46, irq_stub_46, gate, 0);
        set_idt_gate(47, irq_stub_47, gate, 0);

        // LAPIC timer, panic, and spurious vectors
        set_idt_gate(240, isr_stub_240, gate, 0);
        set_idt_gate(241, isr_stub_241, gate, 0);
        set_idt_gate(242, isr_stub_242, gate, 0);
        set_idt_gate(255, isr_stub_255, gate, 0);

        idtr.base = (uint64_t)&idt[0];
        idtr.limit = sizeof(idt) - 1;
        idt_built = 1;
    }
    idt_load(&idtr);
    spin_unlock(&idt_lock);
}

void idt_enable_interrupts(void) {
    asm volatile("sti");
}

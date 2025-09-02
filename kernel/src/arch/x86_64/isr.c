#include <isr.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <panic.h>
#include <vheap.h>
#include <vmm.h>
#include <lprintf.h>

#define MAX_HANDLERS 8

static isr_handler_t handlers[256][MAX_HANDLERS];

int isr_register(uint8_t vector, isr_handler_t h) {
    for (int i = 0; i < MAX_HANDLERS; ++i) {
        if (handlers[vector][i] == NULL) { handlers[vector][i] = h; return 0; }
    }
    return -1;
}

int isr_unregister(uint8_t vector, isr_handler_t h) {
    for (int i = 0; i < MAX_HANDLERS; ++i) {
        if (handlers[vector][i] == h) { handlers[vector][i] = NULL; return 0; }
    }
    return -1;
}

static const char* exc_name(uint64_t v) {
    switch (v) {
        case 0: return "Divide-by-zero";
        case 1: return "Debug";
        case 2: return "NMI";
        case 3: return "Breakpoint";
        case 4: return "Overflow";
        case 5: return "BOUND range";
        case 6: return "Invalid opcode";
        case 7: return "Device not available";
        case 8: return "Double fault";
        case 10: return "Invalid TSS";
        case 11: return "Segment not present";
        case 12: return "Stack fault";
        case 13: return "General protection";
        case 14: return "Page fault";
        default: return "Exception";
    }
}

static void dump_page_fault(uint64_t err) {
    uint64_t cr2;
    __asm__ __volatile__("mov %%cr2, %0" : "=r"(cr2));
    printf("PF: addr=%p err=%llx [%s %s %s %s %s %s]\n",
           (void*)cr2, (unsigned long long)err,
           (err & 1)?"P":"NP",
           (err & 2)?"WR":"RD",
           (err & 4)?"USR":"SUP",
           (err & 8)?"RSV":"",
           (err & 16)?"IFETCH":"",
           (err & 32)?"PK":"");
}

void kernel_panic(const char* reason, const isr_frame_t* f) {
    printf("\n===== KERNEL PANIC =====\n");
    printf("Reason: %s\n\n", reason);

    printf(
        "RAX=0x%016llx RBX=0x%016llx RCX=0x%016llx RDX=0x%016llx\n"
        "RSI=0x%016llx RDI=0x%016llx RBP=0x%016llx RSP=0x%016llx\n"
        "R8=0x%016llx  R9=0x%016llx  R10=0x%016llx R11=0x%016llx\n"
        "R12=0x%016llx R13=0x%016llx R14=0x%016llx R15=0x%016llx\n\n"
        "RIP=0x%016llx CS=0x%04llx RFLAGS=0x%016llx\n"
        "SS=0x%04llx INT_NO=%llu ERR_CODE=0x%016llx\n",
        (unsigned long long) f->rax,
        (unsigned long long) f->rbx,
        (unsigned long long) f->rcx,
        (unsigned long long) f->rdx,
        (unsigned long long) f->rsi,
        (unsigned long long) f->rdi,
        (unsigned long long) f->rbp,
        (unsigned long long) f->rsp,
        (unsigned long long) f->r8,
        (unsigned long long) f->r9,
        (unsigned long long) f->r10,
        (unsigned long long) f->r11,
        (unsigned long long) f->r12,
        (unsigned long long) f->r13,
        (unsigned long long) f->r14,
        (unsigned long long) f->r15,
        (unsigned long long) f->rip,
        (unsigned long long) (f->cs & 0xFFFF),
        (unsigned long long) f->rflags,
        (unsigned long long) (f->ss & 0xFFFF),
        (unsigned long long) f->int_no,
        (unsigned long long) f->err_code
    );

    if (f->int_no == 14) {
        dump_page_fault(f->err_code);
    }

    for (;;) {
        __asm__ __volatile__("cli; hlt");
    }
}


static void default_exception(isr_frame_t* f) {
    if (f->int_no == 14) {
        uint64_t cr2;
        __asm__ __volatile__("mov %%cr2, %0" : "=r"(cr2));
        // Attempt recovery for non-present page inside reserved vheap range on kernel-mode access
        if ((f->err_code & 1ULL) == 0) { // P=0 (non-present)
            debug_printf("PF: Attempting recovery for faulting address %p, err_code=0x%llx\n", (void*)cr2, (unsigned long long)f->err_code);
            uint64_t base, size; vheap_bounds(&base, &size);
            if (base && cr2 >= base && cr2 < (base + size)) {
                if (vheap_map_one(cr2) == 0) {
                    debug_printf("PF: recovered by mapping vheap page at %p\n", (void*)cr2);
                    return; // recovered
                }
            }
            debug_printf("PF: recovery failed\n");
        }
        dump_page_fault(f->err_code);
    }
    kernel_panic(exc_name(f->int_no), f);
}

void exceptions_install_defaults(void) {
    for (int v = 0; v < 32; ++v) {
        isr_register((uint8_t)v, default_exception);
    }
}

// Called from assembly stubs with rdi = frame*
void isr_common_handler(isr_frame_t* f) {
    isr_handler_t* list = handlers[f->int_no];
    for (int i = 0; i < MAX_HANDLERS; ++i) {
        if (list[i]) list[i](f);
    }
}

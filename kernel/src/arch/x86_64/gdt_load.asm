bits 64
section .text

global gdt_load_and_ltr

; void gdt_load_and_ltr(uint64_t gdtr_addr, uint16_t tss_selector)
gdt_load_and_ltr:
    ; rdi = gdtr_addr, rsi = tss_selector (SysV ABI: first in rdi, second in rsi)
    lgdt [rdi]

    ; Reload segments: Set DS/ES/SS to kernel data, do a far jump to reload CS
    mov ax, 0x10            ; GDT_SELECTOR_KERNEL_DS
    mov ds, ax
    mov es, ax
    mov ss, ax

    ; Far jump to reload CS
    lea rax, [rel .flush_cs]
    push qword 0x08         ; GDT_SELECTOR_KERNEL_CS
    push rax
    retfq
.flush_cs:

    ; Load TSS
    mov ax, si
    ltr ax
    ret

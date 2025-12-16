bits 64
default rel
section .text

extern isr_common_handler

; Local trampoline to avoid cross-object relocation warnings
isr_common_trampoline:
    mov rax, isr_common_handler
    jmp rax

%macro PUSH_ALL 0
    push rax
    push rbx
    push rcx
    push rdx
    push rbp
    push rdi
    push rsi
    push r8
    push r9
    push r10
    push r11
    push r12
    push r13
    push r14
    push r15
%endmacro

%macro POP_ALL 0
    pop r15
    pop r14
    pop r13
    pop r12
    pop r11
    pop r10
    pop r9
    pop r8
    pop rsi
    pop rdi
    pop rbp
    pop rdx
    pop rcx
    pop rbx
    pop rax
%endmacro

%macro ISR_NOERR 1
global isr_stub_%1
isr_stub_%1:
    push qword 0              ; err_code
    push qword %1             ; int_no
    PUSH_ALL
    mov rdi, rsp              ; arg: frame*
    sub rsp, 8                ; align stack to 16B before call
    call isr_common_trampoline
    add rsp, 8
    POP_ALL
    add rsp, 16               ; pop int_no, err_code
    iretq
%endmacro

%macro ISR_ERR 1
global isr_stub_%1
isr_stub_%1:
    push qword %1             ; int_no
    PUSH_ALL
    mov rdi, rsp
    sub rsp, 8
    call isr_common_trampoline
    add rsp, 8
    POP_ALL
    add rsp, 8                ; pop int_no (err_code left by CPU)
    iretq
%endmacro

; 0..31 exception stubs
ISR_NOERR 0
ISR_NOERR 1
ISR_NOERR 2
ISR_NOERR 3
ISR_NOERR 4
ISR_NOERR 5
ISR_NOERR 6
ISR_NOERR 7
ISR_ERR   8
ISR_NOERR 9
ISR_ERR   10
ISR_ERR   11
ISR_ERR   12
ISR_ERR   13
ISR_ERR   14
ISR_NOERR 15
ISR_NOERR 16
ISR_NOERR 17
ISR_NOERR 18
ISR_NOERR 19
ISR_NOERR 20
ISR_NOERR 21
ISR_NOERR 22
ISR_NOERR 23
ISR_NOERR 24
ISR_NOERR 25
ISR_NOERR 26
ISR_NOERR 27
ISR_NOERR 28
ISR_NOERR 29
ISR_NOERR 30
ISR_NOERR 31

%macro IRQ 1
global irq_stub_%1
irq_stub_%1:
    push qword 0              ; err_code = 0
    push qword %1             ; int_no = vector
    PUSH_ALL
    mov rdi, rsp
    sub rsp, 8
    call isr_common_trampoline
    add rsp, 8
    POP_ALL
    add rsp, 16
    iretq
%endmacro

; PIC IRQs 32..47
IRQ 32
IRQ 33
IRQ 34
IRQ 35
IRQ 36
IRQ 37
IRQ 38
IRQ 39
IRQ 40
IRQ 41
IRQ 42
IRQ 43
IRQ 44
IRQ 45
IRQ 46
IRQ 47

; LAPIC timer and spurious
ISR_NOERR 240
ISR_NOERR 242
ISR_NOERR 255

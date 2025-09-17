bits 64
section .text

global __thread_trampoline
extern sched_yield
; Expected stack layout on entry (top to bottom):
; [arg][entry][ret=0]
__thread_trampoline:
    mov rdi, [rsp]       ; rdi = arg (first C argument)
    mov rax, [rsp+8]     ; rax = entry
    call rax
.exit:
    call sched_yield
    jmp .exit

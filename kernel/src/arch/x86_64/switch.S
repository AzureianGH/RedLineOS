.section .text
.global context_switch
.type context_switch, @function
# void context_switch(task_context_t* prev, task_context_t* next)
# Save callee-saved + rsp/rip, then jump to next->rip with next->rsp
# Offsets in task_context_t:
# 0  r15, 8 r14, 16 r13, 24 r12, 32 rbx, 40 rbp, 48 r11, 56 r10,
# 64 r9, 72 r8, 80 rax, 88 rcx, 96 rdx, 104 rsi, 112 rdi,
# 120 rsp, 128 rip, 136 rflags
context_switch:
    # rdi = prev, rsi = next
    movq %r15, 0(%rdi)
    movq %r14, 8(%rdi)
    movq %r13, 16(%rdi)
    movq %r12, 24(%rdi)
    movq %rbx, 32(%rdi)
    movq %rbp, 40(%rdi)
    movq %rsp, 120(%rdi)
    movq (%rsp), %rax        # return address from caller frame
    movq %rax, 128(%rdi)

    movq 120(%rsi), %rsp
    movq 0(%rsi), %r15
    movq 8(%rsi), %r14
    movq 16(%rsi), %r13
    movq 24(%rsi), %r12
    movq 32(%rsi), %rbx
    movq 40(%rsi), %rbp
    movq 128(%rsi), %rax
    jmp *%rax
.size context_switch, .-context_switch

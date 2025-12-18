#include <isr.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <stdatomic.h>
#include <panic.h>
#include <vheap.h>
#include <vmm.h>
#include <lprintf.h>
#include <symbols.h>
#include <smp.h>
#include <cpu_local.h>
#include <lapic.h>

#define MAX_HANDLERS 8

static isr_handler_t handlers[256][MAX_HANDLERS];

extern void smp_halt_others(void);

static atomic_flag panic_once = ATOMIC_FLAG_INIT;

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

static const char *reg64_name(uint8_t reg) {
    static const char *names[16] = {
        "rax","rcx","rdx","rbx","rsp","rbp","rsi","rdi",
        "r8","r9","r10","r11","r12","r13","r14","r15"
    };
    return names[reg & 0xF];
}

struct disasm_insn {
    const char *mnemonic;
    char op1[64];
    char op2[64];
    size_t len;
    int is_branch;
    uint64_t target;
};

static inline uint8_t modrm_reg_field(uint8_t modrm, uint8_t rex) {
    return (uint8_t)(((modrm >> 3) & 7) | ((rex & 4) ? 8 : 0));
}

static inline uint8_t modrm_rm_field(uint8_t modrm, uint8_t rex) {
    return (uint8_t)((modrm & 7) | ((rex & 1) ? 8 : 0));
}

static void format_mem_operand(char *out, size_t out_sz, uint8_t mod, uint8_t rm, uint8_t rex, const unsigned char *p, size_t *consumed) {
    *consumed = 0;
    uint8_t sib = 0;
    int has_sib = ((rm & 7) == 4);
    uint8_t base = (rm & 7) | ((rex & 1) ? 8 : 0);
    uint8_t index = 0;
    int scale = 1;
    int has_index = 0;

    if (has_sib) {
        sib = p[*consumed];
        (*consumed)++;
        base = (sib & 7) | ((rex & 1) ? 8 : 0);
        index = ((sib >> 3) & 7) | ((rex & 2) ? 8 : 0);
        scale = 1 << (sib >> 6);
        has_index = ((sib >> 3) & 7) != 4; // index 100b means no index
    }

    int disp = 0;
    if (mod == 1) {
        disp = (int8_t)p[*consumed];
        (*consumed) += 1;
    } else if (mod == 2 || (mod == 0 && !has_sib && (rm & 7) == 5)) {
        disp = (int32_t)*(const int32_t *)(p + *consumed);
        (*consumed) += 4;
    } else if (mod == 0 && has_sib && (sib & 7) == 5) {
        disp = (int32_t)*(const int32_t *)(p + *consumed);
        (*consumed) += 4;
        base = 0xFF; // no base register
    }

    size_t pos = 0;
    pos += snprintf(out + pos, out_sz - pos, "[");
    int first = 1;

    if (mod == 0 && !has_sib && (rm & 7) == 5) {
        pos += snprintf(out + pos, out_sz - pos, "rip");
        first = 0;
    } else if (base != 0xFF) {
        pos += snprintf(out + pos, out_sz - pos, "%s", reg64_name(base));
        first = 0;
    }

    if (has_index) {
        pos += snprintf(out + pos, out_sz - pos, "%s%s*%d", first ? "" : "+", reg64_name(index), scale);
        first = 0;
    }

    if (disp) {
        pos += snprintf(out + pos, out_sz - pos, "%s0x%x", (disp < 0) ? "-" : "+", (disp < 0) ? -disp : disp);
    } else if (first) {
        pos += snprintf(out + pos, out_sz - pos, "0");
    }

    snprintf(out + pos, out_sz - pos, "]");
}

static size_t disassemble_one(const unsigned char *p, uint64_t rip, struct disasm_insn *out) {
    out->mnemonic = "db";
    out->op1[0] = out->op2[0] = '\0';
    out->len = 0;
    out->is_branch = 0;
    out->target = 0;

    size_t idx = 0;
    uint8_t rex = 0;
    while (p[idx] >= 0x40 && p[idx] <= 0x4F) {
        rex = p[idx];
        idx++;
    }

    uint8_t op = p[idx++];

    snprintf(out->op1, sizeof(out->op1), "0x%02hhx", (unsigned long long)(op));

    switch (op) {
        case 0x90:
            out->mnemonic = "nop";
            out->len = idx;
            return out->len;
        case 0xC3:
            out->mnemonic = "ret";
            out->len = idx;
            return out->len;
        case 0xCC:
            out->mnemonic = "int3";
            out->len = idx;
            return out->len;
        case 0xCD: { // int imm8
            uint8_t imm = p[idx];
            out->mnemonic = "int";
            snprintf(out->op1, sizeof(out->op1), "0x%02x", imm);
            out->len = idx + 1;
            return out->len;
        }
        case 0xEB: { // short jmp rel8
            int8_t rel = (int8_t)p[idx];
            out->mnemonic = "jmp";
            snprintf(out->op1, sizeof(out->op1), "0x%016llx", (unsigned long long)(rip + idx + 1 + rel));
            out->len = idx + 1;
            out->is_branch = 1;
            out->target = rip + out->len + rel;
            return out->len;
        }
        case 0xE9: { // jmp rel32
            int32_t rel = *(const int32_t *)(p + idx);
            out->mnemonic = "jmp";
            snprintf(out->op1, sizeof(out->op1), "0x%016llx", (unsigned long long)(rip + idx + 4 + rel));
            out->len = idx + 4;
            out->is_branch = 1;
            out->target = rip + out->len + rel;
            return out->len;
        }
        case 0xE8: { // call rel32
            int32_t rel = *(const int32_t *)(p + idx);
            out->mnemonic = "call";
            snprintf(out->op1, sizeof(out->op1), "0x%016llx", (unsigned long long)(rip + idx + 4 + rel));
            out->len = idx + 4;
            out->is_branch = 1;
            out->target = rip + out->len + rel;
            return out->len;
        }
        case 0x99: { // cqo (sign-extend rax into rdx:rax)
            out->mnemonic = "cqo";
            out->len = idx;
            return out->len;
        }
        case 0xF4: { // hlt
            out->mnemonic = "hlt";
            out->len = idx;
            return out->len;
        }
            
    }

    if (op >= 0x70 && op <= 0x7F) { // short conditional branches
        static const char *conds[16] = {
            "jo","jno","jb","jae","je","jne","jbe","ja",
            "js","jns","jp","jnp","jl","jge","jle","jg"
        };
        int8_t rel = (int8_t)p[idx];
        out->mnemonic = conds[op - 0x70];
        snprintf(out->op1, sizeof(out->op1), "0x%016llx", (unsigned long long)(rip + idx + 1 + rel));
        out->len = idx + 1;
        out->is_branch = 1;
        out->target = rip + out->len + rel;
        return out->len;
    }

    if (op == 0x0F) { // two-byte opcodes
        uint8_t op2 = p[idx++];
        if (op2 == 0x05) {
            out->mnemonic = "syscall";
            out->len = idx;
            return out->len;
        }
        if (op2 == 0x0B) { // ud2
            out->mnemonic = "ud2";
            out->len = idx;
            return out->len;
        }
        if (op2 >= 0x80 && op2 <= 0x8F) { // jcc rel32
            static const char *conds[16] = {
                "jo","jno","jb","jae","je","jne","jbe","ja",
                "js","jns","jp","jnp","jl","jge","jle","jg"
            };
            int32_t rel = *(const int32_t *)(p + idx);
            out->mnemonic = conds[op2 - 0x80];
            snprintf(out->op1, sizeof(out->op1), "0x%016llx", (unsigned long long)(rip + idx + 4 + rel));
            out->len = idx + 4;
            out->is_branch = 1;
            out->target = rip + out->len + rel;
            return out->len;
        }
        // Generic two-byte opcode: note opcode byte and advance past op2
        snprintf(out->op1, sizeof(out->op1), "0x0f%02x", op2);
        out->mnemonic = "0f";
        out->len = idx;
        return out->len;
    }

    if (op >= 0x50 && op <= 0x57) { // push r64
        uint8_t reg = (op - 0x50) | ((rex & 1) ? 8 : 0);
        out->mnemonic = "push";
        snprintf(out->op1, sizeof(out->op1), "%s", reg64_name(reg));
        out->len = idx;
        return out->len;
    }

    if (op >= 0x58 && op <= 0x5F) { // pop r64
        uint8_t reg = (op - 0x58) | ((rex & 1) ? 8 : 0);
        out->mnemonic = "pop";
        snprintf(out->op1, sizeof(out->op1), "%s", reg64_name(reg));
        out->len = idx;
        return out->len;
    }

    if (op >= 0xB8 && op <= 0xBF) { // mov r64, imm
        uint8_t reg = (op - 0xB8) | ((rex & 1) ? 8 : 0);
        int w64 = (rex & 8) != 0;
        if (w64) {
            uint64_t imm = *(const uint64_t *)(p + idx);
            snprintf(out->op2, sizeof(out->op2), "0x%016llx", (unsigned long long)imm);
            out->len = idx + 8;
        } else {
            uint32_t imm = *(const uint32_t *)(p + idx);
            snprintf(out->op2, sizeof(out->op2), "0x%08x", imm);
            out->len = idx + 4;
        }
        out->mnemonic = "mov";
        snprintf(out->op1, sizeof(out->op1), "%s", reg64_name(reg));
        return out->len;
    }

    // Instructions that need ModRM
    switch (op) {
        case 0x89: // mov r/m64, r64
        case 0x8B: // mov r64, r/m64
        case 0x8D: // lea r64, m
        case 0x81: // imm32 group
        case 0x83: // imm8 group
        case 0x85: // test r/m64, r64
        case 0x31: // xor r/m64, r64
        case 0xFF: // inc/dec/call/jmp r/m64
        case 0xF7: // group 3 (test/not/neg/mul/imul/div/idiv)
        case 0xC7: // mov r/m64, imm32
            break;
        default:
            out->len = idx;
            return out->len; // unknown; treat as byte data
    }

    uint8_t modrm = p[idx++];
    uint8_t mod = (modrm >> 6) & 3;
    uint8_t rm = modrm_rm_field(modrm, rex);
    uint8_t reg = modrm_reg_field(modrm, rex);
    char rm_buf[64];
    size_t modrm_consumed = 0;

    if (mod != 3) {
        format_mem_operand(rm_buf, sizeof(rm_buf), mod, modrm & 7, rex, p + idx, &modrm_consumed);
    } else {
        snprintf(rm_buf, sizeof(rm_buf), "%s", reg64_name(rm));
    }

    switch (op) {
        case 0x89: // mov r/m64, r64
            out->mnemonic = "mov";
            snprintf(out->op1, sizeof(out->op1), "%s", rm_buf);
            snprintf(out->op2, sizeof(out->op2), "%s", reg64_name(reg));
            out->len = idx + modrm_consumed;
            return out->len;
        case 0x8B: // mov r64, r/m64
            out->mnemonic = "mov";
            snprintf(out->op1, sizeof(out->op1), "%s", reg64_name(reg));
            snprintf(out->op2, sizeof(out->op2), "%s", rm_buf);
            out->len = idx + modrm_consumed;
            return out->len;
        case 0x8D: // lea r64, m
            out->mnemonic = "lea";
            snprintf(out->op1, sizeof(out->op1), "%s", reg64_name(reg));
            snprintf(out->op2, sizeof(out->op2), "%s", rm_buf);
            out->len = idx + modrm_consumed;
            return out->len;
        case 0x85: // test r/m64, r64
            out->mnemonic = "test";
            snprintf(out->op1, sizeof(out->op1), "%s", rm_buf);
            snprintf(out->op2, sizeof(out->op2), "%s", reg64_name(reg));
            out->len = idx + modrm_consumed;
            return out->len;
        case 0x31: // xor r/m64, r64
            out->mnemonic = "xor";
            snprintf(out->op1, sizeof(out->op1), "%s", rm_buf);
            snprintf(out->op2, sizeof(out->op2), "%s", reg64_name(reg));
            out->len = idx + modrm_consumed;
            return out->len;
        case 0x81: { // imm32 group
            uint32_t imm = *(const uint32_t *)(p + idx + modrm_consumed);
            int subop = (modrm >> 3) & 7;
            const char *mn = NULL;
            if (subop == 5) mn = "sub";
            else if (subop == 7) mn = "cmp";
            else if (subop == 0) mn = "add";
            if (mn) {
                out->mnemonic = mn;
                snprintf(out->op1, sizeof(out->op1), "%s", rm_buf);
                snprintf(out->op2, sizeof(out->op2), "0x%08x", imm);
                out->len = idx + modrm_consumed + 4;
                return out->len;
            }
            out->len = idx + modrm_consumed + 4;
            return out->len;
        }
        case 0x83: { // imm8 group
            uint8_t imm = p[idx + modrm_consumed];
            int subop = (modrm >> 3) & 7;
            const char *mn = NULL;
            if (subop == 5) mn = "sub";
            else if (subop == 7) mn = "cmp";
            else if (subop == 0) mn = "add";
            if (mn) {
                out->mnemonic = mn;
                snprintf(out->op1, sizeof(out->op1), "%s", rm_buf);
                snprintf(out->op2, sizeof(out->op2), "0x%02x", imm);
                out->len = idx + modrm_consumed + 1;
                return out->len;
            }
            out->len = idx + modrm_consumed + 1;
            return out->len;
        }
        case 0xC7: { // mov r/m64, imm32
            uint32_t imm = *(const uint32_t *)(p + idx + modrm_consumed);
            out->mnemonic = "mov";
            snprintf(out->op1, sizeof(out->op1), "%s", rm_buf);
            snprintf(out->op2, sizeof(out->op2), "0x%08x", imm);
            out->len = idx + modrm_consumed + 4;
            return out->len;
        }
        case 0xFF: { // group 5
            int subop = (modrm >> 3) & 7;
            if (subop == 2) { // call r/m64
                out->mnemonic = "call";
                snprintf(out->op1, sizeof(out->op1), "%s", rm_buf);
                out->len = idx + modrm_consumed;
                return out->len;
            } else if (subop == 4) { // jmp r/m64
                out->mnemonic = "jmp";
                snprintf(out->op1, sizeof(out->op1), "%s", rm_buf);
                out->len = idx + modrm_consumed;
                return out->len;
            } else if (subop == 6) { // push r/m64
                out->mnemonic = "push";
                snprintf(out->op1, sizeof(out->op1), "%s", rm_buf);
                out->len = idx + modrm_consumed;
                return out->len;
            }
            out->len = idx + modrm_consumed;
            return out->len;
        }
        case 0xF7: { // group 3
            int subop = (modrm >> 3) & 7;
            if (subop == 0) { // test r/m64, imm32
                uint32_t imm = *(const uint32_t *)(p + idx + modrm_consumed);
                out->mnemonic = "test";
                snprintf(out->op1, sizeof(out->op1), "%s", rm_buf);
                snprintf(out->op2, sizeof(out->op2), "0x%08x", imm);
                out->len = idx + modrm_consumed + 4;
                return out->len;
            } else if (subop == 2) { // not r/m64
                out->mnemonic = "not";
                snprintf(out->op1, sizeof(out->op1), "%s", rm_buf);
            } else if (subop == 3) { // neg r/m64
                out->mnemonic = "neg";
                snprintf(out->op1, sizeof(out->op1), "%s", rm_buf);
            } else if (subop == 4) { // mul r/m64
                out->mnemonic = "mul";
                snprintf(out->op1, sizeof(out->op1), "%s", rm_buf);
            } else if (subop == 5) { // imul r/m64
                out->mnemonic = "imul";
                snprintf(out->op1, sizeof(out->op1), "%s", rm_buf);
            } else if (subop == 6) { // div r/m64
                out->mnemonic = "div";
                snprintf(out->op1, sizeof(out->op1), "%s", rm_buf);
            } else if (subop == 7) { // idiv r/m64
                out->mnemonic = "idiv";
                snprintf(out->op1, sizeof(out->op1), "%s", rm_buf);
            }
            out->len = idx + modrm_consumed;
            return out->len;
        }
    }

    out->len = idx + modrm_consumed;
    return out->len;
}

static void panic_disassemble(uint64_t rip) {
    printf("Disassembly near RIP:\n");
    const size_t pre_bytes = 8;
    uint64_t start = (rip > pre_bytes) ? (rip - pre_bytes) : rip;
    const unsigned char *p = (const unsigned char *)start;
    size_t offset = 0;
    for (int i = 0; i < 10; ++i) {
        struct disasm_insn insn;
        uint64_t cur_addr = start + offset;
        size_t len = disassemble_one(p + offset, cur_addr, &insn);
        if (len == 0) {
            len = 1;
        }
        int is_faulting = ((cur_addr + len == rip) && (!strncmp(insn.mnemonic, "int", 3))) || (cur_addr == rip);
        printf(" 0x%016llx: %-7s", (unsigned long long)cur_addr, insn.mnemonic);
        if (insn.op1[0]) {
            printf(" %s", insn.op1);
            if (insn.op2[0]) {
                printf(", %s", insn.op2);
            }
        }
        printf(" ; len=%zu", len);
        if (is_faulting) {
            printf(" <FAULTING>");
        }
        if (cur_addr == rip) {
            printf(" <RIP>");
        }
        if (insn.is_branch) {
            printf(" -> 0x%016llx", (unsigned long long)insn.target);
            const struct ksym *tsym = symbol_lookup((uintptr_t)insn.target);
            if (tsym) {
                unsigned long long base = (unsigned long long)(tsym->addr + symbols_get_slide());
                unsigned long long off = (insn.target >= base) ? (unsigned long long)(insn.target - base) : 0ULL;
                if (off == 0) {
                    printf(" <%s>", tsym->name);
                } else {
                    printf(" <%s+0x%llx>", tsym->name, off);
                }
            }
        }
        printf("\n");
        int is_term = 0;
        if (!strncmp(insn.mnemonic, "ret", 4) || !strncmp(insn.mnemonic, "int3", 5) || !strncmp(insn.mnemonic, "int", 4) || !strncmp(insn.mnemonic, "iretq", 6) || !strncmp(insn.mnemonic, "hlt", 4)) {
            is_term = 1;
        }
        if (is_term && cur_addr >= rip) {
            break; // stop after showing the faulting/terminal instruction
        }
        offset += len;
        if (offset >= 64 || cur_addr > rip + 32) {
            break;
        }
    }
}

static void panic_backtrace(const isr_frame_t* f) {
    printf("Backtrace (most recent call first):\n");
    uint64_t rbp = f->rbp;
    uint64_t rip = f->rip;

    panic_disassemble(rip);

    for (int depth = 0; depth < 16 && rip; ++depth) {
        const struct ksym *sym = symbol_lookup((uintptr_t)rip);
        if (sym) {
            printf(" #%d RIP=0x%016llx <%s+0x%llx> RBP=0x%016llx\n",
                   depth,
                   (unsigned long long)rip,
                   sym->name,
                   (unsigned long long)((uintptr_t)rip - (sym->addr + symbols_get_slide())),
                   (unsigned long long)rbp);
        } else {
            printf(" #%d RIP=0x%016llx RBP=0x%016llx\n",
                   depth,
                   (unsigned long long)rip,
                   (unsigned long long)rbp);
        }

        if (rbp < 0x1000 || (rbp & 7)) {
            break; // likely invalid frame pointer
        }

        uint64_t *frame = (uint64_t *)rbp;
        uint64_t next_rbp = frame[0];
        uint64_t next_rip = frame[1];

        if (next_rbp <= rbp || next_rip == 0) {
            break; // avoid infinite loops or bogus frames
        }

        rbp = next_rbp;
        rip = next_rip;
    }
}

void kernel_panic(const char* reason, const isr_frame_t* f) {
    __asm__ __volatile__("cli");
    if (atomic_flag_test_and_set_explicit(&panic_once, memory_order_acq_rel)) {
        for (;;) { __asm__ __volatile__("cli; hlt"); }
    }

    printf("\n===== KERNEL PANIC =====\n");
    printf("Reason: %s\n\n", reason);

    smp_halt_others();
        printf("[panic] other CPUs halted\n");

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

    panic_backtrace(f);

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

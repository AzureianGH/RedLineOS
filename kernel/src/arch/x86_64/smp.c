#include <smp.h>
#include <limine.h>
#include <stddef.h>
#include <stdint.h>
#include <lprintf.h>
#include <lapic.h>

extern volatile struct LIMINE_MP(request) mp_request;

// Simple CPU table
static cpu_info_t cpus[64];
static uint32_t cpu_count;
static uint32_t bsp_lapic;

static inline uint32_t x86_lapic_id_read(void) {
    // LAPIC ID register is at 0x20; our lapic.c maps MMIO and has a helper to read SVR only,
    // but we can use CPUID leaf 1 EBX[31:24] as APIC ID on x86_64.
    uint32_t eax, ebx, ecx, edx;
    __asm__ __volatile__("cpuid" : "=a"(eax), "=b"(ebx), "=c"(ecx), "=d"(edx) : "a"(1), "c"(0));
    (void)eax; (void)ecx; (void)edx;
    return (ebx >> 24) & 0xFFu;
}

// AP entry trampoline called by Limine for each AP
static void ap_entry(struct LIMINE_MP(info) *info) {
    (void)info;
    // Minimal enable
    lapic_enable();
    uint32_t apic = x86_lapic_id_read();
    info_printf("AP online: LAPIC %u\n", (unsigned)apic);
    // TODO: jump into scheduler idle loop when implemented
    for (;;) { __asm__ __volatile__("hlt"); }
}

bool smp_init(void) {
    if (!mp_request.response) { info_printf("SMP: Limine MP response missing; assuming 1 CPU\n"); cpu_count = 1; return true; }
    struct LIMINE_MP(response) *resp = mp_request.response;
    cpu_count = (uint32_t)resp->cpu_count;
    if (cpu_count > 64) cpu_count = 64;
    bsp_lapic = resp->bsp_lapic_id;
    info_printf("SMP: CPUs=%u BSP LAPIC=%u\n", (unsigned)cpu_count, (unsigned)bsp_lapic);

    for (uint32_t i = 0; i < cpu_count; ++i) {
        struct LIMINE_MP(info) *ci = resp->cpus[i];
        cpus[i].lapic_id = ci->lapic_id;
        cpus[i].index = i;
        cpus[i].stack_top = NULL;
        // Set AP entry for non-BSP
        if (ci->lapic_id != bsp_lapic) {
            ci->goto_address = ap_entry;
            ci->extra_argument = 0;
        }
    }
    return true;
}

uint32_t smp_cpu_count(void) { return cpu_count ? cpu_count : 1; }
uint32_t smp_bsp_lapic_id(void) { return bsp_lapic; }

const cpu_info_t* smp_get_cpu(uint32_t idx) { return (idx < cpu_count) ? &cpus[idx] : NULL; }

uint32_t smp_this_cpu_index(void) {
    uint32_t lapic = x86_lapic_id_read();
    for (uint32_t i = 0; i < cpu_count; ++i) if (cpus[i].lapic_id == lapic) return i;
    return 0;
}

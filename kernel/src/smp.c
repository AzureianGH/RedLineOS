#include <smp.h>
#include <limine.h>
#include <lprintf.h>
#include <palloc.h>
#include <vheap.h>
#include <gdt.h>
#include <idt.h>
#include <lapic.h>
#include <stdatomic.h>
#include <stddef.h>
#include <stdint.h>
#include <cpu_local.h>

extern volatile struct LIMINE_MP(request) mp_request;

#define AP_STACK_PAGES 16ULL // 64 KiB per AP
#define AP_STACK_GUARD 1ULL
#define PAGE_SIZE 0x1000ULL

static _Atomic uint32_t g_cpu_online = 1; // BSP counts as online
static uint32_t g_cpu_total = 1;

struct ap_bootstrap {
    uint64_t stack_base;
    uint64_t stack_size;
    uint32_t cpu_index;
};

static void ap_idle(void) {
    for (;;) { __asm__ __volatile__("hlt"); }
}

static void enable_sse_on_this_cpu(void) {
    uint64_t cr4;
    __asm__ __volatile__("mov %%cr4, %0" : "=r"(cr4));
    cr4 |= (1ULL << 9) | (1ULL << 10); // OSFXSR | OSXMMEXCPT
    __asm__ __volatile__("mov %0, %%cr4" :: "r"(cr4) : "memory");
}

__attribute__((target("no-sse")))
static void smp_ap_entry(struct LIMINE_MP(info) *info) {
    struct ap_bootstrap *boot = (struct ap_bootstrap *)(uintptr_t)info->extra_argument;
    uint64_t stack_base = 0;
    uint64_t stack_size = 0;
    uint32_t cpu_index = 0;
    if (boot) {
        stack_base = boot->stack_base;
        stack_size = boot->stack_size;
        cpu_index = boot->cpu_index;
    }
    uint64_t stack_top = stack_base + stack_size;
    if (stack_top) {
        __asm__ __volatile__("mov %0, %%rsp" :: "r"(stack_top));
    }
    cpu_local_t local = {
        .cpu_index = cpu_index,
        .lapic_id = info->lapic_id,
    };
    cpu_local_set(&local);
    enable_sse_on_this_cpu();
    // Ensure per-AP descriptor tables are loaded before enabling interrupts.
    gdt_init(cpu_index);
    idt_init();

    idt_enable_interrupts();
    lapic_enable();
    info_printf("smp: AP lapic %u online (cpu_index=%u)\n", info->lapic_id, cpu_index);
    atomic_fetch_add_explicit(&g_cpu_online, 1, memory_order_relaxed);
    ap_idle();
}

void smp_init(uint64_t tsc_hz_hint) {
    (void)tsc_hz_hint;
    struct LIMINE_MP(response) *resp = mp_request.response;
    if (!resp || resp->cpu_count <= 1) {
        info_printf("smp: single CPU (no APs)\n");
        g_cpu_total = 1;
        return;
    }
    g_cpu_total = (uint32_t)resp->cpu_count;
    static cpu_local_t bsp_local;
    bsp_local.cpu_index = 0;
    bsp_local.lapic_id = resp->bsp_lapic_id;
    cpu_local_set(&bsp_local);
    info_printf("smp: cpus=%u bsp_lapic=%u flags=%#x\n",
                g_cpu_total, resp->bsp_lapic_id, resp->flags);
    for (uint64_t i = 0; i < resp->cpu_count; ++i) {
        struct LIMINE_MP(info) *cpu = resp->cpus[i];
        if (!cpu) continue;
        if (cpu->lapic_id == resp->bsp_lapic_id) continue; // BSP already running
        // Allocate stack with guard
        size_t pages = AP_STACK_PAGES + AP_STACK_GUARD;
        size_t bytes = pages * PAGE_SIZE;
        uint64_t base = vheap_commit(bytes);
        if (!base) {
            error_printf("smp: failed to allocate AP stack lapic=%u\n", cpu->lapic_id);
            continue;
        }
        // Leave one guard page unmapped by simply not using it; we start usable after guard
        uint64_t usable_base = base + (AP_STACK_GUARD * PAGE_SIZE);
        struct ap_bootstrap *boot = (struct ap_bootstrap *)usable_base;
        boot->stack_base = usable_base + PAGE_SIZE; // avoid clobbering bootstrap struct
        boot->stack_size = (AP_STACK_PAGES * PAGE_SIZE) - PAGE_SIZE;
        boot->cpu_index = (uint32_t)i;
        uint64_t top = boot->stack_base + boot->stack_size;
        top &= ~0xFULL;
        cpu->extra_argument = (uint64_t)(uintptr_t)boot;
        cpu->goto_address = smp_ap_entry;
        info_printf("smp: queued AP lapic=%u stack=%#llx-%#llx\n",
                    cpu->lapic_id,
                    (unsigned long long)boot->stack_base,
                    (unsigned long long)(boot->stack_base + boot->stack_size));
    }
    info_printf("smp: waiting for APs...\n");
}

uint32_t smp_cpu_count(void) {
    return g_cpu_total;
}

void smp_wait_all_aps(void) {
    while (atomic_load_explicit(&g_cpu_online, memory_order_relaxed) < g_cpu_total) {
        __asm__ __volatile__("pause");
    }
}

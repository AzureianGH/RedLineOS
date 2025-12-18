// Core kernel entry; boot/ and libc/ pieces are split out for modularity.
// Keep main.c focused on high-level init and control flow.
#include <displaystandard.h>
#include <extendedint.h>
#include <stelloc.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdlib.h>
#include <stdint.h>
#include <limine.h>
#include <stdio.h>
#include <boot.h>
#include <cpu_local.h>
#include <fb.h>
#include <libcinternal/libioP.h>
#include <palloc.h>
#include <lprintf.h>
#include <gdt.h>
#include <idt.h>
#include <isr.h>
#include <pic.h>
#include <pit.h>
#include <rtc.h>
#include <tsc.h>
#include <acpi.h>
#include <timer.h>
#include <lapic.h>
#include <ioapic.h>
#include <hpet.h>
#include <vmm.h>
#include <time.h>
#include <timebase.h>
#include <unistd.h>
#include <stdatomic.h>
#include <symbols.h>
#include <alloc_debug.h>
#include <sched.h>
#include <smp.h>


// Halt and catch fire function.
static void hcf(void) {
    for (;;) {
        asm ("hlt");
    }
}

// Example shared atomic time variable
static _Atomic uint64_t shared_time;

static uint64_t monotonic_ms(void) {
    return timebase_monotonic_ns() / 1000000ULL;
}

static bool check_bytes(const uint8_t *p, size_t n, uint8_t val) {
    for (size_t i = 0; i < n; i++) {
        if (p[i] != val) return false;
    }
    return true;
}

static uint64_t task0_tick = 0;
static uint64_t task1_tick = 0;
static uint64_t task2_tick = 0;
static uint64_t task3_tick = 0;
static uint64_t task4_tick = 0;
static uint64_t task5_tick = 0;

static void demo_task(void *arg) {
    uint64_t task_num = (uint64_t)arg;
    for (;;) {
        if (task_num == 0) {
            task0_tick++;
            usleep(1000); // 1 ms
        } else if (task_num == 1) {
            task1_tick++;
            usleep(200000); // 200 ms
        } else if (task_num == 2) {
            task2_tick++;
            usleep(400000); // 400 ms
        } else if (task_num == 3) {
            task3_tick++;
            usleep(800000); // 800 ms
        } else if (task_num == 4) {
            task4_tick++;
            usleep(1000000); // 1.0 s
        } else if (task_num == 5) {
            task5_tick++;
            usleep(3000000); // 3.0 s
        }
    }
}

static void task_print(void* arg)
{
    (void)arg;
    for (;;) {
        printf("\rTask0 ticks: %llu | Task1 ticks: %llu | Task2 ticks: %llu | Task3 ticks: %llu | Task4 ticks: %llu | Task5 ticks: %llu      ",
            (unsigned long long)task0_tick,
            (unsigned long long)task1_tick,
            (unsigned long long)task2_tick,
            (unsigned long long)task3_tick,
            (unsigned long long)task4_tick,
            (unsigned long long)task5_tick);
        usleep(100); // 100 us
    }
}


static void halt_if(bool condition, const char *message) {
    if (condition) {
        if (message != NULL) {
            error_printf("%s\n", message);
        }
        hcf();
    }
}

static void verify_boot_requests(void) {
    info_printf("Verifying bootloader-provided structures...\n");
    bool invalid =
        LIMINE_BASE_REVISION_SUPPORTED == false             ||
        framebuffer_request.response == NULL                ||
        framebuffer_request.response->framebuffer_count < 1 ||
        hhdm_request.response == NULL                       ||
        hhdm_request.response->offset == 0                  ||
        memmap_request.response == NULL                     ||
        memmap_request.response->entry_count == 0           ||
        executable_address_request.response == NULL;

    halt_if(invalid, "Bootloader did not supply required data; halting.");
    success_printf("Bootloader structures verified (fb count=%llu, memmap entries=%llu, hhdm=0x%llx).\n",
        (unsigned long long)framebuffer_request.response->framebuffer_count,
        (unsigned long long)memmap_request.response->entry_count,
        (unsigned long long)hhdm_request.response->offset);
}

static void init_descriptor_tables(void) {
    info_printf("Initializing descriptor tables (GDT/IDT)...\n");
    gdt_init(0); // BSP is CPU index 0
    idt_init();
    exceptions_install_defaults();
    success_printf("Descriptor tables installed.\n");
}

static void init_display(void) {
    struct limine_framebuffer *lfb = framebuffer_request.response->framebuffers[0];
    info_printf("Initializing display: %ux%u %u bpp (pitch=%u)\n",
        lfb->width, lfb->height, lfb->bpp, lfb->pitch);
    displaystandard_init(&(struct fb){ .lfb = framebuffer_request.response->framebuffers[0] });
    set_debug_enabled(false);
    success_printf("Display initialized successfully.\n");
}

static void init_palloc(void) {
    info_printf("Initializing page allocator (palloc) with %llu memmap entries...\n",
        (unsigned long long)memmap_request.response->entry_count);
    palloc_init(memmap_request.response);
    success_printf("Page allocator initialized with %zu pages free.\n", palloc_get_free_page_count());
}

static void init_heap(void) {
    info_printf("Initializing stelloc heap allocator...\n");
    stelloc_init_heap();

    // Basic allocator self-test (slab + stelloc) with poisoning check.

    uintptr_t stack_check = 0;

    uint8_t *a = malloc(24);      // slab-sized
    stack_check = (uintptr_t)a;
    uint8_t *b = malloc(64);      // slab-sized
    uint8_t *c = malloc(2048);    // stelloc-sized (above SLAB_MAX_SIZE)
    halt_if(!a || !b || !c, "Heap allocator failed to return memory.");

#if ALLOC_DEBUG
    halt_if(!check_bytes(a, 24, ALLOC_POISON_ALLOC), "Slab alloc not poisoned (a)");
    halt_if(!check_bytes(b, 64, ALLOC_POISON_ALLOC), "Slab alloc not poisoned (b)");
    halt_if(!check_bytes(c, 64, ALLOC_POISON_ALLOC), "Stelloc alloc not poisoned (c head)");
#endif

    for (int i = 0; i < 24; i++) a[i] = 0xAB;
    for (int i = 0; i < 64; i++) b[i] = 0xBC;
    for (int i = 0; i < 64; i++) c[i] = 0xCD;

    free(a);
    free(b);
    free(c);

    // Ensure we can reallocate after frees.
    uint8_t *r = malloc(24);
    // Check that we got the same slab back (likely).
    halt_if((uintptr_t)r != stack_check, "Heap allocator returned different slab address after free.");
    halt_if(!r, "Heap allocator failed to realloc after free.");
    free(r);

    success_printf("Heap allocator self-test passed.\n");
}

static void init_pic(void) {
    info_printf("Remapping PIC...\n");
    pic_remap();
    pic_set_mask(0); // mask PIT
    pic_set_mask(1); // mask keyboard
    pic_set_mask(2); // cascade
    pic_set_mask(8); // mask RTC
    success_printf("PIC remapped and IRQs masked.\n");
}

static uint64_t calibrate_tsc(void) {
    info_printf("Calibrating TSC...\n");
    uint64_t tsc_hz = tsc_calibrate_hz(1193182u, 10);
    info_printf("TSC ~ %llu Hz\n", (unsigned long long)tsc_hz);
    return tsc_hz;
}

static void init_timers(uint64_t tsc_hz) {
    info_printf("Initializing timers (LAPIC/HPET/PIT fallback)...\n");
    bool ok = timer_init(1000, tsc_hz);
    halt_if(!ok, "Timer initialization failed.");

    switch (timer_source()) {
        case TIMER_SRC_LAPIC: success_printf("Using LAPIC timer at ~%u Hz\n", 1000u); break;
        case TIMER_SRC_HPET:  success_printf("Using HPET at ~%u Hz (periodic IRQ)\n", 1000u); break;
        case TIMER_SRC_PIT:   success_printf("Using PIT at 1000 Hz (legacy)\n"); break;
    }
}

static void init_environment(void) {
    info_printf("Initializing Environment...\n");

    int env_rc = setenv("TZ", "EST5EDT,M3.2.0/2,M11.1.0/2", 1);
    halt_if(env_rc != 0, "Failed to set TZ environment variable.");
    success_printf("Environment initialized (TZ set).\n");
}

static void seed_shared_time(void) {
    time_t now = time(NULL);
    shared_time = now;
    printf("Current time: %s", ctime(&now));
    success_printf("Time seeded for shared clock.\n");
}

extern void kmain(void);

extern char _kernel_link_base;

static void configure_symbol_slide(void) {
    uint64_t link_base = (uint64_t)(uintptr_t)&_kernel_link_base;
    uint64_t actual_base = executable_address_request.response ? executable_address_request.response->virtual_base : 0;
    uint64_t slide = 0;

    if (actual_base != 0 && actual_base != link_base) {
        slide = actual_base - link_base;
    }

    symbols_set_slide(slide);

    const struct ksym *probe = symbol_lookup((uintptr_t)&kmain);
    uintptr_t expected = (uintptr_t)&kmain - slide;
    bool valid = (probe != NULL) && (probe->addr == expected);

    if (!valid) {
        slide = 0;
        symbols_set_slide(slide);
        probe = symbol_lookup((uintptr_t)&kmain);
    }
}

extern void kmain(void) {
    info_printf("=== Kernel startup begin ===\n");
    verify_boot_requests();
    configure_symbol_slide();

    init_descriptor_tables();
    init_display();

    info_printf("Running log ring self-test...\n");
    int log_rc = log_ring_self_test();
    halt_if(log_rc != 0, "Log ring self-test failed.");
    success_printf("Log ring self-test passed.\n");

    init_palloc();
    init_heap();

    init_pic();
    uint64_t tsc_hz = calibrate_tsc();

    info_printf("Initializing ACPI...\n");
    acpi_init();

    timebase_init(tsc_hz);

    init_timers(tsc_hz);

    smp_init(tsc_hz);

    scheduler_init(timer_hz(), tsc_hz);

    // Defer enabling interrupts until after IDT, exception handlers, and timers are configured.
    idt_enable_interrupts();
    success_printf("Interrupts enabled.\n");

    init_environment();
    seed_shared_time();

    smp_wait_all_aps();
    scheduler_start();

    success_printf("Kernel initialization complete.\n");
    info_printf("=== Kernel startup end ===\n");

    task_create("demoA", demo_task, (void*)0, 4);
    task_create("demoB", demo_task, (void*)1, 4);
    task_create("demoC", demo_task, (void*)2, 4);
    task_create("demoD", demo_task, (void*)3, 4);
    task_create("demoE", demo_task, (void*)4, 4);
    task_create("demoF", demo_task, (void*)5, 4);
    task_create("printer", task_print, NULL, 4);
    
    while (true) { __asm__ __volatile__("hlt"); }
}

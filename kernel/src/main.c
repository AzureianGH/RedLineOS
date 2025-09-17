// Core kernel entry; boot/ and libc/ pieces are split out for modularity.
// Keep main.c focused on high-level init and control flow.
#include <displaystandard.h>
#include <extendedint.h>
#include <stelloc.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdlib.h>
#include <limine.h>
#include <stdio.h>
#include <boot.h>
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
#include <smp.h>
#include <sched.h>
#include <unistd.h>
#include <stdatomic.h>

// Externs for environment
extern int init_env();

// Halt and catch fire function.
static void hcf(void) {
    for (;;) {
        asm ("hlt");
    }
}

// Example shared atomic time variable and worker threads
static _Atomic time_t shared_time;

static void process_a(void* arg) {
    (void)arg;
    for (;;) {
        time_t now = time(NULL);
        atomic_store(&shared_time, now);
        sched_yield();
    }
}

static void process_b(void* arg) {
    (void)arg;
    for (;;) {
        time_t rt = atomic_load(&shared_time);
        struct tm local = *localtime(&rt);
        printf("Local time: %s", asctime(&local));
        sleep(5);
    }
}

extern void kmain(void) {
    // Ensure the bootloader actually understands our base revision (see spec).
    if (LIMINE_BASE_REVISION_SUPPORTED == false             ||
        framebuffer_request.response == NULL                ||
        framebuffer_request.response->framebuffer_count < 1 ||
        hhdm_request.response == NULL                       ||
        hhdm_request.response->offset == 0                  ||
        memmap_request.response == NULL                     ||
        memmap_request.response->entry_count == 0
    ) {
        hcf();
    }


    gdt_init();
    idt_init();
    exceptions_install_defaults();

    displaystandard_init(&(struct fb){ .lfb = framebuffer_request.response->framebuffers[0] });
    set_debug_enabled(false);
    success_printf("Display initialized successfully.\n");
    // Initialize physical page allocator early, before ACPI/vmm ioremap needs page tables
    info_printf("Initializing page allocator (palloc)...\n");
    palloc_init(memmap_request.response);
    success_printf("Page allocator initialized with %zu pages free.\n", palloc_get_free_page_count());

    info_printf("Initializing stelloc heap allocator...\n");
    stelloc_init_heap();

    byte* stelloc_test = (byte*)malloc(1);
    // Dereference ptr
    *stelloc_test = 0xAF;
    if (*stelloc_test != 0xAF) {
        error_printf("Stelloc heap allocator self-test failed!\n");
        hcf();
    }
    free(stelloc_test);

    info_printf("Remapping PIC...\n");
    pic_remap();
    pic_set_mask(0); // mask PIT
    pic_set_mask(1); // mask keyboard
    pic_set_mask(2); // cascade
    pic_set_mask(8); // mask RTC
    success_printf("PIC remapped and IRQs masked.\n");

    info_printf("Calibrating TSC...\n");
    uint64_t tsc_hz = tsc_calibrate_hz(1193182u, 10);
    info_printf("TSC ~ %llu Hz\n", (unsigned long long)tsc_hz);

    info_printf("Initializing ACPI...\n");
    acpi_init();
    
    info_printf("Initializing timers (LAPIC/HPET/PIT fallback)...\n");
    if (timer_init(1000, tsc_hz)) {
        switch (timer_source()) {
            case TIMER_SRC_LAPIC: success_printf("Using LAPIC timer at ~%u Hz\n", 1000u); break;
            case TIMER_SRC_HPET:  success_printf("Using HPET at ~%u Hz (periodic IRQ)\n", 1000u); break;
            case TIMER_SRC_PIT:   success_printf("Using PIT at 1000 Hz (legacy)\n"); break;
        }
    }

    info_printf("Initializing SMP...\n");
    smp_init();
    
    info_printf("Initializing scheduler...\n");
    sched_init();
    ktime_init(1000, tsc_hz);

    info_printf("Initializing Environment...\n");
    init_env();

    if (setenv("TZ", "EST5EDT,M3.2.0/2,M11.1.0/2", 1) != 0) {
        error_printf("Failed to set TZ environment variable\n");
        hcf();
    }

    time_t t = time(NULL);

    // Initialize shared clock value
    shared_time = t;

    success_printf("Stelloc heap allocator initialized.\n");
    success_printf("Kernel initialization complete.\n");

    
    
    // Create two kernel threads
    sched_create(process_a, NULL, "procA");
    sched_create(process_b, NULL, "procB");

    // Enter scheduler on BSP
    sched_start();
}

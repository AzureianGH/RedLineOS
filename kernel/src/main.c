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

// Halt and catch fire function.
static void hcf(void) {
    for (;;) {
        asm ("hlt");
    }
}

// Simple LAPIC/PIT timer demo: count ticks and print uptime with milliseconds.
static volatile unsigned long long demo_ticks = 0;
static void demo_tick_cb(void) {
    ++demo_ticks;
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

    vmm_init();

    pic_remap();
    pic_set_mask(0); // mask PIT
    pic_set_mask(1); // mask keyboard
    pic_set_mask(2); // cascade
    pic_set_mask(8); // mask RTC

    info_printf("Calibrating TSC...\n");
    uint64_t tsc_hz = tsc_calibrate_hz(1193182u, 10);
    info_printf("TSC ~ %llu Hz\n", (unsigned long long)tsc_hz);

    info_printf("Initializing ACPI...\n");
    acpi_init();
    
    info_printf("Initializing timers (LAPIC/HPET/PIT fallback)...\n");
    if (timer_init(1000, tsc_hz)) {
        switch (timer_source()) {
            case TIMER_SRC_LAPIC: success_printf("Using LAPIC timer at ~%u Hz\n", 1000u); break;
            case TIMER_SRC_HPET:  success_printf("Using HPET for timing (no IRQ tick yet)\n"); break;
            case TIMER_SRC_PIT:   success_printf("Using PIT at 1000 Hz (legacy)\n"); break;
        }
    }
    // After LAPIC timer is active and PIC lines masked, wire HPET IRQ to IOAPIC/LAPIC
    if (hpet_supported() && ioapic_supported()) {
        if (hpet_enable_and_route_irq(0, 1000000ULL, 242)) {
            success_printf("HPET periodic IRQ routed (1kHz, vector 242)\n");
        } else {
            info_printf("HPET IRQ routing skipped\n");
        }
    }

    info_printf("Initializing stelloc heap allocator...\n");
    stelloc_init_heap();
    success_printf("Stelloc heap allocator initialized.\n");
    success_printf("Kernel initialization complete.\n");
    hcf();
}
